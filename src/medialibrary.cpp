// SPDX-License-Identifier: GPL-2.0-or-later
#include "apikeys.h"
#include "mpcplayer.h"
#include "medialibrary.h"
#include "securestore.h"
#include "appmodel.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>

namespace {

const QSet<QString> videoExtensions = {
    QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("m4v"), QStringLiteral("avi"),
    QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("ts"), QStringLiteral("m2ts"),
    QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("webm"), QStringLiteral("flv"),
    QStringLiteral("divx"), QStringLiteral("iso"), QStringLiteral("vob"), QStringLiteral("ogm"),
};

// Standaardregels van Kodi (xbmc/settings/AdvancedSettings.cpp: cleandatetime en
// cleanstrings) om een titel en jaar uit een bestandsnaam te halen.
const QRegularExpression cleanDateTime(QStringLiteral(
    R"((.*[^ _\,\.\(\)\[\]\-])[ _\.\(\)\[\]\-]+(19[0-9][0-9]|20[0-9][0-9])([ _\,\.\(\)\[\]\-]|[^0-9]$)?)"));
const QRegularExpression cleanStrings(QStringLiteral(
    R"([ _\,\.\(\)\[\]\-](ac3|dts|custom|dc|remastered|divx|divx5|dsr|dsrip|dutch|dvd|dvd5|dvd9|dvdrip|dvdscr|dvdscreener|screener|dvdivx|cam|fragment|fs|hdtv|hdrip|hdtvrip|internal|limited|multisubs|ntsc|ogg|ogm|pal|pdtv|proper|repack|rerip|retail|r3|r5|bd5|se|svcd|swedish|german|read.nfo|nfofix|unrated|extended|ws|telesync|ts|telecine|tc|brrip|bdrip|480p|480i|576p|576i|720p|720i|1080p|1080i|2160p|hrhd|hrhdtv|hddvd|bluray|x264|h264|x265|h265|hevc|web-?dl|webrip|xvid|xvidvd|xxx|www.www|cd[1-9]|\[.*\])([ _\,\.\(\)\[\]\-]|$))"),
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression cleanBrackets(QStringLiteral(R"((\[.*\]))"));

// Afleveringsnummers, zoals Kodi's tvshowmatching: S01E02, 1x02, of "Seizoen 1/E02"
const QRegularExpression episodeSxE(QStringLiteral(R"([Ss]([0-9]{1,2})[ ._x-]*[Ee]([0-9]{1,3}))"));
const QRegularExpression episodeNxN(QStringLiteral(R"((?:^|[^0-9])([0-9]{1,2})x([0-9]{2,3})(?:[^0-9]|$))"));
const QRegularExpression seasonFolder(QStringLiteral(R"((?:season|seizoen|staffel|saison|serie)[ ._-]*([0-9]{1,2}))"),
                                      QRegularExpression::CaseInsensitiveOption);
const QRegularExpression episodeOnly(QStringLiteral(R"((?:^|[^a-z])(?:e|ep|aflevering)[ ._-]*([0-9]{1,3}))"),
                                     QRegularExpression::CaseInsensitiveOption);

struct CleanName {
    QString title;
    QString year;
};

CleanName cleanName(const QString &raw)
{
    CleanName result{raw, {}};
    const auto date = cleanDateTime.match(raw);
    if (date.hasMatch()) {
        result.title = date.captured(1);
        result.year = date.captured(2);
    }
    for (const QRegularExpression *re : {&cleanStrings, &cleanBrackets}) {
        const auto match = re->match(result.title);
        if (match.hasMatch() && match.capturedStart() > 0) {
            result.title = result.title.left(match.capturedStart());
        }
    }
    result.title.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!result.title.contains(QLatin1Char(' ')) || result.title.count(QLatin1Char('.')) > 1) {
        result.title.replace(QLatin1Char('.'), QLatin1Char(' '));
    }
    result.title = result.title.simplified();
    while (result.title.endsWith(QLatin1Char('-')) || result.title.endsWith(QLatin1Char('('))) {
        result.title.chop(1);
        result.title = result.title.trimmed();
    }
    if (result.title.isEmpty()) {
        result.title = raw;
    }
    return result;
}

QString tmdbImage(const QJsonValue &path, const char *size)
{
    const QString p = path.toString();
    return p.isEmpty() ? QString() : QStringLiteral("https://image.tmdb.org/t/p/%1%2").arg(QLatin1String(size), p);
}

// Afbeeldingen naast de video, met de bestandsnamen die Kodi ook herkent
QString localArt(const QDir &dir, const QStringList &names)
{
    for (const QString &name : names) {
        for (const char *ext : {"jpg", "jpeg", "png", "webp"}) {
            const QString file = dir.filePath(name + QLatin1Char('.') + QLatin1String(ext));
            if (QFile::exists(file)) {
                return QUrl::fromLocalFile(file).toString();
            }
        }
    }
    return {};
}

QString joinNames(const QJsonArray &array, const QString &field = QStringLiteral("name"))
{
    QStringList names;
    for (const QJsonValue &v : array) {
        names << (v.isString() ? v.toString() : v.toObject().value(field).toString());
    }
    names.removeAll(QString());
    return names.join(QStringLiteral(" / "));
}

QJsonArray tmdbCast(const QJsonObject &credits)
{
    QJsonArray cast;
    const QJsonArray source = credits.value(QStringLiteral("cast")).toArray();
    for (int i = 0; i < source.size() && cast.size() < 12; ++i) {
        const QJsonObject c = source.at(i).toObject();
        cast.append(QJsonObject{
            {QStringLiteral("name"), c.value(QStringLiteral("name")).toString()},
            {QStringLiteral("role"), c.value(QStringLiteral("character")).toString()},
            {QStringLiteral("photo"), tmdbImage(c.value(QStringLiteral("profile_path")), "w185")},
        });
    }
    return cast;
}

QJsonArray scanEpisodes(const QString &showDir)
{
    QList<QJsonObject> found;
    QDirIterator it(showDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (!videoExtensions.contains(info.suffix().toLower())) {
            continue;
        }
        const QString name = info.completeBaseName();
        int season = -1;
        int episode = -1;
        auto m = episodeSxE.match(name);
        if (!m.hasMatch()) {
            m = episodeNxN.match(name);
        }
        if (m.hasMatch()) {
            season = m.captured(1).toInt();
            episode = m.captured(2).toInt();
        } else {
            const auto folder = seasonFolder.match(info.dir().dirName());
            const auto ep = episodeOnly.match(name);
            if (folder.hasMatch() && ep.hasMatch()) {
                season = folder.captured(1).toInt();
                episode = ep.captured(1).toInt();
            }
        }
        if (season < 0) {
            continue;
        }
        found.append(QJsonObject{
            {QStringLiteral("season"), season},
            {QStringLiteral("episode"), episode},
            {QStringLiteral("path"), QDir::toNativeSeparators(info.absoluteFilePath())},
        });
    }
    std::sort(found.begin(), found.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const int sa = a.value(QStringLiteral("season")).toInt();
        const int sb = b.value(QStringLiteral("season")).toInt();
        return sa != sb ? sa < sb : a.value(QStringLiteral("episode")).toInt() < b.value(QStringLiteral("episode")).toInt();
    });
    QJsonArray result;
    for (const QJsonObject &e : found) {
        result.append(e);
    }
    return result;
}

