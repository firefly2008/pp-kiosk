// SPDX-License-Identifier: GPL-2.0-or-later
#include "artframe.h"
#include "apikeys.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
enum Source { PaintingsCommons, Met, NatureCommons, Pexels, Mix };

// Uitgelichte foto's van Wikimedia Commons (door de gemeenschap gekozen als de allerbeste)
// Schilderijen: door Commons uitgelicht, vaak in hoge resolutie gefotografeerd door de musea zelf
const char *const paintingCategories[] = {
    "Featured pictures of paintings",
    "Featured pictures of landscape paintings",
    "Featured pictures of Dutch Golden Age paintings",
    "Featured pictures of impressionist paintings",
    "Featured pictures of paintings by Vincent van Gogh",
    "Featured pictures of paintings by Claude Monet",
};

const char *const commonsCategories[] = {
    "Featured pictures of landscapes",
    "Featured pictures of coasts",
    "Featured pictures of islands",
    "Featured pictures of volcanoes",
    "Featured pictures of natural phenomena",
    "Featured pictures of snow",
    "Featured pictures of reflections",
    "Featured pictures of birds",
    "Featured pictures of mammals",
};

const char *const pexelsQueries[] = {
    "nature landscape", "mountains", "forest", "ocean", "lake", "waterfall", "autumn", "aurora", "desert", "wildlife",
};

QString stripHtml(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#039;"), QStringLiteral("'"));
    return text.simplified();
}

int randomInt(int max)
{
    return max <= 0 ? 0 : int(QRandomGenerator::global()->bounded(max));
}
}

ArtFrame::ArtFrame(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
    , m_cacheDir(dataDir + QStringLiteral("/frame-cache"))
{
    QDir().mkpath(m_cacheDir);
    m_source = qBound(0, m_settings.value(QStringLiteral("frame/source"), 0).toInt(), int(Mix));
    m_interval = qBound(1, m_settings.value(QStringLiteral("frame/interval"), 5).toInt(), 120);
    m_showInfo = m_settings.value(QStringLiteral("frame/showInfo"), true).toBool();
    m_fill = m_settings.value(QStringLiteral("frame/fill"), true).toBool();
    m_pexelsKey = m_settings.value(QStringLiteral("frame/pexelsKey")).toString();
}

QStringList ArtFrame::sources() const
{
    return {
        tr("Schilderijen – Wikimedia Commons"),
        tr("Topstukken – The Met, New York"),
        tr("Natuurfoto's – Wikimedia Commons"),
        tr("Natuurfoto's – Unsplash"),
        tr("Afwisselend kunst en natuur"),
    };
}

void ArtFrame::save()
{
    m_settings.setValue(QStringLiteral("frame/source"), m_source);
    m_settings.setValue(QStringLiteral("frame/interval"), m_interval);
    m_settings.setValue(QStringLiteral("frame/showInfo"), m_showInfo);
    m_settings.setValue(QStringLiteral("frame/fill"), m_fill);
    m_settings.setValue(QStringLiteral("frame/pexelsKey"), m_pexelsKey);
    Q_EMIT settingsChanged();
}

void ArtFrame::setSource(int source)
{
    source = qBound(0, source, int(Mix));
    if (source == m_source) {
        return;
    }
    m_source = source;
    save();
    // Nieuwe bron: wachtrij leeg en meteen iets nieuws laten zien
    ++m_generation;
    m_queue.clear();
    m_busy = false;
    m_retries = 0;
    m_failures = 0;
    m_downloading = false;
    next();
}

void ArtFrame::setInterval(int minutes)
{
    m_interval = qBound(1, minutes, 120);
    save();
}

void ArtFrame::setShowInfo(bool show)
{
    m_showInfo = show;
    save();
}

void ArtFrame::setFill(bool fill)
{
    m_fill = fill;
    save();
}

void ArtFrame::setPexelsKey(const QString &key)
{
    m_pexelsKey = key.trimmed();
    save();
    if (m_source == Pexels) {
        ++m_generation;
        m_queue.clear();
        m_busy = false;
        next();
    }
}

void ArtFrame::setStatus(const QString &status, bool busy)
{
    m_status = status;
    m_busy = busy;
    Q_EMIT statusChanged();
}

void ArtFrame::keepAwake(bool on)
{
#ifdef Q_OS_WIN
    SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS);
#else
    Q_UNUSED(on)
#endif
}

void ArtFrame::start()
{
    if (m_position < 0) {
        next();
    }
}

