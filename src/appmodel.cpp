// SPDX-License-Identifier: GPL-2.0-or-later
#include "paths.h"
#include "appmodel.h"
#include "homebridge.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <climits>

namespace {
QString dataDir()
{
    return appDataDir();
}

void removeLocalFile(const QString &fileUrl)
{
    if (!fileUrl.isEmpty()) {
        QFile::remove(QUrl(fileUrl).toLocalFile());
    }
}
}

// Streamingdiensten die in Nederland te kijken zijn. Namen en adressen; het logo
// wordt bij de eerste start van de site zelf opgehaald (zie fetchFavicons).
struct WebDefault {
    const char *name;
    const char *url;
    const char *category; // video, muziek, podcasts
    bool optional = false; // standaard verborgen; aanzetten via Diensten kiezen
};
const WebDefault webDefaults[] = {
    {"Netflix", "https://www.netflix.com", "video"},
    {"Disney+", "https://www.disneyplus.com", "video"},
    {"Prime Video", "https://www.primevideo.com", "video"},
    {"HBO Max", "https://www.hbomax.com", "video"},
    {"Videoland", "https://www.videoland.com", "video"},
    {"NPO Start", "https://npo.nl/start", "video"},
    {"NLZIET", "https://app.nlziet.nl", "video"},
    {"SkyShowtime", "https://www.skyshowtime.com", "video"},
    {"Apple TV", "https://tv.apple.com/nl", "video"},
    {"Viaplay", "https://viaplay.com/nl-nl", "video"},
    {"YouTube", "https://www.youtube.com", "video"},
    {"KIJK", "https://www.kijk.nl", "video"},
    {"Ziggo GO", "https://www.ziggogo.tv", "video"},
    {"KPN TV", "https://tv.kpn.com", "video"},
    {"Odido TV", "https://tv.odido.nl", "video"},
    {"Canal+", "https://www.canalplus.com/nl/", "video"},
    {"Discovery+", "https://www.discoveryplus.com/nl", "video"},
    {"Pathé Thuis", "https://www.pathe-thuis.nl", "video"},
    {"Rakuten TV", "https://www.rakuten.tv/nl", "video"},
    {"MUBI", "https://mubi.com/nl", "video"},
    {"CineMember", "https://www.cinemember.nl", "video"},
    {"Picl", "https://picl.nl", "video"},
    {"Cinetree", "https://cinetree.nl", "video"},
    {"Film1", "https://www.film1.nl", "video"},
    {"Crunchyroll", "https://www.crunchyroll.com", "video"},
    {"F1 TV", "https://f1tv.formula1.com", "video"},
    {"ESPN", "https://www.espn.nl/watch/", "video"},
    {"Twitch", "https://www.twitch.tv", "video"},
    {"Plex", "https://app.plex.tv", "video"},
    // Op Nederland gerichte diensten (vanaf 4.1): standaard verborgen, te kiezen via Diensten kiezen
    {"NOS", "https://nos.nl/live", "video", true},
    {"NPO Zapp", "https://www.zapp.nl", "video", true},
    {"NPO Zappelin", "https://www.zappelin.nl", "video", true},
    {"Schooltv", "https://schooltv.nl", "video", true},
    {"Ziggo Sport", "https://www.ziggosport.nl", "video", true},
    {"DAZN", "https://www.dazn.com/nl-NL", "video", true},
    {"Delta TV", "https://tv.delta.nl", "video", true},
    {"BVN", "https://www.bvn.tv", "video", true},
    {"Human", "https://www.human.nl", "video", true},
    {"Dumpert", "https://www.dumpert.nl", "video", true},
    {"Omroep MAX", "https://www.omroepmax.nl", "video", true},
    {"Eurosport", "https://www.eurosport.nl/watch", "video", true},
    {"Streamz", "https://www.streamz.be", "video", true},
    // Muziek
    {"Spotify", "https://open.spotify.com", "muziek"},
    {"YouTube Music", "https://music.youtube.com", "muziek"},
    {"Apple Music", "https://music.apple.com/nl", "muziek"},
    {"Deezer", "https://www.deezer.com/nl", "muziek"},
    {"Tidal", "https://listen.tidal.com", "muziek"},
    {"Amazon Music", "https://music.amazon.com", "muziek"},
    {"Qobuz", "https://play.qobuz.com", "muziek"},
    {"SoundCloud", "https://soundcloud.com", "muziek"},
    // Nieuws (vanaf 4.2)
    {"NOS", "https://nos.nl", "nieuws"},
    {"NU.nl", "https://www.nu.nl", "nieuws"},
    {"AD", "https://www.ad.nl", "nieuws"},
    {"De Telegraaf", "https://www.telegraaf.nl", "nieuws"},
    {"NRC", "https://www.nrc.nl", "nieuws"},
    {"de Volkskrant", "https://www.volkskrant.nl", "nieuws"},
    {"RTL Nieuws", "https://www.rtl.nl/nieuws", "nieuws"},
    {"Het Financieele Dagblad", "https://fd.nl", "nieuws"},
    // Podcasts (Nederland)
    {"Podimo", "https://podimo.com/nl", "podcasts"},
    {"NPO Luister", "https://www.npoluister.nl", "podcasts"},
    {"Apple Podcasts", "https://podcasts.apple.com/nl", "podcasts"},
    {"Pocket Casts", "https://play.pocketcasts.com", "podcasts"},
};