QString toFolder(const QString &value)
{
    QString path = value.trimmed();
    if (path.startsWith(QLatin1String("file:"))) {
        path = QUrl(path).toLocalFile();
    }
    return path.isEmpty() ? QString() : QDir::fromNativeSeparators(QDir::cleanPath(path));
}

// Per aflevering titel + info (plot, uitzenddatum, duur, cijfer, afbeelding) uit een
// TMDB tv-antwoord met season/N append_to_response. Sleutel: "seizoen-aflevering".
void collectTmdbEpisodes(const QJsonObject &show, const QList<int> &seasons, QJsonObject &titles, QJsonObject &info)
{
    for (int n : seasons) {
        const QJsonArray eps = show.value(QStringLiteral("season/%1").arg(n)).toObject().value(QStringLiteral("episodes")).toArray();
        for (const QJsonValue &e : eps) {
            const QJsonObject ep = e.toObject();
            const QString key = QStringLiteral("%1-%2").arg(n).arg(ep.value(QStringLiteral("episode_number")).toInt());
            const QString name = ep.value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                titles.insert(key, name);
            }
            QJsonObject m;
            m.insert(QStringLiteral("overview"), ep.value(QStringLiteral("overview")).toString());
            const QString air = ep.value(QStringLiteral("air_date")).toString();
            if (!air.isEmpty()) {
                m.insert(QStringLiteral("air"), air);
            }
            const int runtime = ep.value(QStringLiteral("runtime")).toInt();
            if (runtime > 0) {
                m.insert(QStringLiteral("runtime"), runtime);
            }
            const double rating = ep.value(QStringLiteral("vote_average")).toDouble();
            if (rating > 0) {
                m.insert(QStringLiteral("rating"), qRound(rating * 10) / 10.0);
            }
            const QString still = tmdbImage(ep.value(QStringLiteral("still_path")), "w780");
            if (!still.isEmpty()) {
                m.insert(QStringLiteral("still"), still);
            }
            info.insert(key, m);
        }
    }
}

// Engelse terugval invullen: alleen lege velden aanvullen, Nederlands blijft voorgaan.
void fillEpisodeGaps(const QJsonObject &titles2, const QJsonObject &info2, QJsonObject &titles, QJsonObject &info)
{
    for (auto it = info2.constBegin(); it != info2.constEnd(); ++it) {
        const QString key = it.key();
        if (titles.value(key).toString().isEmpty() && !titles2.value(key).toString().isEmpty()) {
            titles.insert(key, titles2.value(key));
        }
        QJsonObject m = info.value(key).toObject();
        const QJsonObject en = it.value().toObject();
        if (m.value(QStringLiteral("overview")).toString().isEmpty()) {
            m.insert(QStringLiteral("overview"), en.value(QStringLiteral("overview")));
        }
        for (const char *field : {"air", "runtime", "rating", "still"}) {
            if (!m.contains(QLatin1String(field)) && en.contains(QLatin1String(field))) {
                m.insert(QLatin1String(field), en.value(QLatin1String(field)));
            }
        }
        info.insert(key, m);
    }
}

// Afleveringen die op schijf staan maar (nog) geen plot hebben: dan Engelse terugval halen.
bool hasEmptyOverview(const QJsonObject &entry, const QJsonObject &info)
{
    for (const QJsonValue &e : entry.value(QStringLiteral("episodes")).toArray()) {
        const QJsonObject ep = e.toObject();
        const QString key = QStringLiteral("%1-%2").arg(ep.value(QStringLiteral("season")).toInt()).arg(ep.value(QStringLiteral("episode")).toInt());
        if (info.value(key).toObject().value(QStringLiteral("overview")).toString().isEmpty()) {
            return true;
        }
    }
    return false;
}

} // namespace