void ArtFrame::next()
{
    if (m_position + 1 < m_history.size()) {
        ++m_position;
        Q_EMIT currentChanged();
        return;
    }
    if (!m_queue.isEmpty()) {
        showNextCandidate();
    } else {
        m_waiting = true; // laten zien zodra er iets binnen is
    }
    if (m_queue.size() < 3 && !m_busy) {
        fetchBatch();
    }
}

void ArtFrame::previous()
{
    if (m_position > 0) {
        --m_position;
        Q_EMIT currentChanged();
    }
}

void ArtFrame::showNextCandidate()
{
    if (m_downloading || m_queue.isEmpty()) {
        return;
    }
    // Eerst zelf downloaden (met nette kopregels en een duidelijke foutmelding),
    // pas daarna tonen vanaf de schijf
    QVariantMap item = m_queue.takeFirst();
    const QString remote = item.value(QStringLiteral("image")).toString();
    const QString file = m_cacheDir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash(remote.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".jpg");
    auto show = [this, item, file]() mutable {
        item[QStringLiteral("image")] = QUrl::fromLocalFile(file).toString();
        m_history.append(item);
        while (m_history.size() > 50) {
            m_history.removeFirst();
        }
        m_position = m_history.size() - 1;
        Q_EMIT currentChanged();
    };
    if (QFileInfo(file).size() > 0) {
        show();
        return;
    }
    m_downloading = true;
    QNetworkRequest request{QUrl(remote)};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36 Kijkluister/3.8");
    request.setRawHeader("Accept", "image/avif,image/webp,image/jpeg,image/*;q=0.8,*/*;q=0.5");
    if (remote.contains(QLatin1String("artic.edu"))) {
        request.setRawHeader("Referer", "https://www.artic.edu/");
    }
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, show, generation]() mutable {
        reply->deleteLater();
        m_downloading = false;
        if (generation != m_generation) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || data.size() < 1000) {
            qWarning().noquote() << "Frame: image download failed" << status << reply->errorString() << reply->url().toString()
                                 << data.left(160);
            ++m_failures;
            if (m_failures > 6) {
                setStatus(tr("Afbeeldingen van deze bron kunnen nu niet worden geladen."), false);
                return;
            }
            showNextCandidate(); // volgende proberen
            if (m_queue.isEmpty() && !m_busy) {
                m_waiting = true;
                fetchBatch();
            }
            return;
        }
        m_failures = 0;
        QFile out(file);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
            out.close();
        }
        show();
        pruneCache();
    });
}

// Niet meer dan ~200 afbeeldingen bewaren
void ArtFrame::pruneCache()
{
    QDir dir(m_cacheDir);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.jpg")}, QDir::Files, QDir::Time);
    for (int i = 200; i < files.size(); ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

void ArtFrame::addCandidates(QList<QVariantMap> items)
{
    // Door elkaar, zodat het niet steeds dezelfde volgorde is
    for (int i = items.size() - 1; i > 0; --i) {
        items.swapItemsAt(i, randomInt(i + 1));
    }
    m_queue.append(items);
    if (m_queue.isEmpty()) {
        // Deze zoekopdracht leverde niets bruikbaars op: nog een paar keer een andere proberen
        if (++m_retries <= 4) {
            fetchBatch();
        } else {
            setStatus(tr("Geen afbeeldingen gevonden. Controleer de internetverbinding."), false);
        }
        return;
    }
    m_retries = 0;
    setStatus(QString(), false);
    if (m_waiting) {
        m_waiting = false;
        showNextCandidate();
    }
}

void ArtFrame::get(const QUrl &url, const Callback &callback, const QByteArray &auth)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Kijkluister/3.8 (https://github.com/firefly2008/bigscreen-host-windows)"));
    if (!auth.isEmpty()) {
        request.setRawHeader("Authorization", auth);
    }
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, generation] {
        reply->deleteLater();
        if (generation != m_generation) {
            return; // intussen andere bron gekozen
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            qWarning().noquote() << "Frame: request failed" << reply->url().host() << status << reply->errorString()
                                 << data.left(200);
        }
        callback(data, status);
    });
}

void ArtFrame::fetchBatch()
{
    setStatus(tr("Afbeeldingen zoeken…"), true);
    int source = m_source;
    if (source == Mix) {
        const int choices[] = {PaintingsCommons, Met, NatureCommons};
        source = choices[randomInt(3)];
    }
    switch (source) {
    case PaintingsCommons:
        fetchCommons(true);
        break;
    case Met:
        fetchMet();
        break;
    case NatureCommons:
        fetchCommons(false);
        break;
    case Pexels:
        fetchPexels();
        break;
    }
}

