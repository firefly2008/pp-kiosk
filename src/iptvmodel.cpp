// SPDX-License-Identifier: GPL-2.0-or-later
#include "mpcplayer.h"
#include "iptvmodel.h"
#include "securestore.h"
#include "appmodel.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <QLocale>
#include <QTimeZone>
#include <algorithm>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentRun>

#include <zlib.h>

namespace {

const QString favoritesGroup = QStringLiteral("Favorieten");
const QString allGroup = QStringLiteral("Alle zenders");
const QString regionalGroup = QStringLiteral("Regionale tv");
const QString otherGroup = QStringLiteral("Overig");
const QString nlGroup = QStringLiteral("NPO & RTL");
const QString webPrefix = QStringLiteral("web:");

// NPO- en RTL-zenders: live kijken kan alleen op hun eigen site (beveiligde streams),
// de tv-gids (nu/straks en omschrijvingen) halen we wel zelf op
struct NlChannel { const char *name; const char *page; const char *epg; };
const NlChannel nlChannels[] = {
    {"NPO 1", "https://npo.nl/start/live/npo-1", "NPO.1.nl"},
    {"NPO 2", "https://npo.nl/start/live/npo-2", "NPO.2.nl"},
    {"NPO 3", "https://npo.nl/start/live/npo-3", "NPO.3.nl"},
    {"NPO 1 Extra", "https://npo.nl/start/live", "NPO.1.Extra.nl"},
    {"NPO 2 Extra", "https://npo.nl/start/live", "NPO.2.Extra.nl"},
    {"NPO Politiek en Nieuws", "https://npo.nl/start/live", "NPO.Politiek.en.Nieuws.nl"},
    {"RTL 4", "https://www.rtl.nl/", "RTL.4.nl"},
    {"RTL 5", "https://www.rtl.nl/", "RTL.5.nl"},
    {"RTL 7", "https://www.rtl.nl/", "RTL.7.nl"},
    {"RTL 8", "https://www.rtl.nl/", "RTL.8.nl"},
    {"RTL Z", "https://www.rtl.nl/", "RTL.Z.nl"},
};

// Zelfde kenmerken als open-tv (src-tauri/src/m3u.rs)
const QRegularExpression nameAttr(QStringLiteral(R"re(tvg-name="([^"]*)")re"));
const QRegularExpression idAttr(QStringLiteral(R"re(tvg-id="([^"]*)")re"));
const QRegularExpression logoAttr(QStringLiteral(R"re(tvg-logo="([^"]*)")re"));
const QRegularExpression groupAttr(QStringLiteral(R"re(group-title="([^"]*)")re"));
// Verwijzing naar de gids in de kop van de lijst: #EXTM3U url-tvg="..." of x-tvg-url="..."
const QRegularExpression epgHeader(QStringLiteral(R"re((?:url-tvg|x-tvg-url)="([^"]*)")re"));

// Link van een Xtream-paneel (…/get.php?username=…&password=…): server en inloggegevens eruit halen,
// zodat ook bij een M3U-link de gids van de aanbieder (xmltv.php en per zender) gebruikt kan worden
struct XtreamLogin {
    QString server;
    QString user;
    QString password;
    bool valid() const { return !server.isEmpty() && !user.isEmpty() && !password.isEmpty(); }
};
XtreamLogin xtreamLoginFor(const QVariantMap &source)
{
    XtreamLogin login;
    if (source.value(QStringLiteral("type")).toString() == QLatin1String("xtream")) {
        login.server = source.value(QStringLiteral("url")).toString();
        login.user = source.value(QStringLiteral("user")).toString();
        login.password = source.value(QStringLiteral("password")).toString();
        return login;
    }
    const QUrl url(source.value(QStringLiteral("url")).toString());
    if (!url.path().endsWith(QLatin1String("get.php"), Qt::CaseInsensitive)) {
        return login;
    }
    const QUrlQuery query(url);
    login.user = query.queryItemValue(QStringLiteral("username"), QUrl::FullyDecoded);
    login.password = query.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded);
    QUrl server = url.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    login.server = server.toString();
    return login;
}

// .xml.gz-gidsen uitpakken (gzip)
QByteArray gunzip(const QByteArray &data)
{
    if (data.size() < 2 || uchar(data[0]) != 0x1f || uchar(data[1]) != 0x8b) {
        return data;
    }
    z_stream stream{};
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return {};
    }
    QByteArray out;
    char buffer[65536];
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = uInt(data.size());
    int result = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            break;
        }
        out.append(buffer, int(sizeof(buffer) - stream.avail_out));
    } while (result != Z_STREAM_END);
    inflateEnd(&stream);
    return out;
}

// XMLTV-tijd: "20261113120000 +0100"
qint64 xmltvTime(const QString &value)
{
    QDateTime time = QDateTime::fromString(value.left(14), QStringLiteral("yyyyMMddHHmmss"));
    time.setTimeZone(QTimeZone::UTC);
    const QString zone = value.mid(14).trimmed();
    if (zone.size() == 5) {
        const int sign = zone.startsWith(QLatin1Char('-')) ? -1 : 1;
        const int offset = sign * (zone.mid(1, 2).toInt() * 3600 + zone.mid(3, 2).toInt() * 60);
        return time.toSecsSinceEpoch() - offset;
    }
    return time.toSecsSinceEpoch();
}

QString capture(const QRegularExpression &re, const QString &line)
{
    return re.match(line).captured(1).trimmed();
}

QString serverUrl(QString url)
{
    url = url.trimmed();
    if (!url.contains(QStringLiteral("://"))) {
        url.prepend(QStringLiteral("http://"));
    }
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    // Iemand plakt soms het hele player_api-adres: alleen de server bewaren
    const int api = url.indexOf(QStringLiteral("/player_api.php"));
    if (api > 0) {
        url = url.left(api);
    }
    return url;
}

QNetworkRequest request(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Kijkluister"));
    req.setTransferTimeout(60000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

} // namespace

IptvModel::IptvModel(const QString &dataDir, QObject *parent)
    : QAbstractListModel(parent)
    , m_dataDir(dataDir)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
{
    m_sources = SecureStore::value(m_settings, QStringLiteral("iptv/sources")).toList();
    const QStringList favorites = m_settings.value(QStringLiteral("iptv/favorites")).toStringList();
    m_favorites = QSet<QString>(favorites.begin(), favorites.end());
    m_group = m_favorites.isEmpty() ? allGroup : favoritesGroup;
    loadCache();
    rebuildGroups();
    applyFilter();
    m_epgUrls = SecureStore::value(m_settings, QStringLiteral("iptv/epgUrls")).toStringList();
    m_extraEpgUrls = m_settings.value(QStringLiteral("iptv/extraEpg")).toStringList();
    m_epgUrls.removeIf([](const QString &u) { return u.contains(QLatin1String("firefly2008/bigscreen-host-windows")); });
    // Bestaande installaties met gratis zenders: gratis gids er meteen bij
    for (const QVariant &v : std::as_const(m_sources)) {
        if (v.toMap().value(QStringLiteral("free")).toBool() && !m_epgUrls.contains(freeGuideUrl())) {
            m_epgUrls << freeGuideUrl();
        }
    }
    for (const QString &u : std::as_const(m_extraEpgUrls)) {
        if (!m_epgUrls.contains(u)) {
            m_epgUrls << u;
        }
    }
    if (!m_epgUrls.contains(nlGuideUrl())) {
        m_epgUrls << nlGuideUrl();
    }
    m_reminders = m_settings.value(QStringLiteral("iptv/reminders")).toList();
    m_seriesRules = m_settings.value(QStringLiteral("iptv/series")).toList();
    // Herinneringen en series: elke halve minuut kijken
    auto *tick = new QTimer(this);
    tick->setInterval(30000);
    connect(tick, &QTimer::timeout, this, &IptvModel::checkReminders);
    tick->start();
    connect(this, &IptvModel::epgChanged, this, [this] {
        static qint64 lastPlan = 0;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (!m_seriesRules.isEmpty() && now - lastPlan > 600) {
            lastPlan = now;
            QTimer::singleShot(0, this, &IptvModel::planSeries);
        }
    });
    // 4.3.1: M3U-links van een Xtream-paneel krijgen nu ook de gids; eenmalig de lijst opnieuw laden
    if (!m_settings.value(QStringLiteral("iptv/xtreamLinkGuide")).toBool()) {
        m_settings.setValue(QStringLiteral("iptv/xtreamLinkGuide"), true);
        for (const QVariant &v : std::as_const(m_sources)) {
            const QVariantMap source = v.toMap();
            if (source.value(QStringLiteral("type")).toString() != QLatin1String("xtream") && xtreamLoginFor(source).valid()) {
                QTimer::singleShot(3000, this, &IptvModel::reload);
                break;
            }
        }
    }
    connect(&m_epgWatcher, &QFutureWatcher<EpgData>::finished, this, [this] {
        m_epg = m_epgWatcher.result();
        rebuildEpgIndex();
        m_epgBusy = false;
        saveEpgCache();
        ++m_epgRevision;
        m_epgStatus = m_epg.programmes.isEmpty() ? tr("Geen gids gevonden") : tr("Gids: %1 zenders").arg(m_epg.programmes.size());
        qInfo() << "EPG ready:" << m_epg.programmes.size() << "channels";
        Q_EMIT epgChanged();
    });
    // Gids uit het geheugen van de vorige keer; is die ouder dan 6 uur, opnieuw ophalen
    if (!loadEpgCache() && !m_all.isEmpty()) {
        QMetaObject::invokeMethod(this, &IptvModel::reloadEpg, Qt::QueuedConnection);
    }
    setStatus(m_all.isEmpty() ? QString() : tr("%1 zenders").arg(m_all.size()), false);
}

int IptvModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant IptvModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) {
        return {};
    }
    const Channel &c = m_all.at(m_visible.at(index.row()));
    switch (role) {
    case NameRole:
        return m_favorites.contains(c.url) ? QStringLiteral("★ ") + c.name : c.name;
    case IconRole:
        return c.logo;
    case AccentRole:
        return QColor(QStringLiteral("#22252b"));
    case GroupRole:
        return c.group;
    case UrlRole:
        return c.url;
    case FavoriteRole:
        return m_favorites.contains(c.url);
    case NumberRole:
        return index.row() + 1;
    }
    return {};
}

QHash<int, QByteArray> IptvModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "iconSource"}, {AccentRole, "accent"}, {GroupRole, "group"},
            {UrlRole, "url"}, {FavoriteRole, "favorite"}, {NumberRole, "number"}};
}

void IptvModel::setGroup(const QString &group)
{
    m_groupChosen = true;
    if (group == m_group) {
        return;
    }
    m_group = group;
    applyFilter();
    Q_EMIT filterChanged();
}

void IptvModel::setSearch(const QString &search)
{
    if (search == m_search) {
        return;
    }
    m_search = search;
    applyFilter();
    Q_EMIT filterChanged();
}

void IptvModel::applyFilter()
{
    const QString needle = m_search.trimmed();
    QList<int> visible;
    for (int i = 0; i < m_all.size(); ++i) {
        const Channel &c = m_all.at(i);
        if (!needle.isEmpty()) {
            // Zoeken gaat altijd over alle zenders
            if (!c.name.contains(needle, Qt::CaseInsensitive)) {
                continue;
            }
        } else if (m_group == favoritesGroup) {
            if (!m_favorites.contains(c.url)) {
                continue;
            }
        } else if (m_group != allGroup && c.group != m_group) {
            continue;
        }
        visible.append(i);
    }
    beginResetModel();
    m_visible = visible;
    endResetModel();
    Q_EMIT countChanged();
}

namespace {
// Regionale omroepen van Nederland (vrije streams van de omroepen zelf; lijst via iptv-org,
// getest november 2026). Altijd aanwezig, ook zonder eigen IPTV-bron: tegel "Regionale tv".
struct Regional {
    const char *name;
    const char *url;
    const char *logo;
    const char *epg;
};
const Regional regionals[] = {
    {"NH", "https://takeoff.jetstre.am/?account=nhnieuws&file=live&output=playlist.m3u8&protocol=https&service=wowza&type=live", "https://i.imgur.com/SPkK4md.png", "NH.nl"},
    {"AT5", "https://takeoff.jetstre.am/?account=atvijf&file=live&output=playlist.m3u8&protocol=https&service=wowza&type=live", "https://i.imgur.com/iAiSlEL.png", "AT5.nl"},
    {"RTV Utrecht", "https://d18rwjdhpr8dcw.cloudfront.net/live/rtvutrecht/rtvutrecht/index.m3u8", "https://i.imgur.com/nrvs30l.png", "RTVUtrecht.nl"},
    {"Omroep Gelderland", "https://d2od87akyl46nm.cloudfront.net/live/omroepgelderland/tvgelderland-hls/index.m3u8", "https://i.imgur.com/G08hKit.png", "OmroepGelderland.nl"},
    {"Omroep Brabant", "https://d3slqp9xhts6qb.cloudfront.net/live/omroepbrabant/tv/index.m3u8", "https://i.imgur.com/KLYsKKC.png", "OmroepBrabant.nl"},
    {"L1 (Limburg)", "https://d34pj260kw1xmk.cloudfront.net/live/l1/tv/index.m3u8", "https://i.imgur.com/SDD5KS5.png", "L1TV.nl"},
    {"RTV Oost", "https://d34cg2bnc08ruf.cloudfront.net/live/rtvoost/tv/index.m3u8", "https://i.imgur.com/nTR6q9H.png", "RTVOost.nl"},
    {"RTV Noord", "https://media.rtvnoord.nl/live/rtvnoord/tv/index.m3u8", "https://i.imgur.com/l92Cb6H.png", "RTVNoord.nl"},
    {"Omrop Fryslân", "https://s-d15f8b0b.b.cdn.bluebillywig.com/live/omropfryslan/f8f68bd5/index.m3u8", "https://i.imgur.com/khacDTU.png", "OmropFryslan.nl"},
    {"RTV Drenthe", "https://cdn.rtvdrenthe.nl/live/rtvdrenthe/tv/index.m3u8", "https://i.imgur.com/UBQCjnG.png", "RTVDrenthe.nl"},
    {"Omroep Flevoland", "https://d5ms27yy6exnf.cloudfront.net/live/omroepflevoland/tv/index.m3u8", "https://i.imgur.com/rF4MTYD.png", "OmroepFlevoland.nl"},
    {"Rijnmond", "https://dcur8bjarl5c2.cloudfront.net/live/rijnmond/tv/index.m3u8", "https://i.imgur.com/fJTs9xH.png", "RTVRijnmond.nl"},
    {"Omroep West", "https://d1axml5ozykh3g.cloudfront.net/live/omroepwest/tv/index.m3u8", "https://i.imgur.com/5IDb9fk.png", "OmroepWest.nl"},
    {"Omroep Zeeland", "https://d3isaxd2t6q8zm.cloudfront.net/live/omroepzeeland/tv/index.m3u8", "https://i.imgur.com/3c3QUSn.png", "OmroepZeeland.nl"},
    {"Rijnmond Extra", "https://dcur8bjarl5c2.cloudfront.net/live/rijnmond/tv-extra/index.m3u8", "https://i.imgur.com/mlrd5ia.png", "RTVRijnmondExtra.nl"},
    {"RTV Noord Extra", "https://media.rtvnoord.nl/live/rtvnoord/extra/index.m3u8", "https://i.imgur.com/T0UvltB.png", "RTVNoordExtra.nl"},
    {"Omroep Brabant Radio (beeld)", "https://d3slqp9xhts6qb.cloudfront.net/live/omroepbrabant/visualradio/index.m3u8", "https://i.imgur.com/KLYsKKC.png", "OmroepBrabantRadio.nl"},
};
}

void IptvModel::rebuildGroups()
{
    // Regionale zenders altijd in hun eigen groep
    m_all.erase(std::remove_if(m_all.begin(), m_all.end(), [](const Channel &c) { return c.group == regionalGroup; }), m_all.end());
    QList<Channel> regional;
    for (const Regional &r : regionals) {
        regional.append({QString::fromUtf8(r.name), QString::fromUtf8(r.logo), regionalGroup, QString::fromUtf8(r.url),
                         QString::fromUtf8(r.epg), -1, QString()});
    }
    m_all.append(regional);   // achteraan: in "Alle zenders" blijven je eigen zenders vooraan
    m_all.erase(std::remove_if(m_all.begin(), m_all.end(), [](const Channel &c) { return c.group == nlGroup; }), m_all.end());
    for (const NlChannel &n : nlChannels) {
        m_all.append({QString::fromUtf8(n.name), QString(), nlGroup, webPrefix + QString::fromUtf8(n.page),
                      QString::fromUtf8(n.epg), -1, QString()});
    }

    QStringList groups{favoritesGroup, allGroup};
    QSet<QString> seen;
    for (const Channel &c : std::as_const(m_all)) {
        if (!seen.contains(c.group)) {
            seen.insert(c.group);
            groups.append(c.group); // volgorde van de aanbieder aanhouden
        }
    }
    // Regionale tv als eerste groep na Alle zenders
    groups.removeAll(regionalGroup);
    groups.insert(2, regionalGroup);
    groups.removeAll(nlGroup);
    groups.insert(2, nlGroup);
    m_groups = groups;
    // Zolang je zelf nog niets kiest: begin bij de Nederlandse zenders (niet bij ruim 1.000 zenders)
    if (!m_groupChosen && m_group == allGroup && m_groups.contains(QStringLiteral("Nederland"))) {
        m_group = QStringLiteral("Nederland");
    }
    if (!m_groups.contains(m_group)) {
        m_group = allGroup;
    }
}

namespace {
struct FreeList {
    const char *name;
    const char *url;
    bool news;   // internationaal nieuws: alleen op verzoek (ruim 1.000 zenders)
};
// iptv-org verzamelt alleen vrij en openbaar beschikbare streams van de zenders zelf
const FreeList freeLists[] = {
    {"Nederland", "https://iptv-org.github.io/iptv/countries/nl.m3u", false},
    {"België", "https://iptv-org.github.io/iptv/countries/be.m3u", false},
    {"Nieuws (internationaal)", "https://iptv-org.github.io/iptv/categories/news.m3u", true},
};
}

int IptvModel::addFreeSources(bool includeNews)
{
    QStringList present;
    for (const QVariant &v : std::as_const(m_sources)) {
        present << v.toMap().value(QStringLiteral("url")).toString();
    }
    int added = 0;
    for (const FreeList &list : freeLists) {
        const QString url = QString::fromLatin1(list.url);
        if (present.contains(url) || (list.news && !includeNews)) {
            continue;
        }
        const QString name = QString::fromUtf8(list.name);
        m_sources.append(QVariantMap{
            {QStringLiteral("type"), QStringLiteral("m3u-url")},
            {QStringLiteral("name"), tr("Gratis: %1").arg(name)},
            {QStringLiteral("url"), url},
            {QStringLiteral("group"), name},
            {QStringLiteral("free"), true},
        });
        ++added;
    }
    if (added > 0) {
        SecureStore::setValue(m_settings, QStringLiteral("iptv/sources"), m_sources);
        m_settings.sync();
        Q_EMIT sourcesChanged();
        reload();
    }
    qInfo() << "IPTV: free sources added:" << added;
    return added;
}

// Eén keer, ook bij een bestaande installatie: daarna beslist de gebruiker zelf
void IptvModel::addFreeSourcesOnce()
{
    if (m_settings.value(QStringLiteral("iptv/freeSourcesAdded")).toBool()) {
        return;
    }
    m_settings.setValue(QStringLiteral("iptv/freeSourcesAdded"), true);
    addFreeSources();
}

bool IptvModel::addSource(const QString &type, const QString &name, const QString &url, const QString &user, const QString &password)
{
    QString target = url.trimmed();
    if (type == QLatin1String("m3u-file") && target.startsWith(QLatin1String("file:"))) {
        target = QUrl(target).toLocalFile();
    }
    if (target.isEmpty() || (type == QLatin1String("xtream") && (user.trimmed().isEmpty() || password.isEmpty()))) {
        return false;
    }
    if (type == QLatin1String("m3u-url") && !target.contains(QStringLiteral("://"))) {
        target.prepend(QStringLiteral("http://"));
    }
    if (type == QLatin1String("m3u-file") && !QFileInfo::exists(target)) {
        return false;
    }
    if (type == QLatin1String("xtream")) {
        target = serverUrl(target);
    }
    QVariantMap source{
        {QStringLiteral("type"), type},
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("url"), target},
        {QStringLiteral("user"), user.trimmed()},
        {QStringLiteral("password"), password},
    };
    if (source.value(QStringLiteral("name")).toString().isEmpty()) {
        source[QStringLiteral("name")] = sourceLabel(source);
    }
    m_sources.append(source);
    SecureStore::setValue(m_settings, QStringLiteral("iptv/sources"), m_sources);
    m_settings.sync();
    Q_EMIT sourcesChanged();
    reload();
    return true;
}

void IptvModel::removeSource(int index)
{
    if (index < 0 || index >= m_sources.size()) {
        return;
    }
    m_sources.removeAt(index);
    SecureStore::setValue(m_settings, QStringLiteral("iptv/sources"), m_sources);
    Q_EMIT sourcesChanged();
    reload();
}

QString IptvModel::sourceLabel(const QVariantMap &source) const
{
    const QString type = source.value(QStringLiteral("type")).toString();
    const QString url = source.value(QStringLiteral("url")).toString();
    if (type == QLatin1String("m3u-file")) {
        return QFileInfo(url).completeBaseName();
    }
    return QUrl(url).host();
}

void IptvModel::reload()
{
    if (m_busy) {
        return;
    }
    m_loading.clear();
    m_errors.clear();
    m_loadingEpgUrls.clear();
    m_loadingEpgUrls << m_extraEpgUrls;
    m_loadIndex = -1;
    if (m_sources.isEmpty()) {
        finishLoading();
        return;
    }
    setStatus(tr("Zenders laden…"), true);
    loadNext();
}

void IptvModel::loadNext()
{
    ++m_loadIndex;
    if (m_loadIndex >= m_sources.size()) {
        finishLoading();
        return;
    }
    const QVariantMap source = m_sources.at(m_loadIndex).toMap();
    const QString type = source.value(QStringLiteral("type")).toString();
    const QString name = source.value(QStringLiteral("name")).toString();
    setStatus(tr("Zenders laden: %1").arg(name), true);

    if (type == QLatin1String("xtream")) {
        loadXtream(source);
        return;
    }
    if (type == QLatin1String("m3u-file")) {
        QFile file(source.value(QStringLiteral("url")).toString());
        if (file.open(QIODevice::ReadOnly)) {
            parseM3u(file.readAll());
        } else {
            m_errors << tr("%1: bestand niet te openen").arg(name);
        }
        QMetaObject::invokeMethod(this, &IptvModel::loadNext, Qt::QueuedConnection);
        return;
    }
    QNetworkReply *reply = m_network.get(request(QUrl(source.value(QStringLiteral("url")).toString())));
    const QString fixedGroup = source.value(QStringLiteral("group")).toString();
    if (source.value(QStringLiteral("free")).toBool()) {
        m_loadingEpgUrls << freeGuideUrl();
    }
    const XtreamLogin login = xtreamLoginFor(source);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, fixedGroup, login] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_errors << tr("%1: %2").arg(name, reply->errorString());
            qWarning().noquote() << "IPTV load failed" << name << reply->errorString();
        } else {
            const int before = m_loading.size();
            parseM3u(reply->readAll());
            if (login.valid()) {
                // Xtream-paneel achter de link: volledige gids (xmltv.php) en per zender het stream-id
                QUrl xmltv(login.server + QStringLiteral("/xmltv.php"));
                QUrlQuery q;
                q.addQueryItem(QStringLiteral("username"), login.user);
                q.addQueryItem(QStringLiteral("password"), login.password);
                xmltv.setQuery(q);
                m_loadingEpgUrls << xmltv.toString();
                static const QRegularExpression streamPath(QStringLiteral(R"((?:/live)?/[^/]+/[^/]+/(\d+)(?:\.\w+)?$)"));
                int linked = 0;
                for (int i = before; i < m_loading.size(); ++i) {
                    const auto m = streamPath.match(QUrl(m_loading.at(i).url).path());
                    if (m.hasMatch()) {
                        m_loading[i].xtreamSource = m_loadIndex;
                        m_loading[i].streamId = m.captured(1);
                        ++linked;
                    }
                }
                qInfo() << "IPTV: M3U link is an Xtream panel; guide via xmltv.php," << linked << "channels with stream id";
            }
            // Gratis lijsten: één groep per land in plaats van losse categorieën
            for (int i = before; !fixedGroup.isEmpty() && i < m_loading.size(); ++i) {
                m_loading[i].group = fixedGroup;
            }
        }
        loadNext();
    });
}

// Leest een M3U-lijst zoals open-tv: #EXTINF-regel met kenmerken, daarna het adres
void IptvModel::parseM3u(const QByteArray &data)
{
    const int before = m_loading.size();
    // Gids-adres(sen) uit de kop
    const QString head = QString::fromUtf8(data.left(4096)).section(QLatin1Char('\n'), 0, 0);
    const QString epg = capture(epgHeader, head);
    for (const QString &url : epg.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        m_loadingEpgUrls << url.trimmed();
    }
    QString info;
    QString extGroup;
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1String("#EXTINF"), Qt::CaseInsensitive)) {
            info = line;
            extGroup.clear();
            continue;
        }
        if (line.startsWith(QLatin1String("#EXTGRP:"), Qt::CaseInsensitive)) {
            extGroup = line.mid(8).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1Char('#')) || info.isEmpty()) {
            continue;
        }
        QString name = capture(nameAttr, info);
        if (name.isEmpty()) {
            // Naam na de komma, voorbij alle kenmerken (die zelf komma's kunnen bevatten)
            const int lastQuote = info.lastIndexOf(QLatin1Char('"'));
            const int comma = info.indexOf(QLatin1Char(','), lastQuote < 0 ? 0 : lastQuote);
            if (comma >= 0) {
                name = info.mid(comma + 1).trimmed();
            }
        }
        if (name.isEmpty()) {
            name = capture(idAttr, info);
        }
        QString group = capture(groupAttr, info);
        if (group.isEmpty()) {
            group = extGroup.isEmpty() ? otherGroup : extGroup;
        }
        if (!name.isEmpty()) {
            m_loading.append({name, capture(logoAttr, info), group, line, capture(idAttr, info)});
        }
        info.clear();
    }
    qInfo() << "IPTV: M3U gave" << (m_loading.size() - before) << "channels";
}