MediaLibrary::MediaLibrary(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
{
    // Lijsten van mappen; de enkele map van oudere versies wordt overgenomen
    m_moviesFolders = m_settings.value(QStringLiteral("library/moviesFolders")).toStringList();
    m_showsFolders = m_settings.value(QStringLiteral("library/showsFolders")).toStringList();
    const QString oldMovies = m_settings.value(QStringLiteral("library/moviesFolder")).toString();
    const QString oldShows = m_settings.value(QStringLiteral("library/showsFolder")).toString();
    if (m_moviesFolders.isEmpty() && !oldMovies.isEmpty()) {
        m_moviesFolders << oldMovies;
    }
    if (m_showsFolders.isEmpty() && !oldShows.isEmpty()) {
        m_showsFolders << oldShows;
    }
    m_userKey = SecureStore::value(m_settings, QStringLiteral("library/tmdbKey")).toString();
    m_apiKey = m_userKey.isEmpty() ? ApiKeys::testTmdbKey() : m_userKey;

    QFile file(m_dataDir + QStringLiteral("/library.json"));
    if (file.open(QIODevice::ReadOnly)) {
        m_cache = QJsonDocument::fromJson(file.readAll()).object();
    }

    m_publishTimer.setSingleShot(true);
    m_publishTimer.setInterval(1200);
    connect(&m_publishTimer, &QTimer::timeout, this, &MediaLibrary::publish);

    publish();
    m_status = summary();
}

QString MediaLibrary::moviesFolder() const
{
    return m_moviesFolders.isEmpty() ? QString() : QDir::toNativeSeparators(m_moviesFolders.first());
}

QString MediaLibrary::showsFolder() const
{
    return m_showsFolders.isEmpty() ? QString() : QDir::toNativeSeparators(m_showsFolders.first());
}

QStringList MediaLibrary::moviesFolders() const
{
    QStringList list;
    for (const QString &f : m_moviesFolders) {
        list << QDir::toNativeSeparators(f);
    }
    return list;
}

QStringList MediaLibrary::showsFolders() const
{
    QStringList list;
    for (const QString &f : m_showsFolders) {
        list << QDir::toNativeSeparators(f);
    }
    return list;
}

QString MediaLibrary::localPath(const QUrl &url) const
{
    return QDir::toNativeSeparators(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void MediaLibrary::configure(const QString &moviesFolder, const QString &showsFolder, const QString &apiKey)
{
    configureFolders(moviesFolder.isEmpty() ? QStringList() : QStringList{moviesFolder},
                     showsFolder.isEmpty() ? QStringList() : QStringList{showsFolder}, apiKey);
}

void MediaLibrary::configureFolders(const QStringList &moviesFolders, const QStringList &showsFolders, const QString &apiKey)
{
    auto clean = [](const QStringList &in) {
        QStringList out;
        for (const QString &f : in) {
            const QString folder = toFolder(f);
            if (!folder.isEmpty() && !out.contains(folder, Qt::CaseInsensitive)) {
                out << folder;
            }
        }
        return out;
    };
    m_moviesFolders = clean(moviesFolders);
    m_showsFolders = clean(showsFolders);
    m_userKey = apiKey.trimmed();
    m_apiKey = m_userKey.isEmpty() ? ApiKeys::testTmdbKey() : m_userKey;
    m_keyRejected = false;
    m_settings.setValue(QStringLiteral("library/moviesFolders"), m_moviesFolders);
    m_settings.setValue(QStringLiteral("library/showsFolders"), m_showsFolders);
    m_settings.setValue(QStringLiteral("library/moviesFolder"), moviesFolder());
    m_settings.setValue(QStringLiteral("library/showsFolder"), showsFolder());
    SecureStore::setValue(m_settings, QStringLiteral("library/tmdbKey"), m_userKey);
    m_settings.sync();
    Q_EMIT settingsChanged();
    qInfo() << "Library folders" << m_moviesFolders << m_showsFolders << "key" << (m_userKey.isEmpty() ? "none" : "own");
    scan(false);
}

void MediaLibrary::scan(bool refreshAll)
{
    if (m_busy) {
        m_queue.clear(); // lopende ronde afbreken; daarna opnieuw
        m_rescanPending = true;
        return;
    }
    const bool canImprove = !m_apiKey.isEmpty() && !m_keyRejected;
    auto needsScrape = [&](const QJsonObject &entry) {
        const QString source = entry.value(QStringLiteral("source")).toString();
        return entry.isEmpty() || refreshAll || source.isEmpty() || (source == QLatin1String("none") && canImprove)
            || (entry.value(QStringLiteral("kind")).toString() == QLatin1String("show") && source == QLatin1String("tvmaze")
                && canImprove && !entry.value(QStringLiteral("tmdbTried")).toBool());
    };
    // Eenmalig: eerder gescrapete series zonder afleveringsinfo opnieuw ophalen,
    // zónder de bestaande gegevens te wissen (requeue, geen skelet).
    auto wantsEpisodeInfo = [](const QJsonObject &entry) {
        const QString source = entry.value(QStringLiteral("source")).toString();
        return !entry.isEmpty() && !entry.contains(QStringLiteral("episodeInfo"))
            && (source == QLatin1String("tmdb") || source == QLatin1String("tvmaze"));
    };

    QJsonObject fresh;
    QStringList queue;

    for (const QString &moviesFolder : std::as_const(m_moviesFolders)) {
        if (!QDir(moviesFolder).exists()) {
            qWarning() << "Library: movies folder not reachable" << moviesFolder;
            continue;
        }
        const QDir root(moviesFolder);
        QDirIterator it(moviesFolder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            if (!videoExtensions.contains(info.suffix().toLower())) {
                continue;
            }
            const QString lower = info.absoluteFilePath().toLower();
            const QString base = info.completeBaseName();
            if (lower.contains(QLatin1String("/extras/")) || lower.contains(QLatin1String("/featurettes/"))
                || lower.contains(QLatin1String("/trailers/")) || lower.contains(QLatin1String("-trailer"))
                || (base.toLower().contains(QLatin1String("sample")) && info.size() < 300LL * 1024 * 1024)) {
                continue;
            }
            const QString key = QDir::toNativeSeparators(info.absoluteFilePath());
            QJsonObject entry = m_cache.value(key).toObject();
            if (needsScrape(entry)) {
                CleanName name = cleanName(base);
                // Geen jaar in de bestandsnaam: dan de mapnaam proberen ("Film (1999)/film.mkv")
                if (name.year.isEmpty() && info.dir() != root) {
                    const CleanName folder = cleanName(info.dir().dirName());
                    if (!folder.year.isEmpty()) {
                        name = folder;
                    }
                }
                entry = QJsonObject{
                    {QStringLiteral("kind"), QStringLiteral("movie")},
                    {QStringLiteral("path"), key},
                    {QStringLiteral("title"), name.title},
                    {QStringLiteral("searchTitle"), name.title},
                    {QStringLiteral("year"), name.year},
                };
                queue << key;
            }
            QStringList posters{base + QStringLiteral("-poster")};
            QStringList fanart{base + QStringLiteral("-fanart")};
            if (info.dir() != root) { // eigen map per film: ook de algemene namen
                posters << QStringLiteral("poster") << QStringLiteral("folder") << QStringLiteral("cover");
                fanart << QStringLiteral("fanart") << QStringLiteral("backdrop");
            }
            entry[QStringLiteral("localPoster")] = localArt(info.dir(), posters);
            entry[QStringLiteral("localBackdrop")] = localArt(info.dir(), fanart);
            fresh.insert(key, entry);
        }
    }

    for (const QString &showsFolder : std::as_const(m_showsFolders)) {
        if (!QDir(showsFolder).exists()) {
            qWarning() << "Library: shows folder not reachable" << showsFolder;
            continue;
        }
        const auto dirs = QDir(showsFolder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &dirInfo : dirs) {
            const QJsonArray episodes = scanEpisodes(dirInfo.absoluteFilePath());
            if (episodes.isEmpty()) {
                continue;
            }
            const QString key = QDir::toNativeSeparators(dirInfo.absoluteFilePath());
            QJsonObject entry = m_cache.value(key).toObject();
            // Nieuwe afleveringen erbij gekomen: titels opnieuw ophalen
            QSet<QString> knownFiles;
            for (const QJsonValue &e : entry.value(QStringLiteral("episodes")).toArray()) {
                knownFiles.insert(e.toObject().value(QStringLiteral("path")).toString());
            }
            bool newEpisodes = false;
            for (const QJsonValue &e : episodes) {
                newEpisodes = newEpisodes || !knownFiles.contains(e.toObject().value(QStringLiteral("path")).toString());
            }
            if (needsScrape(entry) || (newEpisodes && !entry.isEmpty())) {
                const CleanName name = cleanName(dirInfo.fileName());
                entry = QJsonObject{
                    {QStringLiteral("kind"), QStringLiteral("show")},
                    {QStringLiteral("path"), key},
                    {QStringLiteral("title"), name.title},
                    {QStringLiteral("searchTitle"), name.title},
                    {QStringLiteral("year"), name.year},
                };
                queue << key;
            } else if (wantsEpisodeInfo(entry)) {
                // Bestaande serie bijwerken: gegevens behouden, alleen opnieuw ophalen
                queue << key;
            }
            entry[QStringLiteral("episodes")] = episodes;
            const QDir dir(dirInfo.absoluteFilePath());
            entry[QStringLiteral("localPoster")] = localArt(dir, {QStringLiteral("poster"), QStringLiteral("folder"), QStringLiteral("cover")});
            entry[QStringLiteral("localBackdrop")] = localArt(dir, {QStringLiteral("fanart"), QStringLiteral("backdrop")});
            fresh.insert(key, entry);
        }
    }

    m_cache = fresh;
    m_queue = queue;
    m_total = queue.size();
    publish();
    save();
    qInfo() << "Library scan:" << fresh.size() << "items," << queue.size() << "to look up";
    if (queue.isEmpty()) {
        setStatus(summary(), false);
        return;
    }
    m_busy = true;
    next();
}

void MediaLibrary::next()
{
    if (m_queue.isEmpty()) {
        m_busy = false;
        m_publishTimer.stop();
        publish();
        save();
        setStatus(summary(), false);
        if (m_rescanPending) {
            m_rescanPending = false;
            scan(false);
        }
        return;
    }
    const QString key = m_queue.takeFirst();
    const QJsonObject entry = m_cache.value(key).toObject();
    setStatus(tr("Informatie ophalen: %1 van %2  ·  %3").arg(m_total - m_queue.size()).arg(m_total)
                  .arg(entry.value(QStringLiteral("title")).toString()), true);
    if (entry.isEmpty()) {
        QMetaObject::invokeMethod(this, &MediaLibrary::next, Qt::QueuedConnection);
    } else if (entry.value(QStringLiteral("kind")).toString() == QLatin1String("movie")) {
        scrapeMovie(key, true);
    } else if (!m_apiKey.isEmpty() && !m_keyRejected) {
        scrapeShowTmdb(key, true);
    } else {
        scrapeShowTvmaze(key);
    }
}

void MediaLibrary::finish(const QString &key, QJsonObject entry, const QString &source)
{
    if (m_cache.contains(key)) { // kan intussen verdwenen zijn door een nieuwe scan
        entry[QStringLiteral("source")] = source;
        m_cache.insert(key, entry);
        if (!m_publishTimer.isActive()) {
            m_publishTimer.start();
        }
    }
    qInfo().noquote() << "Library:" << entry.value(QStringLiteral("title")).toString() << "->" << source;
    QMetaObject::invokeMethod(this, &MediaLibrary::next, Qt::QueuedConnection);
}

void MediaLibrary::getJson(QNetworkRequest request, const JsonCallback &callback)
{
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Kijkluister (gebaseerd op Plasma Bigscreen)"));
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            qWarning().noquote() << "Network error" << reply->url().host() << reply->errorString();
        }
        callback(QJsonDocument::fromJson(bytes), status);
    });
}