// Zoekt MPC-HC (64-bit) op de gebruikelijke plekken, ook die van het K-Lite Codec Pack.
// Niet gevonden: de bestandsnaam alleen, zodat de tegel blijft staan en bij openen
// meldt dat het programma ontbreekt.
QString mpcHcPath()
{
    const QString pf = QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles"));
    const QString pf86 = QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles(x86)"));
    const QStringList candidates = {
        pf + QStringLiteral("/MPC-HC/mpc-hc64.exe"),
        pf86 + QStringLiteral("/K-Lite Codec Pack/MPC-HC64/mpc-hc64.exe"),
        pf + QStringLiteral("/K-Lite Codec Pack/MPC-HC64/mpc-hc64.exe"),
        pf86 + QStringLiteral("/MPC-HC/mpc-hc64.exe"),
    };
    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return QStringLiteral("mpc-hc64.exe");
}

AppModel::AppModel(Kind kind, const QString &category, QObject *parent)
    : QAbstractListModel(parent)
    , m_kind(kind)
    , m_category(category.isEmpty() ? QStringLiteral("video") : category)
    , m_settings(dataDir() + QStringLiteral("/settings.ini"), QSettings::IniFormat)
{
    // Nieuwe installatie: nog niets ingesteld (hooguit de bibliotheekmappen)
    m_freshInstall = true;
    for (const QString &k : m_settings.allKeys()) {
        if (!k.startsWith(QLatin1String("library/")) && !k.startsWith(QLatin1String("components/"))) {
            m_freshInstall = false;
            break;
        }
    }
    m_hidden = m_settings.value(key("hiddenApps")).toStringList();
    // Nieuwe optionele diensten één keer verborgen zetten (daarna beslist Diensten kiezen)
    if (m_kind == Web) {
        QStringList introduced = m_settings.value(QStringLiteral("web/optionalIntroduced")).toStringList();
        bool changed = false;
        for (const WebDefault &entry : webDefaults) {
            const QString url = QString::fromUtf8(entry.url);
            if (entry.optional && QString::fromUtf8(entry.category) == m_category && !introduced.contains(url)) {
                introduced.append(url);
                if (!m_hidden.contains(url)) {
                    m_hidden.append(url);
                }
                changed = true;
            }
        }
        if (changed) {
            m_settings.setValue(key("hiddenApps"), m_hidden);
            m_settings.setValue(QStringLiteral("web/optionalIntroduced"), introduced);
        }
    }
    m_customIcons = m_settings.value(key("customIcons")).toMap();
    m_background = m_settings.value(QStringLiteral("background")).toString();
    refresh();
}