void IptvModel::loadXtream(const QVariantMap &source)
{
    const QString server = source.value(QStringLiteral("url")).toString();
    const QString user = source.value(QStringLiteral("user")).toString();
    const QString password = source.value(QStringLiteral("password")).toString();
    const QString name = source.value(QStringLiteral("name")).toString();
    auto apiUrl = [server, user, password](const QString &action) {
        QUrl url(server + QStringLiteral("/player_api.php"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("username"), user);
        query.addQueryItem(QStringLiteral("password"), password);
        query.addQueryItem(QStringLiteral("action"), action);
        url.setQuery(query);
        return url;
    };

    QNetworkReply *categories = m_network.get(request(apiUrl(QStringLiteral("get_live_categories"))));
    connect(categories, &QNetworkReply::finished, this, [=, this] {
        categories->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(categories->readAll());
        if (categories->error() != QNetworkReply::NoError || !doc.isArray()) {
            m_errors << tr("%1: inloggen mislukt of server niet bereikbaar").arg(name);
            loadNext();
            return;
        }
        QHash<QString, QString> categoryNames;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            categoryNames.insert(o.value(QStringLiteral("category_id")).toVariant().toString(),
                                 o.value(QStringLiteral("category_name")).toString());
        }
        QNetworkReply *streams = m_network.get(request(apiUrl(QStringLiteral("get_live_streams"))));
        connect(streams, &QNetworkReply::finished, this, [=, this] {
            streams->deleteLater();
            const QJsonDocument list = QJsonDocument::fromJson(streams->readAll());
            if (streams->error() != QNetworkReply::NoError || !list.isArray()) {
                m_errors << tr("%1: zenderlijst niet op te halen").arg(name);
                loadNext();
                return;
            }
            for (const QJsonValue &v : list.array()) {
                const QJsonObject o = v.toObject();
                const QString id = o.value(QStringLiteral("stream_id")).toVariant().toString();
                const QString channelName = o.value(QStringLiteral("name")).toString().trimmed();
                if (id.isEmpty() || channelName.isEmpty()) {
                    continue;
                }
                QString group = categoryNames.value(o.value(QStringLiteral("category_id")).toVariant().toString());
                m_loading.append({
                    channelName,
                    o.value(QStringLiteral("stream_icon")).toString(),
                    group.isEmpty() ? otherGroup : group,
                    QStringLiteral("%1/live/%2/%3/%4.ts").arg(server, user, password, id),
                    o.value(QStringLiteral("epg_channel_id")).toString(),
                    m_loadIndex,
                    id,
                });
            }
            qInfo() << "IPTV: Xtream gave" << list.array().size() << "channels";
            // Volledige gids van de server (XMLTV); per zender ook nog op aanvraag
            QUrl xmltv(server + QStringLiteral("/xmltv.php"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("username"), user);
            q.addQueryItem(QStringLiteral("password"), password);
            xmltv.setQuery(q);
            m_loadingEpgUrls << xmltv.toString();
            loadNext();
        });
    });
}

void IptvModel::finishLoading()
{
    // Zijn alle bronnen mislukt, dan de vorige lijst houden
    if (!m_loading.isEmpty() || m_errors.isEmpty()) {
        m_all = m_loading;
        saveCache();
        m_loadingEpgUrls.removeDuplicates();
        m_epgUrls = m_loadingEpgUrls;
        SecureStore::setValue(m_settings, QStringLiteral("iptv/epgUrls"), m_epgUrls);
        reloadEpg();
    }
    m_loading.clear();
    rebuildGroups();
    applyFilter();
    Q_EMIT channelsChanged();
    Q_EMIT filterChanged();
    QString text = m_sources.isEmpty() ? QString() : tr("%1 zenders").arg(m_all.size());
    if (!m_errors.isEmpty()) {
        text += (text.isEmpty() ? QString() : QStringLiteral("  ·  ")) + m_errors.join(QStringLiteral("  ·  "));
    }
    setStatus(text, false);
}

void IptvModel::toggleFavorite(int row)
{
    if (row < 0 || row >= m_visible.size()) {
        return;
    }
    const QString url = m_all.at(m_visible.at(row)).url;
    if (m_favorites.contains(url)) {
        m_favorites.remove(url);
    } else {
        m_favorites.insert(url);
    }
    m_settings.setValue(QStringLiteral("iptv/favorites"), QStringList(m_favorites.begin(), m_favorites.end()));
    if (m_group == favoritesGroup && m_search.isEmpty()) {
        applyFilter();
    } else {
        Q_EMIT dataChanged(index(row), index(row), {NameRole, FavoriteRole});
    }
}

bool IptvModel::isFavorite(int row) const
{
    return row >= 0 && row < m_visible.size() && m_favorites.contains(m_all.at(m_visible.at(row)).url);
}

QVariantMap IptvModel::channelAt(int row) const
{
    if (row < 0 || row >= m_visible.size()) {
        return {};
    }
    const Channel &c = m_all.at(m_visible.at(row));
    return {{QStringLiteral("name"), c.name}, {QStringLiteral("logo"), c.logo}, {QStringLiteral("group"), c.group},
            {QStringLiteral("url"), c.url}, {QStringLiteral("number"), row + 1}};
}

QString IptvModel::nlGuideUrl()
{
    return QStringLiteral("https://epgshare01.online/epgshare01/epg_ripper_NL1.xml.gz");
}

bool IptvModel::isWebChannel(int row) const
{
    return channelAt(row).value(QStringLiteral("url")).toString().startsWith(webPrefix);
}

// NPO/RTL: de zender op de site van de omroep openen
bool IptvModel::openWeb(int row)
{
    const QString url = channelAt(row).value(QStringLiteral("url")).toString();
    if (!url.startsWith(webPrefix)) {
        return false;
    }
    const bool ok = QDesktopServices::openUrl(QUrl(url.mid(webPrefix.size())));
    qInfo() << "IPTV web channel" << channelAt(row).value(QStringLiteral("name")).toString() << (ok ? "opened" : "FAILED");
    return ok;
}

bool IptvModel::openExternal(int row)
{
    const QString url = channelAt(row).value(QStringLiteral("url")).toString();
    if (url.isEmpty()) {
        return false;
    }
    if (MpcPlayer *mpc = MpcPlayer::instance(); mpc && mpc->play({url}, false)) {
        return true;
    }
    return QDesktopServices::openUrl(QUrl(url));
}

QString IptvModel::localPath(const QUrl &url) const
{
    return QDir::toNativeSeparators(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void IptvModel::saveCache() const
{
    QJsonArray array;
    for (const Channel &c : m_all) {
        if (c.group == regionalGroup || c.group == nlGroup) {
            continue;
        }
        array.append(QJsonArray{c.name, c.logo, c.group, c.url, c.epgId, c.xtreamSource, c.streamId});
    }
    QFile file(m_dataDir + QStringLiteral("/iptv-channels.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    }
}

void IptvModel::loadCache()
{
    QFile file(m_dataDir + QStringLiteral("/iptv-channels.json"));
    if (m_sources.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    for (const QJsonValue &v : QJsonDocument::fromJson(file.readAll()).array()) {
        const QJsonArray a = v.toArray();
        if (a.size() >= 4) {
            m_all.append({a.at(0).toString(), a.at(1).toString(), a.at(2).toString(), a.at(3).toString(),
                          a.at(4).toString(), a.size() > 5 ? a.at(5).toInt(-1) : -1, a.at(6).toString()});
        }
    }
}

void IptvModel::setStatus(const QString &status, bool busy)
{
    m_status = status;
    m_busy = busy;
    Q_EMIT statusChanged();
}

// =====================================================================
// Gids (EPG)
// =====================================================================

void IptvModel::reloadEpg()
{
    if (m_epgBusy) {
        return;
    }
    m_shortEpgRequested.clear();
    // Gids voor de NPO- en RTL-zenders (gratis, ook zonder eigen zenderlijst)
    if (!m_epgUrls.contains(nlGuideUrl())) {
        m_epgUrls << nlGuideUrl();
    }
    if (m_epgUrls.isEmpty()) {
        m_epg = {};
        m_epgBase.clear();
        ++m_epgRevision;
        m_epgStatus.clear();
        Q_EMIT epgChanged();
        return;
    }
    m_epgBusy = true;
    m_epgFailures = 0;
    m_epgFiles.clear();
    m_epgIndex = -1;
    m_epgStatus = tr("Gids laden…");
    Q_EMIT epgChanged();
    downloadNextEpg();
}

void IptvModel::downloadNextEpg()
{
    ++m_epgIndex;
    if (m_epgIndex >= m_epgUrls.size()) {
        finishEpgDownload();
        return;
    }
    QString address = m_epgUrls.at(m_epgIndex);
    QUrl url = QUrl::fromUserInput(address);
    QNetworkRequest req = request(url);
    req.setTransferTimeout(180000);
    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            m_epgFiles.append(reply->readAll());
        } else {
            ++m_epgFailures;
            qWarning().noquote() << "EPG download failed"
                                 << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                                 << reply->url().toString() << reply->errorString() << reply->readAll().left(160);
        }
        downloadNextEpg();
    });
}

void IptvModel::finishEpgDownload()
{
    // Alleen de zenders uit onze lijst bewaren: gidsen kunnen enorm zijn
    QSet<QString> wanted;
    for (const Channel &c : std::as_const(m_all)) {
        if (!c.epgId.isEmpty()) {
            wanted.insert(c.epgId);
            wanted.insert(QStringLiteral("base:") + c.epgId.section(QLatin1Char('@'), 0, 0).toLower());
        }
        wanted.insert(QStringLiteral("name:") + c.name.toLower());
    }
    const QList<QByteArray> files = m_epgFiles;
    m_epgFiles.clear();
    // Uitpakken en lezen op de achtergrond, zodat het scherm blijft reageren
    m_epgWatcher.setFuture(QtConcurrent::run([files, wanted] { return parseXmltv(files, wanted); }));
}

IptvModel::EpgData IptvModel::parseXmltv(const QList<QByteArray> &files, const QSet<QString> &wanted)
{
    EpgData data;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 from = now - 3 * 3600;
    const qint64 until = now + 48 * 3600;
    for (const QByteArray &raw : files) {
        const QByteArray xml = gunzip(raw);
        QXmlStreamReader reader(xml);
        QString channelId;
        QSet<QString> matched;
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement()) {
                continue;
            }
            if (reader.name() == QLatin1String("channel")) {
                channelId = reader.attributes().value(QLatin1String("id")).toString();
            } else if (reader.name() == QLatin1String("display-name") && !channelId.isEmpty()) {
                const QString name = reader.readElementText().trimmed().toLower();
                if (!data.names.contains(name) && wanted.contains(QStringLiteral("name:") + name)) {
                    data.names.insert(name, channelId);
                    matched.insert(channelId);
                }
            } else if (reader.name() == QLatin1String("programme")) {
                const auto attrs = reader.attributes();
                const QString channel = attrs.value(QLatin1String("channel")).toString();
                Programme p;
                p.start = xmltvTime(attrs.value(QLatin1String("start")).toString());
                p.stop = xmltvTime(attrs.value(QLatin1String("stop")).toString());
                const bool keep = (wanted.contains(channel) || matched.contains(channel)
                                   || wanted.contains(QStringLiteral("base:") + channel.section(QLatin1Char('@'), 0, 0).toLower()))
                                  && p.stop > from && p.start < until;
                while (!(reader.isEndElement() && reader.name() == QLatin1String("programme")) && !reader.atEnd()) {
                    reader.readNext();
                    if (!keep || !reader.isStartElement()) {
                        continue;
                    }
                    if (reader.name() == QLatin1String("title") && p.title.isEmpty()) {
                        p.title = reader.readElementText().trimmed();
                    } else if (reader.name() == QLatin1String("desc") && p.description.isEmpty()) {
                        p.description = reader.readElementText().trimmed();
                    } else if (reader.name() == QLatin1String("category") && p.category.isEmpty()) {
                        p.category = reader.readElementText().trimmed();
                    }
                }
                if (keep && !p.title.isEmpty()) {
                    data.programmes[channel].append(p);
                }
            }
        }
    }
    // Zenders die alleen via hun naam gekoppeld zijn, en verder ongebruikte gids-id's weg
    QSet<QString> used = wanted;
    for (auto it = data.programmes.constBegin(); it != data.programmes.constEnd(); ++it) {
        if (wanted.contains(QStringLiteral("base:") + it.key().section(QLatin1Char('@'), 0, 0).toLower())) {
            used.insert(it.key());
        }
    }
    for (auto it = data.names.constBegin(); it != data.names.constEnd(); ++it) {
        used.insert(it.value());
    }
    for (auto it = data.programmes.begin(); it != data.programmes.end();) {
        it = used.contains(it.key()) ? std::next(it) : data.programmes.erase(it);
    }
    sortAndTrim(data);
    return data;
}

void IptvModel::sortAndTrim(EpgData &data)
{
    for (auto &list : data.programmes) {
        std::sort(list.begin(), list.end(), [](const Programme &a, const Programme &b) { return a.start < b.start; });
    }
}

QString IptvModel::epgKey(const Channel &c) const
{
    if (!c.epgId.isEmpty() && m_epg.programmes.contains(c.epgId)) {
        return c.epgId;
    }
    if (c.xtreamSource >= 0 && m_epg.programmes.contains(QStringLiteral("xtream:") + c.streamId)) {
        return QStringLiteral("xtream:") + c.streamId;
    }
    if (!c.epgId.isEmpty()) {
        // Zelfde zender, andere uitvoering in de id (NPO1.nl@SD tegenover NPO1.nl@HD of NPO1.nl)
        const QString viaBase = m_epgBase.value(c.epgId.section(QLatin1Char('@'), 0, 0).toLower());
        if (!viaBase.isEmpty()) {
            return viaBase;
        }
    }
    return m_epg.names.value(c.name.toLower());
}

void IptvModel::rebuildEpgIndex()
{
    m_epgBase.clear();
    for (auto it = m_epg.programmes.constBegin(); it != m_epg.programmes.constEnd(); ++it) {
        if (!it.key().startsWith(QLatin1String("xtream:"))) {
            m_epgBase.insert(it.key().section(QLatin1Char('@'), 0, 0).toLower(), it.key());
        }
    }
}

QString IptvModel::freeGuideUrl()
{
    // Openbare repo kijkluister-epg, elke nacht overschreven door de workflow "Tv-gids"
    return QStringLiteral("https://raw.githubusercontent.com/firefly2008/kijkluister-epg/main/guide.xml.gz");
}

void IptvModel::setExtraEpgUrls(const QStringList &urls)
{
    QStringList clean;
    for (const QString &u : urls) {
        if (!u.trimmed().isEmpty() && !clean.contains(u.trimmed())) {
            clean << u.trimmed();
        }
    }
    if (clean == m_extraEpgUrls) {
        return;
    }
    m_extraEpgUrls = clean;
    m_settings.setValue(QStringLiteral("iptv/extraEpg"), m_extraEpgUrls);
    m_settings.sync();
    Q_EMIT sourcesChanged();
    // Gidsadressen opnieuw verzamelen en de gids ophalen
    QStringList all = m_epgUrls;
    for (const QString &u : std::as_const(m_extraEpgUrls)) {
        if (!all.contains(u)) {
            all << u;
        }
    }
    m_epgUrls = all;
    SecureStore::setValue(m_settings, QStringLiteral("iptv/epgUrls"), m_epgUrls);
    reloadEpg();
}

const QList<IptvModel::Programme> *IptvModel::programmesFor(const Channel &c) const
{
    const QString key = epgKey(c);
    const auto it = m_epg.programmes.constFind(key);
    return (key.isEmpty() || it == m_epg.programmes.constEnd()) ? nullptr : &it.value();
}

QVariantMap IptvModel::nowNext(int row) const
{
    if (row < 0 || row >= m_visible.size()) {
        return {};
    }
    const QList<Programme> *list = programmesFor(m_all.at(m_visible.at(row)));
    if (!list) {
        return {};
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto fmt = [](qint64 secs) { return QDateTime::fromSecsSinceEpoch(secs).toLocalTime().toString(QStringLiteral("HH:mm")); };
    QVariantMap result;
    for (int i = 0; i < list->size(); ++i) {
        const Programme &p = list->at(i);
        if (p.stop <= now) {
            continue;
        }
        if (p.start <= now) {
            result[QStringLiteral("nowTitle")] = p.title;
            result[QStringLiteral("nowTime")] = fmt(p.start) + QStringLiteral(" – ") + fmt(p.stop);
            result[QStringLiteral("nowProgress")] = double(now - p.start) / qMax<qint64>(1, p.stop - p.start);
            result[QStringLiteral("nowDescription")] = p.description;
            if (i + 1 < list->size()) {
                result[QStringLiteral("nextTitle")] = list->at(i + 1).title;
                result[QStringLiteral("nextTime")] = fmt(list->at(i + 1).start);
            }
        } else {
            result[QStringLiteral("nextTitle")] = p.title;
            result[QStringLiteral("nextTime")] = fmt(p.start);
        }
        break;
    }
    return result;
}

QVariantList IptvModel::guide(int row) const
{
    if (row < 0 || row >= m_visible.size()) {
        return {};
    }
    const QList<Programme> *list = programmesFor(m_all.at(m_visible.at(row)));
    if (!list) {
        return {};
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QDate today = QDate::currentDate();
    QVariantList result;
    for (const Programme &p : *list) {
        if (p.stop <= now) {
            continue;
        }
        const QDateTime start = QDateTime::fromSecsSinceEpoch(p.start).toLocalTime();
        const QDateTime stop = QDateTime::fromSecsSinceEpoch(p.stop).toLocalTime();
        const qint64 days = today.daysTo(start.date());
        QString day = days == 0 ? tr("Vandaag") : days == 1 ? tr("Morgen") : QLocale().toString(start.date(), QStringLiteral("dddd d MMMM"));
        result.append(QVariantMap{
            {QStringLiteral("title"), p.title},
            {QStringLiteral("description"), p.description},
            {QStringLiteral("category"), p.category},
            {QStringLiteral("start"), start.toString(QStringLiteral("HH:mm"))},
            {QStringLiteral("stop"), stop.toString(QStringLiteral("HH:mm"))},
            {QStringLiteral("day"), day},
            {QStringLiteral("duration"), int((p.stop - p.start) / 60)},
            {QStringLiteral("startSecs"), double(p.start)},
            {QStringLiteral("stopSecs"), double(p.stop)},
            {QStringLiteral("isNow"), p.start <= now},
            {QStringLiteral("progress"), p.start <= now ? double(now - p.start) / qMax<qint64>(1, p.stop - p.start) : 0.0},
        });
        if (result.size() >= 60) {
            break;
        }
    }
    return result;
}

// Xtream: gids van een zender rechtstreeks opvragen, zoals open-tv (get_simple_data_table)
void IptvModel::requestEpg(int row)
{
    if (row < 0 || row >= m_visible.size()) {
        return;
    }
    const Channel c = m_all.at(m_visible.at(row));
    if (c.xtreamSource < 0 || c.xtreamSource >= m_sources.size() || c.streamId.isEmpty()
        || m_shortEpgRequested.contains(c.streamId)) {
        return;
    }
    const QList<Programme> *existing = programmesFor(c);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (existing && !existing->isEmpty() && existing->last().stop > now + 3600) {
        return; // al genoeg gids
    }
    m_shortEpgRequested.insert(c.streamId);
    const XtreamLogin login = xtreamLoginFor(m_sources.at(c.xtreamSource).toMap());
    if (!login.valid()) {
        return;
    }
    QUrl url(login.server + QStringLiteral("/player_api.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("username"), login.user);
    query.addQueryItem(QStringLiteral("password"), login.password);
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("get_simple_data_table"));
    query.addQueryItem(QStringLiteral("stream_id"), c.streamId);
    url.setQuery(query);
    QNetworkReply *reply = m_network.get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, c] {
        reply->deleteLater();
        const QJsonArray listings = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("epg_listings")).toArray();
        QList<Programme> programmes;
        auto text = [](const QJsonValue &v) { return QString::fromUtf8(QByteArray::fromBase64(v.toString().toUtf8())).trimmed(); };
        for (const QJsonValue &v : listings) {
            const QJsonObject o = v.toObject();
            Programme p;
            p.start = o.value(QStringLiteral("start_timestamp")).toVariant().toLongLong();
            p.stop = o.value(QStringLiteral("stop_timestamp")).toVariant().toLongLong();
            p.title = text(o.value(QStringLiteral("title")));
            p.description = text(o.value(QStringLiteral("description")));
            if (p.start > 0 && p.stop > p.start && !p.title.isEmpty()) {
                programmes.append(p);
            }
        }
        if (programmes.isEmpty()) {
            return;
        }
        std::sort(programmes.begin(), programmes.end(), [](const Programme &a, const Programme &b) { return a.start < b.start; });
        m_epg.programmes.insert(QStringLiteral("xtream:") + c.streamId, programmes);
        ++m_epgRevision;
        Q_EMIT epgChanged();
    });
}

void IptvModel::saveEpgCache() const
{
    QJsonObject programmes;
    for (auto it = m_epg.programmes.constBegin(); it != m_epg.programmes.constEnd(); ++it) {
        QJsonArray list;
        for (const Programme &p : it.value()) {
            list.append(QJsonArray{double(p.start), double(p.stop), p.title, p.description, p.category});
        }
        programmes.insert(it.key(), list);
    }
    QJsonObject names;
    for (auto it = m_epg.names.constBegin(); it != m_epg.names.constEnd(); ++it) {
        names.insert(it.key(), it.value());
    }
    QFile file(m_dataDir + QStringLiteral("/iptv-epg.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("saved"), double(QDateTime::currentSecsSinceEpoch())},
                                             // Mislukte download: niet als "vers" bewaren, de volgende start probeert het opnieuw
                                             {QStringLiteral("urls"), m_epgFailures > 0 ? QJsonArray() : QJsonArray::fromStringList(m_epgUrls)},
                                             {QStringLiteral("programmes"), programmes},
                                             {QStringLiteral("names"), names}}).toJson(QJsonDocument::Compact));
    }
}