void MediaLibrary::tmdb(const QString &path, const QList<QPair<QString, QString>> &query, const JsonCallback &callback)
{
    QUrl url(QStringLiteral("https://api.themoviedb.org/3/") + path);
    QUrlQuery q;
    for (const auto &item : query) {
        q.addQueryItem(item.first, item.second);
    }
    QNetworkRequest request;
    // Een "API Read Access Token" (lang) gaat als header, een gewone API-sleutel als parameter
    if (m_apiKey.size() > 40) {
        request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    } else {
        q.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    }
    url.setQuery(q);
    request.setUrl(url);
    getJson(request, callback);
}

void MediaLibrary::keyRejected()
{
    if (!m_keyRejected) {
        qWarning() << "TMDB key rejected";
    }
    m_keyRejected = true;
}

void MediaLibrary::scrapeMovie(const QString &key, bool withYear)
{
    const QJsonObject start = m_cache.value(key).toObject();
    if (m_apiKey.isEmpty() || m_keyRejected) {
        finish(key, start, QStringLiteral("none"));
        return;
    }
    const QString year = start.value(QStringLiteral("year")).toString();
    QList<QPair<QString, QString>> query{
        {QStringLiteral("query"), start.value(QStringLiteral("searchTitle")).toString()},
        {QStringLiteral("language"), QStringLiteral("nl-NL")},
        {QStringLiteral("include_adult"), QStringLiteral("false")},
    };
    if (withYear && !year.isEmpty()) {
        query.append({QStringLiteral("year"), year});
    }
    tmdb(QStringLiteral("search/movie"), query, [this, key, withYear, year](const QJsonDocument &doc, int status) {
        const QJsonObject entry = m_cache.value(key).toObject();
        if (status == 401) {
            keyRejected();
            finish(key, entry, QStringLiteral("none"));
            return;
        }
        const QJsonArray results = doc.object().value(QStringLiteral("results")).toArray();
        if (results.isEmpty()) {
            if (status == 200 && withYear && !year.isEmpty()) {
                scrapeMovie(key, false); // zoals Kodi: nog eens zonder jaar
                return;
            }
            finish(key, entry, status == 200 ? QStringLiteral("notfound") : QStringLiteral("none"));
            return;
        }
        const int id = results.first().toObject().value(QStringLiteral("id")).toInt();
        tmdb(QStringLiteral("movie/%1").arg(id),
             {{QStringLiteral("language"), QStringLiteral("nl-NL")}, {QStringLiteral("append_to_response"), QStringLiteral("credits")}},
             [this, key, id](const QJsonDocument &doc, int status) {
                 QJsonObject entry = m_cache.value(key).toObject();
                 if (status != 200) {
                     finish(key, entry, QStringLiteral("none"));
                     return;
                 }
                 const QJsonObject m = doc.object();
                 entry[QStringLiteral("tmdbId")] = id;
                 entry[QStringLiteral("title")] = m.value(QStringLiteral("title")).toString(entry.value(QStringLiteral("title")).toString());
                 const QString release = m.value(QStringLiteral("release_date")).toString();
                 if (!release.isEmpty()) {
                     entry[QStringLiteral("year")] = release.left(4);
                 }
                 entry[QStringLiteral("overview")] = m.value(QStringLiteral("overview")).toString();
                 entry[QStringLiteral("tagline")] = m.value(QStringLiteral("tagline")).toString();
                 entry[QStringLiteral("genres")] = joinNames(m.value(QStringLiteral("genres")).toArray());
                 const double rating = m.value(QStringLiteral("vote_average")).toDouble();
                 entry[QStringLiteral("rating")] = rating > 0 ? qRound(rating * 10) / 10.0 : 0.0;
                 entry[QStringLiteral("runtime")] = m.value(QStringLiteral("runtime")).toInt();
                 entry[QStringLiteral("poster")] = tmdbImage(m.value(QStringLiteral("poster_path")), "w500");
                 entry[QStringLiteral("backdrop")] = tmdbImage(m.value(QStringLiteral("backdrop_path")), "w1280");
                 const QJsonObject credits = m.value(QStringLiteral("credits")).toObject();
                 entry[QStringLiteral("cast")] = tmdbCast(credits);
                 QStringList directors;
                 for (const QJsonValue &c : credits.value(QStringLiteral("crew")).toArray()) {
                     if (c.toObject().value(QStringLiteral("job")).toString() == QLatin1String("Director")) {
                         directors << c.toObject().value(QStringLiteral("name")).toString();
                     }
                 }
                 entry[QStringLiteral("maker")] = directors.join(QStringLiteral(", "));
                 entry[QStringLiteral("makerLabel")] = QStringLiteral("Regie");
                 if (!entry.value(QStringLiteral("overview")).toString().isEmpty()) {
                     finish(key, entry, QStringLiteral("tmdb"));
                     return;
                 }
                 // Geen Nederlandse beschrijving: Engelse gebruiken, zoals de Kodi-scraper
                 tmdb(QStringLiteral("movie/%1").arg(id), {{QStringLiteral("language"), QStringLiteral("en-US")}},
                      [this, key, entry](const QJsonDocument &doc, int status) mutable {
                          if (status == 200) {
                              entry[QStringLiteral("overview")] = doc.object().value(QStringLiteral("overview")).toString();
                          }
                          finish(key, entry, QStringLiteral("tmdb"));
                      });
             });
    });
}