QString AppModel::key(const char *name) const
{
    // Elke rij eigen instellingen; "video" houdt de sleutels van eerdere versies
    // Per profiel (4.3); het hoofdprofiel houdt de oude sleutels
    if (m_kind != Web) {
        return profileKey(QLatin1String(name));
    }
    if (m_category == QLatin1String("video")) {
        return profileKey(QStringLiteral("web/") + QLatin1String(name));
    }
    return profileKey(QStringLiteral("web/%1/").arg(m_category) + QLatin1String(name));
}

QList<AppModel::App> AppModel::customTiles() const
{
    QList<App> tiles;
    const QVariantList stored = m_settings.value(key("customTiles")).toList();
    for (const QVariant &entry : stored) {
        const QVariantMap map = entry.toMap();
        tiles.append({map.value(QStringLiteral("name")).toString(), map.value(QStringLiteral("target")).toString(), true,
                      map.value(QStringLiteral("category"), QStringLiteral("video")).toString()});
    }
    return tiles;
}

void AppModel::saveCustomTiles(const QList<App> &tiles)
{
    QVariantList stored;
    for (const App &tile : tiles) {
        stored.append(QVariantMap{{QStringLiteral("name"), tile.name}, {QStringLiteral("target"), tile.path},
                                  {QStringLiteral("category"), tile.category}});
    }
    m_settings.setValue(key("customTiles"), stored);
}

QString AppModel::faviconFile(const QString &url) const
{
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(QUrl(url).host().toUtf8(), QCryptographicHash::Md5).toHex().left(12));
    return dataDir() + QStringLiteral("/images/site-") + hash + QStringLiteral(".png");
}

// Haalt per web-tegel eenmalig het pictogram van de site op en bewaart het lokaal.
void AppModel::fetchFavicons()
{
    QDir().mkpath(dataDir() + QStringLiteral("/images"));
    for (const App &tile : std::as_const(m_apps)) {
        const QString file = faviconFile(tile.path);
        if (QFile::exists(file) || m_customIcons.contains(tile.path)) {
            continue;
        }
        const QString target = tile.path;
        QUrl service(QStringLiteral("https://www.google.com/s2/favicons"));
        service.setQuery(QStringLiteral("sz=128&domain=") + QUrl(target).host());
        QNetworkReply *reply = m_network.get(QNetworkRequest(service));
        connect(reply, &QNetworkReply::finished, this, [this, reply, file, target] {
            reply->deleteLater();
            const QByteArray bytes = reply->readAll();
            QImage image;
            if (reply->error() != QNetworkReply::NoError || !image.loadFromData(bytes) || image.width() < 32) {
                qInfo() << "No site icon for" << target << reply->errorString();
                // Site geeft geen bruikbaar pictogram (verbinding wel in orde): meegeleverd logo gebruiken
                if (reply->error() == QNetworkReply::NoError && QUrl(target).host().contains(QLatin1String("nlziet.nl"))) {
                    QFile::copy(QStringLiteral(":/assets/logos/nlziet.png"), file);
                    QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner);
                    ++m_revision;
                    Q_EMIT revisionChanged();
                    for (int row = 0; row < m_apps.size(); ++row) {
                        if (m_apps.at(row).path == target) {
                            Q_EMIT dataChanged(index(row), index(row), {IconRole, AccentRole});
                        }
                    }
                }
                return;
            }
            image.save(file, "PNG");
            for (int row = 0; row < m_apps.size(); ++row) {
                if (m_apps.at(row).path == target) {
                    Q_EMIT dataChanged(index(row), index(row), {IconRole, AccentRole});
                }
            }
            ++m_revision;
            Q_EMIT revisionChanged();
        });
    }
}