// The Met: uitgelichte schilderijen met open toegang
void ArtFrame::fetchMet()
{
    if (!m_metIds.isEmpty()) {
        fetchMetObject();
        return;
    }
    // Sinds oktober 2026: /v1.1/search, in pagina's (offset/limit)
    QUrl url(QStringLiteral("https://collectionapi.metmuseum.org/public/collection/v1.1/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), QStringLiteral("painting"));
    query.addQueryItem(QStringLiteral("isHighlight"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("hasImages"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("medium"), QStringLiteral("Paintings"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("offset"), QString::number(100 * randomInt(3)));
    url.setQuery(query);
    get(url, [this](const QByteArray &data, int) {
        // Antwoordvorm ruim lezen: een lijst met nummers, of objecten met een objectID
        const QJsonObject root = QJsonDocument::fromJson(data).object();
        for (const char *key : {"objectIDs", "results", "data", "objects", "items"}) {
            for (const QJsonValue &v : root.value(QLatin1String(key)).toArray()) {
                const int id = v.isObject() ? v.toObject().value(QStringLiteral("objectID")).toInt() : v.toInt();
                if (id > 0) {
                    m_metIds.append(id);
                }
            }
        }
        qInfo() << "Frame: Met gave" << m_metIds.size() << "objects" << (m_metIds.isEmpty() ? data.left(300) : QByteArray());
        if (m_metIds.isEmpty()) {
            setStatus(tr("The Met is niet bereikbaar"), false);
            return;
        }
        fetchMetObject();
    });
}

void ArtFrame::fetchMetObject()
{
    // Per object een aanvraag; een paar tegelijk ophalen is genoeg voor de wachtrij
    auto pending = std::make_shared<int>(4);
    auto found = std::make_shared<QList<QVariantMap>>();
    for (int i = 0; i < 4; ++i) {
        const int id = m_metIds.at(randomInt(m_metIds.size()));
        get(QUrl(QStringLiteral("https://collectionapi.metmuseum.org/public/collection/v1/objects/%1").arg(id)),
            [this, pending, found](const QByteArray &data, int) {
                const QJsonObject o = QJsonDocument::fromJson(data).object();
                const QString image = o.value(QStringLiteral("primaryImage")).toString();
                if (!image.isEmpty() && o.value(QStringLiteral("isPublicDomain")).toBool()) {
                    QString subtitle = o.value(QStringLiteral("artistDisplayName")).toString();
                    const QString date = o.value(QStringLiteral("objectDate")).toString();
                    if (!date.isEmpty()) {
                        subtitle += (subtitle.isEmpty() ? QString() : QStringLiteral(", ")) + date;
                    }
                    found->append({
                        {QStringLiteral("image"), image},
                        {QStringLiteral("title"), o.value(QStringLiteral("title")).toString()},
                        {QStringLiteral("subtitle"), subtitle},
                        {QStringLiteral("credit"), tr("The Metropolitan Museum of Art · open access")},
                    });
                }
                if (--*pending == 0) {
                    addCandidates(*found);
                }
            });
    }
}

// Wikimedia Commons: uitgelichte natuurfoto's (Featured pictures), liggend formaat
void ArtFrame::fetchCommons(bool paintings)
{
    const QString category = paintings
        ? QString::fromLatin1(paintingCategories[randomInt(int(std::size(paintingCategories)))])
        : QString::fromLatin1(commonsCategories[randomInt(int(std::size(commonsCategories)))]);
    QUrl url(QStringLiteral("https://commons.wikimedia.org/w/api.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("generator"), QStringLiteral("categorymembers"));
    query.addQueryItem(QStringLiteral("gcmtitle"), QStringLiteral("Category:") + category);
    query.addQueryItem(QStringLiteral("gcmtype"), QStringLiteral("file"));
    query.addQueryItem(QStringLiteral("gcmlimit"), QStringLiteral("100"));
    // Willekeurig ergens in het alfabet beginnen, zodat het niet steeds dezelfde 100 zijn
    query.addQueryItem(QStringLiteral("gcmsort"), QStringLiteral("sortkey"));
    query.addQueryItem(QStringLiteral("gcmstartsortkeyprefix"), QString(QChar(u'A' + randomInt(26))));
    query.addQueryItem(QStringLiteral("prop"), QStringLiteral("imageinfo"));
    query.addQueryItem(QStringLiteral("iiprop"), QStringLiteral("url|size|mime|extmetadata"));
    query.addQueryItem(QStringLiteral("iiurlwidth"), QStringLiteral("1920"));
    query.addQueryItem(QStringLiteral("iiextmetadatafilter"), QStringLiteral("Artist|ObjectName|LicenseShortName"));
    url.setQuery(query);
    get(url, [this, paintings](const QByteArray &data, int) {
        const QJsonObject pages = QJsonDocument::fromJson(data).object().value(QStringLiteral("query")).toObject()
                                      .value(QStringLiteral("pages")).toObject();
        QList<QVariantMap> items;
        for (const QJsonValue &page : pages) {
            const QJsonObject info = page.toObject().value(QStringLiteral("imageinfo")).toArray().first().toObject();
            const double w = info.value(QStringLiteral("width")).toDouble();
            const double h = info.value(QStringLiteral("height")).toDouble();
            const QString thumb = info.value(QStringLiteral("thumburl")).toString();
            const QString mime = info.value(QStringLiteral("mime")).toString();
            // Liggend (schilderijen mogen iets minder breed zijn), en een foto of scan
            if (thumb.isEmpty() || h <= 0 || w / h < (paintings ? 1.1 : 1.25)
                || (mime != QLatin1String("image/jpeg") && mime != QLatin1String("image/png") && mime != QLatin1String("image/tiff"))) {
                continue;
            }
            const QJsonObject meta = info.value(QStringLiteral("extmetadata")).toObject();
            auto field = [&meta](const char *name) {
                return stripHtml(meta.value(QLatin1String(name)).toObject().value(QStringLiteral("value")).toString());
            };
            QString title = field("ObjectName");
            if (title.isEmpty()) {
                title = page.toObject().value(QStringLiteral("title")).toString().section(QLatin1Char(':'), 1).section(QLatin1Char('.'), 0, -2);
            }
            items.append({
                {QStringLiteral("image"), thumb},
                {QStringLiteral("title"), title},
                {QStringLiteral("subtitle"), field("Artist").left(80)},
                {QStringLiteral("credit"), tr("Wikimedia Commons · %1").arg(field("LicenseShortName"))},
                {QStringLiteral("kind"), paintings ? QStringLiteral("painting") : QStringLiteral("photo")},
            });
        }
        qInfo() << "Frame: Commons" << (paintings ? "paintings" : "nature") << "gave" << items.size()
                << "of" << pages.size() << (items.isEmpty() && !pages.isEmpty() ? QJsonDocument(pages.begin()->toObject()).toJson(QJsonDocument::Compact).left(600) : QByteArray());
        addCandidates(items);
    });
}

// Unsplash: gratis natuurfoto's met de eigen sleutel van de gebruiker (Instellingen → Sleutels)
void ArtFrame::fetchPexels()
{
    const QString key = (ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString());
    if (key.isEmpty()) {
        setStatus(tr("Unsplash heeft een eigen sleutel nodig: Instellingen → Sleutels"), false);
        return;
    }
    QUrl url(QStringLiteral("https://api.unsplash.com/search/photos"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), QString::fromLatin1(pexelsQueries[randomInt(int(std::size(pexelsQueries)))]));
    query.addQueryItem(QStringLiteral("orientation"), QStringLiteral("landscape"));
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("30"));
    query.addQueryItem(QStringLiteral("page"), QString::number(1 + randomInt(5)));
    url.setQuery(query);
    get(url, [this](const QByteArray &data, int status) {
        if (status == 401 || status == 403) {
            setStatus(tr("Unsplash weigert de sleutel"), false);
            return;
        }
        QList<QVariantMap> items;
        for (const QJsonValue &v : QJsonDocument::fromJson(data).object().value(QStringLiteral("results")).toArray()) {
            const QJsonObject o = v.toObject();
            const QString raw = o.value(QStringLiteral("urls")).toObject().value(QStringLiteral("raw")).toString();
            if (raw.isEmpty()) {
                continue;
            }
            items.append({
                {QStringLiteral("image"), raw + QStringLiteral("&w=1920&fm=jpg&q=85")},
                {QStringLiteral("title"), o.value(QStringLiteral("alt_description")).toString()},
                {QStringLiteral("subtitle"), tr("Foto: %1").arg(o.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")).toString())},
                {QStringLiteral("credit"), tr("Foto's van Unsplash")},
            });
        }
        qInfo() << "Frame: Unsplash gave" << items.size() << "photos";
        addCandidates(items);
    }, "Client-ID " + key.toLatin1());
}