bool IptvModel::loadEpgCache()
{
    QFile file(m_dataDir + QStringLiteral("/iptv-epg.json"));
    if (m_epgUrls.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const qint64 saved = qint64(root.value(QStringLiteral("saved")).toDouble());
    const QJsonObject programmes = root.value(QStringLiteral("programmes")).toObject();
    for (auto it = programmes.constBegin(); it != programmes.constEnd(); ++it) {
        QList<Programme> list;
        for (const QJsonValue &v : it.value().toArray()) {
            const QJsonArray a = v.toArray();
            list.append({qint64(a.at(0).toDouble()), qint64(a.at(1).toDouble()), a.at(2).toString(), a.at(3).toString(), a.at(4).toString()});
        }
        m_epg.programmes.insert(it.key(), list);
    }
    const QJsonObject names = root.value(QStringLiteral("names")).toObject();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        m_epg.names.insert(it.key(), it.value().toString());
    }
    rebuildEpgIndex();
    ++m_epgRevision;
    m_epgStatus = tr("Gids: %1 zenders").arg(m_epg.programmes.size());
    // Ouder dan 6 uur, of er is sindsdien een gidsadres bijgekomen: opnieuw ophalen
    const QStringList cachedUrls = root.value(QStringLiteral("urls")).toVariant().toStringList();
    for (const QString &u : std::as_const(m_epgUrls)) {
        if (!cachedUrls.contains(u)) {
            return false;
        }
    }
    return QDateTime::currentSecsSinceEpoch() - saved < 6 * 3600;
}