// Opent een site schermvullend in Edge (of Chrome) als app-venster zonder adresbalk.
// Een eigen ingebouwde webweergave valt af: Netflix e.d. vereisen DRM (Widevine/
// PlayReady) die alleen in de echte browsers zit. Eigen profielmap, zodat het
// venster altijd schermvullend start en inloggegevens bewaard blijven.
bool AppModel::launchWeb(const QString &url)
{
    QStringList candidates;
    const QString pf86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QString pf = qEnvironmentVariable("ProgramFiles");
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    for (const QString &base : {pf86, pf}) {
        candidates << base + QStringLiteral("/Microsoft/Edge/Application/msedge.exe");
    }
    for (const QString &base : {pf, pf86, local}) {
        candidates << base + QStringLiteral("/Google/Chrome/Application/chrome.exe");
    }
    for (const QString &browser : std::as_const(candidates)) {
        if (!QFile::exists(browser)) {
            continue;
        }
        // Nieuwssites: eigen profiel met grotere tekst (leesbaar vanaf de bank)
        const bool news = m_category == QLatin1String("nieuws");
        QStringList args = {
            QStringLiteral("--app=") + url,
            // Pijltjes springen tussen links en knoppen (bediening met afstandsbediening/controller)
            QStringLiteral("--enable-spatial-navigation"),
            QStringLiteral("--start-fullscreen"),
            // Geen schuifbalk rechts (scrollen met muiswiel, pijltjes of afstandsbediening blijft werken)
            QStringLiteral("--hide-scrollbars"),
            QStringLiteral("--enable-features=OverlayScrollbar,msOverlayScrollbarWinStyle,msOverlayScrollbarWinStyleAnimation"),
            QStringLiteral("--no-first-run"),
            QStringLiteral("--no-default-browser-check"),
            QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(dataDir() + (news ? QStringLiteral("/browser-nieuws") : QStringLiteral("/browser"))),
        };
        if (news) {
            args << QStringLiteral("--force-device-scale-factor=1.5");
        }
        // Eén dienst tegelijk: eerder geopende vensters (Ziggo, YouTube, Netflix…) eerst sluiten
        if (HomeBridge::closeOwnBrowserWindows() > 0) {
            // Even wachten tot het profiel vrij is, anders gaat het nieuwe adres naar het sluitende venster
            QTimer::singleShot(1200, this, [browser, args, url] {
                if (QProcess::startDetached(browser, args)) {
                    qInfo() << "Web tile" << url << "opened with" << browser;
                }
            });
            return true;
        }
        if (QProcess::startDetached(browser, args)) {
            qInfo() << "Web tile" << url << "opened with" << browser;
            return true;
        }
    }
    qInfo() << "No Edge/Chrome found; opening" << url << "in default browser";
    return QDesktopServices::openUrl(QUrl(url));
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_apps.size();
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size()) {
        return {};
    }
    const App &app = m_apps.at(index.row());
    switch (role) {
    case NameRole:
        return app.name;
    case PathRole:
        return app.path;
    case IconRole: {
        const QString custom = m_customIcons.value(app.path).toString();
        if (!custom.isEmpty()) {
            return custom;
        }
        if (app.path == QLatin1String("bigscreen:movies")) {
            return QStringLiteral("glyph:") + QChar(0xE8B2);
        }
        if (app.path == QLatin1String("bigscreen:iptv")) {
            return QStringLiteral("glyph:") + QChar(0xE714);
        }
        if (app.path == QLatin1String("bigscreen:frame")) {
            return QStringLiteral("glyph:") + QChar(0xEB9F);
        }
        if (app.path == QLatin1String("bigscreen:shows")) {
            return QStringLiteral("glyph:") + QChar(0xE7F4);
        }
        if (app.path == QLatin1String("bigscreen:resume")) {
            return QStringLiteral("glyph:") + QChar(0xE768);   // afspelen
        }
        if (app.path == QLatin1String("bigscreen:regional")) {
            return QStringLiteral("glyph:") + QChar(0xE707);   // kaartspeld
        }
        if (app.path == QLatin1String("bigscreen:radio-now")) {
            return m_nowPlayingIcon.isEmpty() ? QStringLiteral("glyph:") + QChar(0xE8D6) : m_nowPlayingIcon;
        }
        if (m_kind == Web) {
            const QString file = faviconFile(app.path);
            return QFile::exists(file) ? QUrl::fromLocalFile(file).toString() : QString();
        }
        return QString(QStringLiteral("image://appicon/")
                       + QString::fromLatin1(QUrl::toPercentEncoding(app.path)));
    }
    case CustomIconRole:
        return m_customIcons.contains(app.path);
    case AccentRole:
        return accentFor(app);
    }
    return {};
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    return {{NameRole, "name"}, {PathRole, "path"}, {IconRole, "iconSource"}, {CustomIconRole, "customIcon"}, {AccentRole, "accent"}};
}