void MediaLibrary::scrapeShowTmdb(const QString &key, bool withYear)
{
    const QJsonObject start = m_cache.value(key).toObject();
    const QString year = start.value(QStringLiteral("year")).toString();
    QList<QPair<QString, QString>> query{
        {QStringLiteral("query"), start.value(QStringLiteral("searchTitle")).toString()},
        {QStringLiteral("language"), QStringLiteral("nl-NL")},
    };
    if (withYear && !year.isEmpty()) {
        query.append({QStringLiteral("first_air_date_year"), year});
    }
    tmdb(QStringLiteral("search/tv"), query, [this, key, withYear, year](const QJsonDocument &doc, int status) {
        const QJsonObject entry = m_cache.value(key).toObject();
        if (status == 401) {
            keyRejected();
            scrapeShowTvmaze(key);
            return;
        }
        const QJsonArray results = doc.object().value(QStringLiteral("results")).toArray();
        if (results.isEmpty()) {
            if (status == 200 && withYear && !year.isEmpty()) {
                scrapeShowTmdb(key, false);
                return;
            }
            // Tweede kans bij TVmaze; TMDB dan niet bij elke scan opnieuw proberen
            QJsonObject tried = entry;
            tried[QStringLiteral("tmdbTried")] = true;
            if (m_cache.contains(key)) {
                m_cache.insert(key, tried);
            }
            scrapeShowTvmaze(key);
            return;
        }
        const int id = results.first().toObject().value(QStringLiteral("id")).toInt();
        // Seizoenen meteen meenemen in dezelfde aanroep, zoals de Kodi-scraper (max. 20 extra's)
        QSet<int> seasons;
        for (const QJsonValue &e : entry.value(QStringLiteral("episodes")).toArray()) {
            seasons.insert(e.toObject().value(QStringLiteral("season")).toInt());
        }
        QStringList appends{QStringLiteral("credits")};
        QList<int> sorted(seasons.begin(), seasons.end());
        std::sort(sorted.begin(), sorted.end());
        for (int s : std::as_const(sorted)) {
            if (appends.size() < 20) {
                appends << QStringLiteral("season/%1").arg(s);
            }
        }
        tmdb(QStringLiteral("tv/%1").arg(id),
             {{QStringLiteral("language"), QStringLiteral("nl-NL")}, {QStringLiteral("append_to_response"), appends.join(QLatin1Char(','))}},
             [this, key, id, sorted, appends](const QJsonDocument &doc, int status) {
                 QJsonObject entry = m_cache.value(key).toObject();
                 if (status != 200) {
                     finish(key, entry, QStringLiteral("none"));
                     return;
                 }
                 const QJsonObject s = doc.object();
                 entry[QStringLiteral("tmdbId")] = id;
                 entry[QStringLiteral("title")] = s.value(QStringLiteral("name")).toString(entry.value(QStringLiteral("title")).toString());
                 const QString aired = s.value(QStringLiteral("first_air_date")).toString();
                 if (!aired.isEmpty()) {
                     entry[QStringLiteral("year")] = aired.left(4);
                 }
                 entry[QStringLiteral("overview")] = s.value(QStringLiteral("overview")).toString();
                 entry[QStringLiteral("genres")] = joinNames(s.value(QStringLiteral("genres")).toArray());
                 const double rating = s.value(QStringLiteral("vote_average")).toDouble();
                 entry[QStringLiteral("rating")] = rating > 0 ? qRound(rating * 10) / 10.0 : 0.0;
                 entry[QStringLiteral("poster")] = tmdbImage(s.value(QStringLiteral("poster_path")), "w500");
                 entry[QStringLiteral("backdrop")] = tmdbImage(s.value(QStringLiteral("backdrop_path")), "w1280");
                 entry[QStringLiteral("cast")] = tmdbCast(s.value(QStringLiteral("credits")).toObject());
                 entry[QStringLiteral("maker")] = joinNames(s.value(QStringLiteral("created_by")).toArray()).replace(QStringLiteral(" / "), QStringLiteral(", "));
                 entry[QStringLiteral("makerLabel")] = QStringLiteral("Bedenker");
                 QJsonObject titles;
                 QJsonObject info;
                 collectTmdbEpisodes(s, sorted, titles, info);
                 entry[QStringLiteral("episodeTitles")] = titles;
                 entry[QStringLiteral("episodeInfo")] = info;
                 // Veel Nederlandse afleveringsplots zijn leeg: dan Engels bijhalen en lege velden invullen.
                 if (!hasEmptyOverview(entry, info)) {
                     finish(key, entry, QStringLiteral("tmdb"));
                     return;
                 }
                 // Nederlandse gegevens vast bewaren zodat de Engelse terugval ze kan aanvullen
                 if (m_cache.contains(key)) {
                     m_cache.insert(key, entry);
                 }
                 tmdb(QStringLiteral("tv/%1").arg(id),
                      {{QStringLiteral("language"), QStringLiteral("en-US")}, {QStringLiteral("append_to_response"), appends.join(QLatin1Char(','))}},
                      [this, key, sorted](const QJsonDocument &enDoc, int enStatus) {
                          QJsonObject entry = m_cache.value(key).toObject();
                          if (enStatus == 200) {
                              QJsonObject titles = entry.value(QStringLiteral("episodeTitles")).toObject();
                              QJsonObject info = entry.value(QStringLiteral("episodeInfo")).toObject();
                              QJsonObject titles2;
                              QJsonObject info2;
                              collectTmdbEpisodes(enDoc.object(), sorted, titles2, info2);
                              fillEpisodeGaps(titles2, info2, titles, info);
                              entry[QStringLiteral("episodeTitles")] = titles;
                              entry[QStringLiteral("episodeInfo")] = info;
                          }
                          finish(key, entry, QStringLiteral("tmdb"));
                      });
             });
    });
}