QVariantList IptvModel::searchAll(const QString &query, int limit) const
{
    const QString q = query.trimmed();
    QVariantList result;
    if (q.size() < 2) {
        return result;
    }
    QVariantList programmes;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSet<QString> seenChannels;
    for (const Channel &c : m_all) {
        if (seenChannels.contains(c.url)) {
            continue;
        }
        seenChannels.insert(c.url);
        if (c.name.contains(q, Qt::CaseInsensitive) && result.size() < limit) {
            result.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("channel")}, {QStringLiteral("name"), c.name},
                                      {QStringLiteral("detail"), c.group}, {QStringLiteral("icon"), c.logo},
                                      {QStringLiteral("url"), c.url}});
        }
        if (programmes.size() >= limit) {
            continue;
        }
        if (const QList<Programme> *list = programmesFor(c)) {
            for (const Programme &p : *list) {
                if (p.stop <= now || !p.title.contains(q, Qt::CaseInsensitive)) {
                    continue;
                }
                const QDateTime start = QDateTime::fromSecsSinceEpoch(p.start).toLocalTime();
                const qint64 days = QDate::currentDate().daysTo(start.date());
                const QString day = p.start <= now ? tr("Nu") : days == 0 ? tr("Vandaag") : days == 1 ? tr("Morgen")
                                  : QLocale().toString(start.date(), QStringLiteral("dddd"));
                programmes.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("programme")}, {QStringLiteral("name"), p.title},
                                              {QStringLiteral("detail"), QStringLiteral("%1 · %2 %3").arg(c.name, day, start.toString(QStringLiteral("HH:mm")))},
                                              {QStringLiteral("icon"), c.logo}, {QStringLiteral("url"), c.url},
                                              {QStringLiteral("channel"), c.name},
                                              {QStringLiteral("startSecs"), double(p.start)}, {QStringLiteral("stopSecs"), double(p.stop)},
                                              {QStringLiteral("isNow"), p.start <= now}});
                if (programmes.size() >= limit) {
                    break;
                }
            }
        }
    }
    std::sort(programmes.begin(), programmes.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("startSecs")).toDouble() < b.toMap().value(QStringLiteral("startSecs")).toDouble();
    });
    result.append(programmes);
    return result;
}