void AppModel::refresh()
{
    if (m_kind == Web) {
        QList<App> tiles;
        for (const WebDefault &entry : webDefaults) {
            tiles.append({QString::fromUtf8(entry.name), QString::fromUtf8(entry.url), false, QString::fromUtf8(entry.category)});
        }
        tiles.append(customTiles());
        // Alleen de tegels van het gekozen tabblad (Video, Muziek, Podcasts)
        tiles.erase(std::remove_if(tiles.begin(), tiles.end(), [this](const App &tile) {
            return m_hidden.contains(tile.path) || tile.category != m_category;
        }), tiles.end());
        applyOrder(tiles);
        beginResetModel();
        m_apps = tiles;
        endResetModel();
        Q_EMIT countChanged();
        qInfo() << "Web tiles" << m_apps.size() << "hidden" << m_hidden.size();
        fetchFavicons();
        return;
    }

    // Media-rij: Films, Series, Live tv en programma's die de gebruiker zelf heeft
    // toegevoegd. (Versie 3.0-3.5 had hier ook Muziek, Video's, Afbeeldingen en
    // MPC-HC; tot en met versie 2 alle programma's uit het Startmenu.)
    QList<App> apps = {
        {tr("Films & Series"), QStringLiteral("bigscreen:movies"), false},
        {tr("Live tv"), QStringLiteral("bigscreen:iptv"), false},
        {tr("Regionale tv"), QStringLiteral("bigscreen:regional"), false},
        {tr("Schilderij"), QStringLiteral("bigscreen:frame"), false},
    };
    // Verder kijken: tegel naast Schilderij zolang er iets verder te kijken is
    if (m_resumeCount > 0) {
        apps.append({tr("Verder kijken"), QStringLiteral("bigscreen:resume"), false});
    }
    apps.append(customTiles());
    apps.erase(std::remove_if(apps.begin(), apps.end(), [this](const App &tile) {
        return m_hidden.contains(tile.path);
    }), apps.end());

    applyOrder(apps);
    if (!m_nowPlaying.isEmpty()) {
        apps.prepend({tr("Nu op de radio: %1").arg(m_nowPlaying), QStringLiteral("bigscreen:radio-now"), false});
    }
    beginResetModel();
    m_apps = apps;
    endResetModel();
    Q_EMIT countChanged();
    QStringList summary;
    for (const App &tile : std::as_const(m_apps)) {
        summary << tile.name + QStringLiteral(" -> ") + tile.path;
    }
    qInfo().noquote() << "Media tiles:" << summary.join(QStringLiteral(" | "));
}