void MediaLibrary::scrapeShowTvmaze(const QString &key)
{
    const QJsonObject start = m_cache.value(key).toObject();
    QUrl url(QStringLiteral("https://api.tvmaze.com/singlesearch/shows"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), start.value(QStringLiteral("searchTitle")).toString());
    query.addQueryItem(QStringLiteral("embed[]"), QStringLiteral("episodes"));
    query.addQueryItem(QStringLiteral("embed[]"), QStringLiteral("cast"));
    url.setQuery(query);
    getJson(QNetworkRequest(url), [this, key](const QJsonDocument &doc, int status) {
        QJsonObject entry = m_cache.value(key).toObject();
        if (status != 200) {
            finish(key, entry, status == 404 ? QStringLiteral("notfound") : QStringLiteral("none"));
            return;
        }
        const QJsonObject s = doc.object();
        const int id = s.value(QStringLiteral("id")).toInt();
        entry[QStringLiteral("title")] = s.value(QStringLiteral("name")).toString(entry.value(QStringLiteral("title")).toString());
        const QString premiered = s.value(QStringLiteral("premiered")).toString();
        if (!premiered.isEmpty()) {
            entry[QStringLiteral("year")] = premiered.left(4);
        }
        QString summary = s.value(QStringLiteral("summary")).toString();
        summary.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        entry[QStringLiteral("overview")] = summary.trimmed();
        entry[QStringLiteral("genres")] = joinNames(s.value(QStringLiteral("genres")).toArray());
        entry[QStringLiteral("rating")] = s.value(QStringLiteral("rating")).toObject().value(QStringLiteral("average")).toDouble();
        const QJsonObject image = s.value(QStringLiteral("image")).toObject();
        entry[QStringLiteral("poster")] = image.value(QStringLiteral("original")).toString(image.value(QStringLiteral("medium")).toString());
        const QJsonObject embedded = s.value(QStringLiteral("_embedded")).toObject();
        QJsonArray cast;
        for (const QJsonValue &c : embedded.value(QStringLiteral("cast")).toArray()) {
            if (cast.size() >= 12) {
                break;
            }
            const QJsonObject person = c.toObject().value(QStringLiteral("person")).toObject();
            cast.append(QJsonObject{
                {QStringLiteral("name"), person.value(QStringLiteral("name")).toString()},
                {QStringLiteral("role"), c.toObject().value(QStringLiteral("character")).toObject().value(QStringLiteral("name")).toString()},
                {QStringLiteral("photo"), person.value(QStringLiteral("image")).toObject().value(QStringLiteral("medium")).toString()},
            });
        }
        entry[QStringLiteral("cast")] = cast;
        QJsonObject titles;
        QJsonObject info;
        for (const QJsonValue &e : embedded.value(QStringLiteral("episodes")).toArray()) {
            const QJsonObject ep = e.toObject();
            const QString epKey = QStringLiteral("%1-%2").arg(ep.value(QStringLiteral("season")).toInt()).arg(ep.value(QStringLiteral("number")).toInt());
            const QString name = ep.value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                titles.insert(epKey, name);
            }
            QString sum = ep.value(QStringLiteral("summary")).toString();
            sum.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
            QJsonObject m;
            m.insert(QStringLiteral("overview"), sum.trimmed());
            const QString air = ep.value(QStringLiteral("airdate")).toString();
            if (!air.isEmpty()) {
                m.insert(QStringLiteral("air"), air);
            }
            const int runtime = ep.value(QStringLiteral("runtime")).toInt();
            if (runtime > 0) {
                m.insert(QStringLiteral("runtime"), runtime);
            }
            const QJsonObject epImage = ep.value(QStringLiteral("image")).toObject();
            const QString still = epImage.value(QStringLiteral("original")).toString(epImage.value(QStringLiteral("medium")).toString());
            if (!still.isEmpty()) {
                m.insert(QStringLiteral("still"), still);
            }
            info.insert(epKey, m);
        }
        entry[QStringLiteral("episodeTitles")] = titles;
        entry[QStringLiteral("episodeInfo")] = info;
        entry[QStringLiteral("maker")] = QString();
        // Achtergrondafbeelding staat in een aparte lijst
        getJson(QNetworkRequest(QUrl(QStringLiteral("https://api.tvmaze.com/shows/%1/images").arg(id))),
                [this, key, entry](const QJsonDocument &doc, int status) mutable {
                    if (status == 200) {
                        QString best;
                        for (const QJsonValue &v : doc.array()) {
                            const QJsonObject img = v.toObject();
                            if (img.value(QStringLiteral("type")).toString() != QLatin1String("background")) {
                                continue;
                            }
                            const QString url = img.value(QStringLiteral("resolutions")).toObject().value(QStringLiteral("original"))
                                                    .toObject().value(QStringLiteral("url")).toString();
                            if (best.isEmpty() || img.value(QStringLiteral("main")).toBool()) {
                                best = url;
                            }
                        }
                        entry[QStringLiteral("backdrop")] = best;
                    }
                    finish(key, entry, QStringLiteral("tvmaze"));
                });
    });
}