int IptvModel::showChannel(const QString &url)
{
    if (!m_search.isEmpty()) {
        setSearch(QString());
    }
    if (m_group != allGroup) {
        setGroup(allGroup);
    }
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_all.at(m_visible.at(row)).url == url) {
            return row;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- 4.4: gidsraster, Nu op tv, herinneringen, series

const IptvModel::Channel *IptvModel::channelByUrl(const QString &url) const
{
    for (const Channel &c : m_all) {
        if (c.url == url) {
            return &c;
        }
    }
    return nullptr;
}

QVariantList IptvModel::guideGrid(int first, int count, double from, double until) const
{
    QVariantList rows;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto fmt = [](qint64 secs) { return QDateTime::fromSecsSinceEpoch(secs).toLocalTime().toString(QStringLiteral("HH:mm")); };
    for (int row = qMax(0, first); row < m_visible.size() && row < first + count; ++row) {
        const Channel &c = m_all.at(m_visible.at(row));
        QVariantList programmes;
        if (const QList<Programme> *list = programmesFor(c)) {
            for (const Programme &p : *list) {
                if (p.stop <= qint64(from) || p.start >= qint64(until)) {
                    continue;
                }
                programmes.append(QVariantMap{
                    {QStringLiteral("title"), p.title},
                    {QStringLiteral("description"), p.description},
                    {QStringLiteral("startSecs"), double(p.start)},
                    {QStringLiteral("stopSecs"), double(p.stop)},
                    {QStringLiteral("start"), fmt(p.start)},
                    {QStringLiteral("stop"), fmt(p.stop)},
                    {QStringLiteral("isNow"), p.start <= now && p.stop > now},
                });
            }
        }
        rows.append(QVariantMap{{QStringLiteral("row"), row}, {QStringLiteral("name"), c.name}, {QStringLiteral("logo"), c.logo},
                                {QStringLiteral("url"), c.url}, {QStringLiteral("programmes"), programmes}});
    }
    return rows;
}

QVariantList IptvModel::favoritesNow(int limit) const
{
    QVariantList result;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto fmt = [](qint64 secs) { return QDateTime::fromSecsSinceEpoch(secs).toLocalTime().toString(QStringLiteral("HH:mm")); };
    for (const Channel &c : m_all) {
        if (!m_favorites.contains(c.url)) {
            continue;
        }
        QVariantMap item{{QStringLiteral("url"), c.url}, {QStringLiteral("name"), c.name}, {QStringLiteral("logo"), c.logo},
                         {QStringLiteral("title"), QString()}, {QStringLiteral("time"), QString()}, {QStringLiteral("progress"), -1.0}};
        if (const QList<Programme> *list = programmesFor(c)) {
            for (const Programme &p : *list) {
                if (p.start <= now && p.stop > now) {
                    item[QStringLiteral("title")] = p.title;
                    item[QStringLiteral("time")] = fmt(p.start) + QStringLiteral(" – ") + fmt(p.stop);
                    item[QStringLiteral("progress")] = double(now - p.start) / qMax<qint64>(1, p.stop - p.start);
                    break;
                }
            }
        }
        result.append(item);
        if (result.size() >= limit) {
            break;
        }
    }
    return result;
}

bool IptvModel::isReminder(const QString &url, double start) const
{
    for (const QVariant &v : m_reminders) {
        const QVariantMap r = v.toMap();
        if (r.value(QStringLiteral("url")).toString() == url && qint64(r.value(QStringLiteral("start")).toDouble()) == qint64(start)) {
            return true;
        }
    }
    return false;
}

void IptvModel::toggleReminder(const QString &channel, const QString &url, const QString &logo, const QString &title, double start)
{
    for (int i = 0; i < m_reminders.size(); ++i) {
        const QVariantMap r = m_reminders.at(i).toMap();
        if (r.value(QStringLiteral("url")).toString() == url && qint64(r.value(QStringLiteral("start")).toDouble()) == qint64(start)) {
            m_reminders.removeAt(i);
            m_settings.setValue(QStringLiteral("iptv/reminders"), m_reminders);
            qInfo() << "Reminder removed" << title;
            Q_EMIT remindersChanged();
            return;
        }
    }
    m_reminders.append(QVariantMap{{QStringLiteral("channel"), channel}, {QStringLiteral("url"), url}, {QStringLiteral("logo"), logo},
                                   {QStringLiteral("title"), title}, {QStringLiteral("start"), start}});
    m_settings.setValue(QStringLiteral("iptv/reminders"), m_reminders);
    qInfo() << "Reminder set" << title << QDateTime::fromSecsSinceEpoch(qint64(start)).toString(Qt::ISODate);
    Q_EMIT remindersChanged();
    checkReminders();
}

void IptvModel::checkReminders()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bool changed = false;
    for (int i = m_reminders.size() - 1; i >= 0; --i) {
        QVariantMap r = m_reminders.at(i).toMap();
        const qint64 start = qint64(r.value(QStringLiteral("start")).toDouble());
        if (start - now <= 120) {
            m_reminders.removeAt(i);
            changed = true;
            // Te laat (meer dan een half uur bezig): stil weghalen
            if (now - start < 1800) {
                r[QStringLiteral("time")] = QDateTime::fromSecsSinceEpoch(start).toLocalTime().toString(QStringLiteral("HH:mm"));
                qInfo() << "Reminder due" << r.value(QStringLiteral("title")).toString();
                Q_EMIT reminderDue(r);
            }
        }
    }
    if (changed) {
        m_settings.setValue(QStringLiteral("iptv/reminders"), m_reminders);
        Q_EMIT remindersChanged();
    }
}