bool AppModel::launch(int row)
{
    if (row < 0 || row >= m_apps.size()) {
        return false;
    }
    const App &app = m_apps.at(row);
    if (app.path.startsWith(QLatin1String("bigscreen:"))) {
        Q_EMIT internalRequested(app.path);
        return true;
    }
    const bool ok = m_kind == Web ? launchWeb(app.path)
                                  : QFileInfo::exists(app.path) && QDesktopServices::openUrl(QUrl::fromLocalFile(app.path));
    qInfo() << "Launch" << app.name << (ok ? "ok" : "FAILED");
    return ok;
}

bool AppModel::openUri(const QString &uri)
{
    const bool ok = QDesktopServices::openUrl(QUrl(uri));
    qInfo() << "Open" << uri << (ok ? "ok" : "FAILED");
    return ok;
}

QString AppModel::nameAt(int row) const
{
    return (row >= 0 && row < m_apps.size()) ? m_apps.at(row).name : QString();
}

QString AppModel::iconAt(int row) const
{
    return data(index(row), IconRole).toString();
}

QColor AppModel::accentAt(int row) const
{
    return (row >= 0 && row < m_apps.size()) ? accentFor(m_apps.at(row)) : QColor(QStringLiteral("#22252b"));
}

// Achtergrondkleur van een tegel: de overheersende kleur van het logo, zo donker
// gemaakt dat witte tekst erop leesbaar blijft. Toepassingen krijgen neutraal grijs.
QColor AppModel::accentFor(const App &app) const
{
    if (app.path == QLatin1String("bigscreen:radio-now") && !m_nowPlayingAccent.isEmpty()) {
        return QColor(m_nowPlayingAccent);
    }
    // Vaste, levendige kleuren voor de twee hoofdtegels (wit blijft goed leesbaar)
    if (app.path == QLatin1String("bigscreen:movies")) {
        return QColor(QStringLiteral("#8e24aa"));   // paars
    }
    if (app.path == QLatin1String("bigscreen:iptv")) {
        return QColor(QStringLiteral("#1976d2"));   // blauw
    }
    if (m_kind != Web) {
        return QColor(QStringLiteral("#22252b"));
    }
    QString file = QUrl(m_customIcons.value(app.path).toString()).toLocalFile();
    if (file.isEmpty()) {
        file = faviconFile(app.path);
    }
    const auto cached = m_accentCache.constFind(file);
    if (cached != m_accentCache.constEnd()) {
        return *cached;
    }

    // Zonder logo: vaste kleur per naam
    QColor result = QColor::fromHslF(((app.name.size() * 47 + app.name.at(0).unicode() * 13) % 360) / 360.0, 0.45, 0.32);
    const QImage source = QFile::exists(file) ? QImage(file) : QImage();
    if (!source.isNull()) {
        const QImage image = source.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_ARGB32);
        // Gemiddelde van de pixels, gewogen naar verzadiging: een rood logo op wit
        // geeft rood, geen roze. Is alles grijs/zwart/wit, dan blijft het donkergrijs.
        double r = 0, g = 0, b = 0, weight = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = QColor::fromRgba(image.pixel(x, y));
                if (pixel.alpha() < 128) {
                    continue;
                }
                const double w = pixel.hslSaturationF() * (1.0 - qAbs(pixel.lightnessF() - 0.5) * 2.0);
                r += pixel.redF() * w;
                g += pixel.greenF() * w;
                b += pixel.blueF() * w;
                weight += w;
            }
        }
        if (weight > 8.0) {
            const QColor mean = QColor::fromRgbF(r / weight, g / weight, b / weight);
            result = QColor::fromHslF(qMax(0.0f, mean.hslHueF()), qMin(0.75f, mean.hslSaturationF()),
                                      qBound(0.22f, mean.lightnessF(), 0.36f));
        } else {
            result = QColor(QStringLiteral("#2a2d33"));
        }
        m_accentCache.insert(file, result);
    }
    return result;
}