void MediaLibrary::publish()
{
    QVariantList movies;
    QVariantList shows;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        QVariantMap item = entry.toVariantMap();
        // Eigen afbeeldingen naast de video gaan voor, net als in Kodi
        if (!entry.value(QStringLiteral("localPoster")).toString().isEmpty()) {
            item[QStringLiteral("poster")] = entry.value(QStringLiteral("localPoster")).toString();
        }
        if (!entry.value(QStringLiteral("localBackdrop")).toString().isEmpty()) {
            item[QStringLiteral("backdrop")] = entry.value(QStringLiteral("localBackdrop")).toString();
        }
        if (entry.value(QStringLiteral("kind")).toString() == QLatin1String("show")) {
            const QJsonObject titles = entry.value(QStringLiteral("episodeTitles")).toObject();
            const QJsonObject info = entry.value(QStringLiteral("episodeInfo")).toObject();
            QVariantList episodes;
            for (const QJsonValue &e : entry.value(QStringLiteral("episodes")).toArray()) {
                const QJsonObject ep = e.toObject();
                const int season = ep.value(QStringLiteral("season")).toInt();
                const int number = ep.value(QStringLiteral("episode")).toInt();
                const QJsonObject ei = info.value(QStringLiteral("%1-%2").arg(season).arg(number)).toObject();
                episodes.append(QVariantMap{
                    {QStringLiteral("season"), season},
                    {QStringLiteral("episode"), number},
                    {QStringLiteral("code"), QStringLiteral("S%1E%2").arg(season, 2, 10, QLatin1Char('0')).arg(number, 2, 10, QLatin1Char('0'))},
                    {QStringLiteral("title"), titles.value(QStringLiteral("%1-%2").arg(season).arg(number)).toString()},
                    {QStringLiteral("path"), ep.value(QStringLiteral("path")).toString()},
                    {QStringLiteral("overview"), ei.value(QStringLiteral("overview")).toString()},
                    {QStringLiteral("air"), ei.value(QStringLiteral("air")).toString()},
                    {QStringLiteral("runtime"), ei.value(QStringLiteral("runtime")).toInt()},
                    {QStringLiteral("rating"), ei.value(QStringLiteral("rating")).toDouble()},
                    {QStringLiteral("still"), ei.value(QStringLiteral("still")).toString()},
                });
            }
            item[QStringLiteral("episodes")] = episodes;
            item.remove(QStringLiteral("episodeTitles"));
            item.remove(QStringLiteral("episodeInfo"));
            shows.append(item);
        } else {
            movies.append(item);
        }
    }
    auto byTitle = [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("title")).toString().localeAwareCompare(b.toMap().value(QStringLiteral("title")).toString()) < 0;
    };
    std::sort(movies.begin(), movies.end(), byTitle);
    std::sort(shows.begin(), shows.end(), byTitle);
    m_movies = movies;
    m_shows = shows;
    Q_EMIT itemsChanged();
}