bool IptvModel::isSeries(const QString &url, const QString &title) const
{
    for (const QVariant &v : m_seriesRules) {
        const QVariantMap r = v.toMap();
        if (r.value(QStringLiteral("url")).toString() == url
            && r.value(QStringLiteral("title")).toString().compare(title, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void IptvModel::toggleSeries(const QString &channel, const QString &url, const QString &title)
{
    for (int i = 0; i < m_seriesRules.size(); ++i) {
        const QVariantMap r = m_seriesRules.at(i).toMap();
        if (r.value(QStringLiteral("url")).toString() == url
            && r.value(QStringLiteral("title")).toString().compare(title, Qt::CaseInsensitive) == 0) {
            m_seriesRules.removeAt(i);
            m_settings.setValue(QStringLiteral("iptv/series"), m_seriesRules);
            qInfo() << "Series removed" << title;
            Q_EMIT seriesChanged();
            return;
        }
    }
    m_seriesRules.append(QVariantMap{{QStringLiteral("channel"), channel}, {QStringLiteral("url"), url}, {QStringLiteral("title"), title}});
    m_settings.setValue(QStringLiteral("iptv/series"), m_seriesRules);
    qInfo() << "Series added" << title << channel;
    Q_EMIT seriesChanged();
    planSeries();
}

void IptvModel::planSeries()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int planned = 0;
    for (const QVariant &v : std::as_const(m_seriesRules)) {
        const QVariantMap r = v.toMap();
        const QString url = r.value(QStringLiteral("url")).toString();
        const QString title = r.value(QStringLiteral("title")).toString();
        const Channel *c = channelByUrl(url);
        const QList<Programme> *list = c ? programmesFor(*c) : nullptr;
        if (!list) {
            continue;
        }
        for (const Programme &p : *list) {
            // Alles wat nog komt (of net begonnen is) in de komende twee dagen
            if (p.stop > now && p.start < now + 48 * 3600 && p.title.compare(title, Qt::CaseInsensitive) == 0) {
                Q_EMIT seriesEpisode(r.value(QStringLiteral("channel")).toString(), url, p.title, p.start, p.stop);
                ++planned;
            }
        }
    }
    if (planned > 0) {
        qInfo() << "Series: checked" << planned << "broadcast(s)";
    }
}

int IptvModel::rowOf(const QString &url) const
{
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_all.at(m_visible.at(row)).url == url) {
            return row;
        }
    }
    return -1;
}