bool AppModel::hasCustomIcon(int row) const
{
    return row >= 0 && row < m_apps.size() && m_customIcons.contains(m_apps.at(row).path);
}

void AppModel::hide(int row)
{
    if (row < 0 || row >= m_apps.size()) {
        return;
    }
    const App tile = m_apps.at(row);
    if (tile.custom) {
        // Zelf toegevoegde tegel: echt weggooien in plaats van verbergen
        QList<App> own = customTiles();
        own.erase(std::remove_if(own.begin(), own.end(), [&tile](const App &t) { return t.path == tile.path; }), own.end());
        saveCustomTiles(own);
    } else {
        m_hidden.append(tile.path);
        m_settings.setValue(key("hiddenApps"), m_hidden);
    }
    beginRemoveRows({}, row, row);
    m_apps.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

bool AppModel::addTile(const QString &name, const QString &target)
{
    QString cleanName = name.trimmed();
    QString cleanTarget = target.trimmed();
    if (cleanTarget.startsWith(QLatin1Char('"')) && cleanTarget.endsWith(QLatin1Char('"'))) {
        cleanTarget = cleanTarget.mid(1, cleanTarget.size() - 2);
    }
    if (cleanTarget.isEmpty()) {
        return false;
    }
    if (m_kind == Web) {
        if (!cleanTarget.contains(QStringLiteral("://"))) {
            cleanTarget.prepend(QStringLiteral("https://"));
        }
        const QUrl url(cleanTarget);
        if (!url.isValid() || url.host().isEmpty()) {
            return false;
        }
        if (cleanName.isEmpty()) {
            cleanName = url.host();
        }
    } else {
        if (cleanTarget.startsWith(QStringLiteral("file:"))) {
            cleanTarget = QUrl(cleanTarget).toLocalFile();
        }
        const QFileInfo info(cleanTarget);
        if (!info.exists()) { // programma, snelkoppeling of map
            return false;
        }
        cleanTarget = QDir::toNativeSeparators(info.absoluteFilePath());
        if (cleanName.isEmpty()) {
            cleanName = info.completeBaseName();
        }
    }
    QList<App> own = customTiles();
    for (const App &tile : std::as_const(own)) {
        if (tile.path == cleanTarget) {
            return false;
        }
    }
    own.append({cleanName, cleanTarget, true, m_kind == Web ? m_category : QString()});
    saveCustomTiles(own);
    m_hidden.removeAll(cleanTarget);
    m_settings.setValue(key("hiddenApps"), m_hidden);
    refresh();
    qInfo() << "Tile added" << cleanName << cleanTarget;
    return true;
}

// Zet de tegels in de volgorde die de gebruiker heeft gesleept; nieuwe tegels achteraan.
void AppModel::applyOrder(QList<App> &tiles) const
{
    const QStringList order = m_settings.value(key("order")).toStringList();
    if (order.isEmpty()) {
        return;
    }
    // Verder kijken nog niet zelf verplaatst: direct na Schilderij
    const int frame = order.indexOf(QStringLiteral("bigscreen:frame"));
    auto rank = [&order, frame](const QString &path) -> double {
        const int i = order.indexOf(path);
        if (i >= 0) return i;
        if (path == QLatin1String("bigscreen:resume") && frame >= 0) return frame + 0.5;
        return 1e9;
    };
    std::stable_sort(tiles.begin(), tiles.end(), [&rank](const App &a, const App &b) {
        return rank(a.path) < rank(b.path);
    });
}

void AppModel::saveOrder()
{
    QStringList order;
    for (const App &tile : std::as_const(m_apps)) {
        if (tile.path != QLatin1String("bigscreen:radio-now")) {
            order << tile.path;
        }
    }
    // Tegels van andere tabbladen houden hun plek in de opgeslagen volgorde
    const QStringList previous = m_settings.value(key("order")).toStringList();
    for (const QString &path : previous) {
        if (!order.contains(path)) {
            order << path;
        }
    }
    m_settings.setValue(key("order"), order);
}

void AppModel::move(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= m_apps.size() || to >= m_apps.size()) {
        return;
    }
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_apps.move(from, to);
    endMoveRows();
    saveOrder();
}