void MediaLibrary::save()
{
    QFile file(m_dataDir + QStringLiteral("/library.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(m_cache).toJson(QJsonDocument::Compact));
    }
}

void MediaLibrary::setStatus(const QString &status, bool busy)
{
    m_status = status;
    m_busy = busy;
    Q_EMIT statusChanged();
}

QString MediaLibrary::summary() const
{
    QString text = tr("%1 films  ·  %2 series").arg(m_movies.size()).arg(m_shows.size());
    if (m_keyRejected) {
        text += tr("  ·  TMDB-sleutel geweigerd, controleer hem via Mappen en sleutel");
    } else if (m_apiKey.isEmpty() && !m_moviesFolders.isEmpty()) {
        text += tr("  ·  zonder TMDB-sleutel alleen filmtitels");
    }
    return text;
}

// Serie: deze aflevering en de afleveringen erna als afspeellijst in MPC-HC
bool MediaLibrary::playEpisodes(const QStringList &paths)
{
    QStringList existing;
    for (const QString &p : paths) {
        if (QFileInfo::exists(p)) {
            existing << p;
        }
    }
    if (existing.isEmpty()) {
        return false;
    }
    if (MpcPlayer *mpc = MpcPlayer::instance(); mpc && mpc->play(existing, false)) {
        return true;
    }
    return play(existing.first());
}

bool MediaLibrary::play(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        qWarning() << "Play: file missing" << path;
        return false;
    }
    // In de meegeleverde MPC-HC (schermvullend, sluit na afloop); anders het standaardprogramma
    if (MpcPlayer *mpc = MpcPlayer::instance(); mpc && mpc->play({path}, true)) {
        return true;
    }
    qInfo() << "Play with default program" << path;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MediaLibrary::setApiKey(const QString &key)
{
    m_userKey = key.trimmed();
    m_apiKey = m_userKey.isEmpty() ? ApiKeys::testTmdbKey() : m_userKey;
    m_keyRejected = false;
    SecureStore::setValue(m_settings, QStringLiteral("library/tmdbKey"), m_userKey);
    m_settings.sync();
    Q_EMIT settingsChanged();
    qInfo() << "TMDB key" << (m_userKey.isEmpty() ? "none" : "set");
    if (!m_userKey.isEmpty() && configured()) {
        scan(false);
    }
}