void AppModel::restoreHidden()
{
    m_hidden.clear();
    m_settings.setValue(key("hiddenApps"), m_hidden);
    refresh();
}

// Kopieert de gekozen afbeelding naar de eigen datamap, zodat het pictogram of de
// achtergrond blijft werken als het origineel verplaatst of gewist wordt.
QString AppModel::importImage(const QUrl &source, const QString &baseName)
{
    const QString sourcePath = source.isLocalFile() ? source.toLocalFile() : source.toString();
    const QFileInfo info(sourcePath);
    if (!info.isFile() || QImageReader(sourcePath).format().isEmpty()) {
        qWarning() << "Not a readable image:" << sourcePath;
        return {};
    }
    const QString dir = dataDir() + QStringLiteral("/images");
    QDir().mkpath(dir);
    // Tijdstempel in de naam: anders toont QML de oude afbeelding uit zijn cache
    const QString target = QStringLiteral("%1/%2-%3.%4")
                               .arg(dir, baseName)
                               .arg(QDateTime::currentMSecsSinceEpoch())
                               .arg(info.suffix().toLower());
    if (!QFile::copy(sourcePath, target)) {
        qWarning() << "Could not copy" << sourcePath << "to" << target;
        return {};
    }
    return QUrl::fromLocalFile(target).toString();
}

bool AppModel::setCustomIcon(int row, const QUrl &imageFile)
{
    if (row < 0 || row >= m_apps.size()) {
        return false;
    }
    const QString path = m_apps.at(row).path;
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex().left(12));
    const QString imported = importImage(imageFile, QStringLiteral("icon-") + hash);
    if (imported.isEmpty()) {
        return false;
    }
    removeLocalFile(m_customIcons.value(path).toString());
    m_customIcons.insert(path, imported);
    m_settings.setValue(key("customIcons"), m_customIcons);
    Q_EMIT dataChanged(index(row), index(row), {IconRole, CustomIconRole, AccentRole});
    ++m_revision;
    Q_EMIT revisionChanged();
    return true;
}

void AppModel::clearCustomIcon(int row)
{
    if (!hasCustomIcon(row)) {
        return;
    }
    const QString path = m_apps.at(row).path;
    removeLocalFile(m_customIcons.take(path).toString());
    m_settings.setValue(key("customIcons"), m_customIcons);
    Q_EMIT dataChanged(index(row), index(row), {IconRole, CustomIconRole, AccentRole});
    ++m_revision;
    Q_EMIT revisionChanged();
}

bool AppModel::setBackground(const QUrl &imageFile)
{
    const QString imported = importImage(imageFile, QStringLiteral("background"));
    if (imported.isEmpty()) {
        return false;
    }
    removeLocalFile(m_background);
    m_background = imported;
    m_settings.setValue(QStringLiteral("background"), m_background);
    Q_EMIT backgroundChanged();
    return true;
}

void AppModel::clearBackground()
{
    removeLocalFile(m_background);
    m_background.clear();
    m_settings.setValue(QStringLiteral("background"), m_background);
    Q_EMIT backgroundChanged();
}

void AppModel::setNowPlaying(const QString &name, const QString &icon, const QString &accent)
{
    if (name == m_nowPlaying && icon == m_nowPlayingIcon) {
        return;
    }
    m_nowPlaying = name;
    m_nowPlayingIcon = icon;
    m_nowPlayingAccent = accent;
    refresh();
}

void AppModel::setResumeCount(int count)
{
    if ((count > 0) == (m_resumeCount > 0)) {
        m_resumeCount = count;
        return;
    }
    m_resumeCount = count;
    refresh();
}
