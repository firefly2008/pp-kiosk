// SPDX-License-Identifier: GPL-2.0-or-later
#include "radiomodel.h"

#include <QCoreApplication>
#include "paths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QTimeZone>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

RadioModel::RadioModel(const QString &dataDir, QObject *parent)
    : QAbstractListModel(parent)
    , m_dataDir(dataDir)
{
    // Volgorde: luistertijdaandeel NMO, januari-juni 2026. Per zender een reserve-adres:
    // gaat het eerste mis, dan probeert de speler het volgende.
    const QString stw = QStringLiteral("https://playerservices.streamtheworld.com/api/livestream-redirect/");
    m_stations = {
        {QStringLiteral("NPO Radio 2"), QStringLiteral("Grootste zender van Nederland"), QStringLiteral("www.nporadio2.nl"), QStringLiteral("#b3261e"),
         {QStringLiteral("https://icecast.omroep.nl/radio2-bb-mp3"), QStringLiteral("https://icecast.omroep.nl/radio2-sb-mp3")}},
        {QStringLiteral("Qmusic"), QStringLiteral("Hits en de Top 40"), QStringLiteral("qmusic.nl"), QStringLiteral("#b8231f"),
         {QStringLiteral("https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_live.mp3"), QStringLiteral("https://stream.qmusic.nl/qmusic/mp3")}},
        {QStringLiteral("Radio 538"), QStringLiteral("Dance en hits"), QStringLiteral("www.538.nl"), QStringLiteral("#1e3f8a"),
         {stw + QStringLiteral("RADIO538.mp3"), stw + QStringLiteral("TLPSTR01.mp3")}},
        {QStringLiteral("Sky Radio"), QStringLiteral("Non-stop muziek"), QStringLiteral("www.skyradio.nl"), QStringLiteral("#0d5a9e"),
         {stw + QStringLiteral("SKYRADIO.mp3")}},
        {QStringLiteral("Radio 10"), QStringLiteral("Hits van toen"), QStringLiteral("www.radio10.nl"), QStringLiteral("#c2410c"),
         {stw + QStringLiteral("RADIO10.mp3")}},
        {QStringLiteral("JOE"), QStringLiteral("Hits van de jaren 80, 90 en nu"), QStringLiteral("joe.nl"), QStringLiteral("#8a1d3f"),
         {QStringLiteral("https://icecast-qmusicnl-cdp.triple-it.nl/Joe_nl.mp3"),
          QStringLiteral("https://icecast-qmusicnl-cdp.triple-it.nl/Joe_nl_high.aac"),
          QStringLiteral("https://icecast-qmusicnl-cdp.triple-it.nl/Joe_nl.mp3?aw_0_1st.playerId=redirect"),
          QStringLiteral("https://audio-streaming.joe.nl/Joe_nl.mp3")}},
        {QStringLiteral("Radio Veronica"), QStringLiteral("Rock en classics"), QStringLiteral("www.radioveronica.nl"), QStringLiteral("#3b2a7a"),
         {stw + QStringLiteral("VERONICA.mp3"), QStringLiteral("https://stream.radioveronica.nl/veronica")}},
    };
    // Alle andere Nederlandse zenders (getest november 2026): te kiezen via Diensten kiezen
    struct Extra { const char *name; const char *slogan; const char *site; const char *accent; const char *url; const char *url2; };
    static const Extra extras[] = {
        {"NPO Radio 1", "Nieuws en sport", "www.nporadio1.nl", "#b3261e", "https://icecast.omroep.nl/radio1-bb-mp3", "https://icecast.omroep.nl/radio1-bb-aac"},
        {"NPO 3FM", "Nieuwe muziek", "www.npo3fm.nl", "#d4331f", "https://icecast.omroep.nl/3fm-bb-mp3", ""},
        {"NPO Klassiek", "Klassieke muziek", "www.npoklassiek.nl", "#7a1f3d", "https://icecast.omroep.nl/radio4-bb-mp3", ""},
        {"NPO Radio 5", "Hits van de jaren 60, 70 en 80", "www.nporadio5.nl", "#2f5aa8", "https://icecast.omroep.nl/radio5-bb-mp3", "https://icecast.omroep.nl/radio5-sb-aac"},
        {"NPO FunX", "Urban en hiphop", "www.funx.nl", "#4b2a8a", "https://icecast.omroep.nl/funx-bb-mp3", "http://icecast.omroep.nl/funx-hiphopfb-bb-mp3"},
        {"100% NL", "Alleen Nederlandse muziek", "www.100p.nl", "#c2410c", "https://stream.100p.nl/100pctnl.mp3", ""},
        {"Sublime", "Soul, funk en jazz", "sublime.nl", "#3b2a6b", "https://playerservices.streamtheworld.com/api/livestream-redirect/SUBLIME.mp3", ""},
        {"SLAM!", "Dance", "www.slam.nl", "#a21caf", "http://stream.slam.nl/slam", ""},
        {"KINK", "Alternatieve rock", "kink.nl", "#1f2937", "https://29043.live.streamtheworld.com/KINKAAC_SC", ""},
        {"Arrow Classic Rock", "Classic rock", "www.arrow.nl", "#7c2d12", "https://stream.player.arrow.nl/arrowcr", "http://stream.gal.io/arrow"},
        {"BNR Nieuwsradio", "Zakelijk nieuws", "www.bnr.nl", "#1e3a5f", "https://stream.bnr.nl/bnr_mp3_128_20", "https://stream.bnr.nl/bnr_aac_96_20"},
        {"Radio Noordzee", "Hits van toen en nu", "www.radionoordzee.nl", "#0e5a7a", "https://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR17.mp3", ""},
        {"Groot Nieuws Radio", "Christelijke muziek", "www.grootnieuwsradio.nl", "#14532d", "https://playerservices.streamtheworld.com/api/livestream-redirect/GNR.mp3", ""},
        {"Radio Decibel", "Hits uit Amsterdam", "decibel.nl", "#9a1b5f", "https://28033.live.streamtheworld.com/RADIODECIBEL.mp3", ""},
        {"Simone FM", "Gezellige muziek", "www.simone.nl", "#be185d", "https://stream.simone.nl/simone", ""},
        {"Radio Continu", "Easy listening", "radiocontinu.nl", "#365314", "https://stream.radiocontinu.nl/radiocontinu", ""},
        {"Classic NL", "Klassiek", "classic.nl", "#4a3728", "https://stream.classic.nl/classicnl.mp3", ""},
        {"Grand Prix Radio", "Motorsport", "grandprixradio.nl", "#991b1b", "https://playerservices.streamtheworld.com/api/livestream-redirect/GRAND_PRIX_RADIO.mp3", ""},
        {"Concertzender", "Oude muziek", "www.concertzender.nl", "#44403c", "http://streams.greenhost.nl:8080/oudemuziek", ""},
        {"Qmusic Non-Stop", "Hits zonder praatjes", "qmusic.nl", "#b8231f", "https://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_nonstop_96.mp3", ""},
        {"Qmusic Foute Uur", "Foute hits", "qmusic.nl", "#b8231f", "https://stream.qmusic.nl/fouteuur/aac", ""},
        {"Sky Radio Lovesongs", "Liefdesliedjes", "www.skyradio.nl", "#0d5a9e", "https://25233.live.streamtheworld.com/SRGSTR03AAC.aac", ""},
        {"Radio 538 Hitzone", "Hitzone", "www.538.nl", "#1e3f8a", "https://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR11.mp3", ""},
        {"Radio 10 Top 4000", "De Top 4000", "www.radio10.nl", "#c2410c", "https://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR24.mp3", ""},
        {"JOE 70s & 80s", "Hits uit de jaren 70 en 80", "joe.nl", "#8a1d3f", "https://stream.joe.nl/joe/aachigh", ""},
        {"SLAM! Non Stop", "Dance zonder praatjes", "www.slam.nl", "#a21caf", "http://stream.radiocorp.nl/web10_mp3", ""},
        // Regionale omroepen
        {"Radio Gelderland", "Omroep Gelderland", "www.gld.nl", "#1d4ed8", "https://d2od87akyl46nm.cloudfront.net/icecast/omroepgelderland/radiogelderland", ""},
        {"NH Radio", "Noord-Holland", "www.nhnieuws.nl", "#c2410c", "https://ice.cr6.streamzilla.xlcdn.com:8000/sz=nhnieuws=NHRadio_mp3", ""},
        {"Radio Rijnmond", "Rotterdam en omgeving", "www.rijnmond.nl", "#15803d", "http://d2e9xgjjdd9cr5.cloudfront.net/icecast/rijnmond/radio-mp3", ""},
        {"Omroep Brabant Radio", "Brabant", "www.omroepbrabant.nl", "#b91c1c", "https://av.omroepbrabant.nl/icecast/omroepbrabant/mp3hq", "http://streaming.omroepbrabant.nl/mp3hq"},
        {"L1 Radio", "Limburg", "l1.nl", "#0f766e", "https://d34pj260kw1xmk.cloudfront.net/icecast/l1/radio-bb-mp3", ""},
        {"Radio Oost", "Overijssel", "www.oost.nl", "#be123c", "https://d34cg2bnc08ruf.cloudfront.net/icecast/rtvoost/radio-oost-mp3", ""},
        {"Radio Noord", "Groningen", "www.rtvnoord.nl", "#1d4ed8", "https://media.rtvnoord.nl/icecast/rtvnoord/radio", ""},
        {"Omrop Fryslân Radio", "Friesland", "www.omropfryslan.nl", "#1e40af", "https://d3pvma9xb2775h.cloudfront.net/icecast/omropfryslan/radio.mp3", ""},
        {"Radio Drenthe", "Drenthe", "www.rtvdrenthe.nl", "#b45309", "https://cdn.rtvdrenthe.nl/icecast/rtvdrenthe/rtvradio", ""},
        {"Omroep Zeeland Radio", "Zeeland", "www.omroepzeeland.nl", "#0369a1", "https://d3isaxd2t6q8zm.cloudfront.net/icecast/omroepzeeland/omroepzeeland_radio", ""},
        {"Radio M Utrecht", "Utrecht", "www.rtvutrecht.nl", "#c2410c", "https://d18rwjdhpr8dcw.cloudfront.net/icecast/rtvutrecht/radiomutrecht-bb-mp3", ""},
    };
    for (const Extra &e : extras) {
        Station st;
        st.name = QString::fromUtf8(e.name);
        st.slogan = QString::fromUtf8(e.slogan);
        st.site = QString::fromUtf8(e.site);
        st.accent = QString::fromUtf8(e.accent);
        st.streams = {QString::fromUtf8(e.url)};
        if (*e.url2) {
            st.streams.append(QString::fromUtf8(e.url2));
        }
        st.extra = true;
        m_stations.append(st);
    }
    loadCustom();
    rebuild();
    fetchLogos();
}

void RadioModel::loadCustom()
{
    const QJsonArray list = QJsonDocument::fromJson(QSettings(appSettingsFile(), QSettings::IniFormat).value(profileKey(QStringLiteral("radio/custom"))).toByteArray()).array();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        Station s;
        s.name = o.value(QStringLiteral("name")).toString();
        s.slogan = o.value(QStringLiteral("slogan")).toString();
        s.site = o.value(QStringLiteral("site")).toString();
        s.accent = o.value(QStringLiteral("accent")).toString(QStringLiteral("#2f3a4a"));
        s.streams = {o.value(QStringLiteral("url")).toString()};
        s.logoUrl = o.value(QStringLiteral("logo")).toString();
        s.custom = true;
        if (!s.name.isEmpty() && !s.streams.first().isEmpty()) {
            m_stations.append(s);
        }
    }
}

void RadioModel::saveCustom() const
{
    QJsonArray list;
    for (const Station &s : m_stations) {
        if (s.custom) {
            list.append(QJsonObject{{QStringLiteral("name"), s.name}, {QStringLiteral("slogan"), s.slogan}, {QStringLiteral("site"), s.site},
                                    {QStringLiteral("accent"), s.accent}, {QStringLiteral("url"), s.streams.value(0)},
                                    {QStringLiteral("logo"), s.logoUrl}});
        }
    }
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(profileKey(QStringLiteral("radio/custom")), QJsonDocument(list).toJson(QJsonDocument::Compact));
}

void RadioModel::rebuild()
{
    QSettings settings(appSettingsFile(), QSettings::IniFormat);
    const QStringList hidden = settings.value(profileKey(QStringLiteral("radio/hidden"))).toStringList();
    const QStringList order = settings.value(profileKey(QStringLiteral("radio/order"))).toStringList();
    const QStringList shown = settings.value(profileKey(QStringLiteral("radio/shown"))).toStringList();
    QList<int> rows;
    for (int i = 0; i < m_stations.size(); ++i) {
        const Station &st = m_stations.at(i);
        const bool visible = st.extra ? shown.contains(st.name) : !hidden.contains(st.name);
        if (visible) {
            rows.append(i);
        }
    }
    // (Tegel "Radiozender toevoegen" vervallen in 4.1: toevoegen gaat via Aanpassen)
    auto keyOf = [this](int i) { return i < 0 ? QStringLiteral("+add") : m_stations.at(i).name; };
    // Opgeslagen volgorde; nieuwe zenders vóór de toevoegtegel als die achteraan stond
    const int addPos = order.indexOf(QStringLiteral("+add"));
    auto rank = [&](int i) -> double {
        const int pos = order.indexOf(keyOf(i));
        if (pos >= 0) return pos;
        if (i < 0) return 100000;
        return addPos >= 0 ? addPos - 0.5 : 99999;
    };
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) { return rank(a) < rank(b); });
    m_visible = rows;
}

void RadioModel::saveOrder() const
{
    QStringList order;
    for (int i : m_visible) {
        order << (i < 0 ? QStringLiteral("+add") : m_stations.at(i).name);
    }
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(profileKey(QStringLiteral("radio/order")), order);
}

void RadioModel::move(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= count() || to >= count()) {
        return;
    }
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_visible.move(from, to);
    endMoveRows();
    saveOrder();
}

int RadioModel::nextStation(int row, int step) const
{
    if (stationCount() <= 0) {
        return -1;
    }
    int r = row;
    for (int i = 0; i < count(); ++i) {
        r = (r + step + count()) % count();
        if (isStation(r)) {
            return r;
        }
    }
    return row;
}

QVariantList RadioModel::catalog() const
{
    QVariantList list;
    for (int i = 0; i < m_stations.size(); ++i) {
        const Station &st = m_stations.at(i);
        if (st.custom) {
            continue;
        }
        const QString file = logoFileFor(st);
        list.append(QVariantMap{{QStringLiteral("name"), st.name},
                                {QStringLiteral("icon"), QFile::exists(file) ? QUrl::fromLocalFile(file).toString() : QString()},
                                {QStringLiteral("accent"), st.accent},
                                {QStringLiteral("on"), m_visible.contains(i)}});
    }
    return list;
}

void RadioModel::setChosen(const QStringList &names)
{
    QStringList hidden;
    QStringList shown;
    for (const Station &st : std::as_const(m_stations)) {
        if (st.custom) {
            continue;
        }
        if (st.extra && names.contains(st.name)) {
            shown << st.name;
        } else if (!st.extra && !names.contains(st.name)) {
            hidden << st.name;
        }
    }
    QSettings settings(appSettingsFile(), QSettings::IniFormat);
    settings.setValue(profileKey(QStringLiteral("radio/hidden")), hidden);
    settings.setValue(profileKey(QStringLiteral("radio/shown")), shown);
    beginResetModel();
    rebuild();
    endResetModel();
    Q_EMIT countChanged();
    ++m_revision;
    Q_EMIT revisionChanged();
    qInfo() << "Radio chosen" << stationCount();
}

int RadioModel::hiddenCount() const
{
    int n = 0;
    for (int i = 0; i < m_stations.size(); ++i) {
        if (!m_stations.at(i).extra && !m_stations.at(i).custom && !m_visible.contains(i)) {
            ++n;
        }
    }
    return n;
}

QString RadioModel::pathAt(int row) const
{
    return isAddTile(row) ? QStringLiteral("radio:add") : QStringLiteral("radio:") + nameAt(row);
}

int RadioModel::rowOfName(const QString &name) const
{
    for (int row = 0; row < count(); ++row) {
        if (isStation(row) && st(row).name.compare(name, Qt::CaseInsensitive) == 0) {
            return row;
        }
    }
    return -1;
}

void RadioModel::remove(int row)
{
    if (!isStation(row)) {
        return;
    }
    beginResetModel();
    const int index = m_visible.at(row);
    if (m_stations.at(index).custom) {
        m_stations.removeAt(index);
        saveCustom();
    } else if (m_stations.at(index).extra) {
        QSettings settings(appSettingsFile(), QSettings::IniFormat);
        QStringList shown = settings.value(profileKey(QStringLiteral("radio/shown"))).toStringList();
        shown.removeAll(m_stations.at(index).name);
        settings.setValue(profileKey(QStringLiteral("radio/shown")), shown);
    } else {
        QSettings settings(appSettingsFile(), QSettings::IniFormat);
        QStringList hidden = settings.value(profileKey(QStringLiteral("radio/hidden"))).toStringList();
        hidden.append(m_stations.at(index).name);
        settings.setValue(profileKey(QStringLiteral("radio/hidden")), hidden);
    }
    rebuild();
    endResetModel();
    Q_EMIT countChanged();
    ++m_revision;
    Q_EMIT revisionChanged();
}

void RadioModel::restoreHidden()
{
    beginResetModel();
    QSettings(appSettingsFile(), QSettings::IniFormat).remove(profileKey(QStringLiteral("radio/hidden")));
    rebuild();
    endResetModel();
    Q_EMIT countChanged();
}

void RadioModel::searchStations(const QString &query)
{
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        m_results.clear();
        Q_EMIT resultsChanged();
        return;
    }
    QUrl url(QStringLiteral("https://all.api.radio-browser.info/json/stations/search"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("name"), q);
    params.addQueryItem(QStringLiteral("limit"), QStringLiteral("40"));
    params.addQueryItem(QStringLiteral("hidebroken"), QStringLiteral("true"));
    params.addQueryItem(QStringLiteral("order"), QStringLiteral("clickcount"));
    params.addQueryItem(QStringLiteral("reverse"), QStringLiteral("true"));
    url.setQuery(params);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PP Kiosk"));
    m_searching = true;
    Q_EMIT resultsChanged();
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, q] {
        reply->deleteLater();
        m_searching = false;
        m_results.clear();
        const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).array();
        QSet<QString> seen;
        for (const QJsonValue &v : list) {
            const QJsonObject o = v.toObject();
            const QString streamUrl = o.value(QStringLiteral("url_resolved")).toString(o.value(QStringLiteral("url")).toString());
            const QString name = o.value(QStringLiteral("name")).toString().trimmed();
            if (streamUrl.isEmpty() || name.isEmpty() || seen.contains(name.toLower())) {
                continue;
            }
            seen.insert(name.toLower());
            QStringList detail{o.value(QStringLiteral("country")).toString()};
            const QString tags = o.value(QStringLiteral("tags")).toString().section(QLatin1Char(','), 0, 2).replace(QLatin1Char(','), QStringLiteral(", "));
            if (!tags.isEmpty()) {
                detail.append(tags);
            }
            detail.removeAll(QString());
            m_results.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("detail"), detail.join(QStringLiteral(" · "))},
                {QStringLiteral("url"), streamUrl},
                {QStringLiteral("logo"), o.value(QStringLiteral("favicon")).toString()},
                {QStringLiteral("site"), QUrl(o.value(QStringLiteral("homepage")).toString()).host()},
            });
        }
        qInfo() << "Radio search" << q << m_results.size();
        Q_EMIT resultsChanged();
    });
}

bool RadioModel::hasResult(int index) const
{
    if (index < 0 || index >= m_results.size()) {
        return false;
    }
    const QString url = m_results.at(index).toMap().value(QStringLiteral("url")).toString();
    for (int i : m_visible) {
        if (i >= 0 && m_stations.at(i).streams.contains(url)) {
            return true;
        }
    }
    return false;
}

bool RadioModel::addResult(int index)
{
    if (index < 0 || index >= m_results.size() || hasResult(index)) {
        return false;
    }
    const QVariantMap r = m_results.at(index).toMap();
    // Staat de zender al in de ingebouwde lijst (maar uit)? Dan die aanzetten
    const QString url = r.value(QStringLiteral("url")).toString();
    for (const Station &known : std::as_const(m_stations)) {
        if (known.extra && known.streams.contains(url)) {
            QSettings settings(appSettingsFile(), QSettings::IniFormat);
            QStringList shown = settings.value(profileKey(QStringLiteral("radio/shown"))).toStringList();
            shown.append(known.name);
            settings.setValue(profileKey(QStringLiteral("radio/shown")), shown);
            beginResetModel();
            rebuild();
            endResetModel();
            Q_EMIT countChanged();
            qInfo() << "Radio added" << known.name << url;
            return true;
        }
    }
    Station s;
    s.name = r.value(QStringLiteral("name")).toString();
    s.slogan = r.value(QStringLiteral("detail")).toString();
    s.site = r.value(QStringLiteral("site")).toString();
    s.streams = {r.value(QStringLiteral("url")).toString()};
    s.logoUrl = r.value(QStringLiteral("logo")).toString();
    static const QStringList accents{QStringLiteral("#2f4a6b"), QStringLiteral("#5b2f6b"), QStringLiteral("#2f6b55"),
                                     QStringLiteral("#6b4a2f"), QStringLiteral("#6b2f3a"), QStringLiteral("#3a3f6b")};
    s.accent = accents.at(qHash(s.name) % accents.size());
    s.custom = true;
    beginResetModel();
    m_stations.append(s);
    rebuild();
    endResetModel();
    saveCustom();
    Q_EMIT countChanged();
    qInfo() << "Radio added" << s.name << s.streams.first();
    fetchLogos();
    return true;
}

int RadioModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();   // inclusief tegel "Radiozender toevoegen"
}

QVariant RadioModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= count()) {
        return {};
    }
    switch (role) {
    case NameRole:
        return isAddTile(row) ? tr("Radiozender toevoegen") : st(row).name;
    case IconRole:
        return iconAt(row);
    case AccentRole:
        return accentAt(row);
    case PathRole:
        return pathAt(row);
    }
    return {};
}

QHash<int, QByteArray> RadioModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "iconSource"}, {AccentRole, "accent"}, {PathRole, "path"}};
}

QString RadioModel::nameAt(int row) const
{
    if (isAddTile(row)) {
        return tr("Radiozender toevoegen");
    }
    return isStation(row) ? st(row).name : QString();
}

QString RadioModel::iconAt(int row) const
{
    if (isAddTile(row)) {
        return QStringLiteral("glyph:") + QChar(0xE710);   // plus
    }
    const QString file = logoFile(row);
    if (!file.isEmpty() && QFile::exists(file)) {
        return QUrl::fromLocalFile(file).toString();
    }
    return QStringLiteral("glyph:") + QChar(0xE7F6); // radio-pictogram (Segoe Fluent Icons)
}

QString RadioModel::accentAt(int row) const
{
    return isStation(row) ? st(row).accent : QStringLiteral("#2b2e35");
}

QString RadioModel::subtitleAt(int row) const
{
    if (isAddTile(row)) {
        return tr("Zoek tussen ruim 40.000 zenders");
    }
    // Slogans staan in het Nederlands in de lijst; in het Engels via de vertaling (context RadioSlogans)
    return isStation(row) ? QCoreApplication::translate("RadioSlogans", st(row).slogan.toUtf8().constData()) : QString();
}

QStringList RadioModel::streamsAt(int row) const
{
    return isStation(row) ? st(row).streams : QStringList();
}

bool RadioModel::launch(int row)
{
    if (isAddTile(row)) {
        Q_EMIT addRequested();
        return true;
    }
    if (row < 0 || row >= count()) {
        return false;
    }
    qInfo() << "Radio" << st(row).name;
    Q_EMIT playRequested(row);
    return true;
}

QString RadioModel::logoFile(int row) const
{
    if (!isStation(row)) {
        return {};
    }
    return logoFileFor(st(row));
}

QString RadioModel::logoFileFor(const Station &s) const
{
    QString id = s.name.toLower();
    if (s.custom) {
        id = QString::fromLatin1(QCryptographicHash::hash(s.streams.value(0).toUtf8(), QCryptographicHash::Md5).toHex().left(12));
    }
    id.replace(QLatin1Char(' '), QLatin1Char('-'));
    return m_dataDir + QStringLiteral("/images/radio-") + id + QStringLiteral(".png");
}

// Logo eenmalig ophalen (pictogram van de site van de zender) en lokaal bewaren
void RadioModel::fetchLogos()
{
    QDir().mkpath(m_dataDir + QStringLiteral("/images"));
    for (const Station &station : std::as_const(m_stations)) {
        const QString file = logoFileFor(station);
        if (QFile::exists(file)) {
            continue;
        }
        QUrl source;
        if (station.custom && station.logoUrl.startsWith(QLatin1String("http"))) {
            source = QUrl(station.logoUrl);
        } else if (!station.site.isEmpty()) {
            source = QUrl(QStringLiteral("https://www.google.com/s2/favicons"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("sz"), QStringLiteral("128"));
            query.addQueryItem(QStringLiteral("domain"), station.site);
            source.setQuery(query);
        } else {
            continue;
        }
        QNetworkRequest request(source);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_network.get(request);
        const QString name = station.name;
        connect(reply, &QNetworkReply::finished, this, [this, reply, file, name] {
            reply->deleteLater();
            QImage image;
            if (reply->error() != QNetworkReply::NoError || !image.loadFromData(reply->readAll()) || image.width() < 32) {
                qInfo() << "No radio logo for" << name;
                if (reply->error() == QNetworkReply::NoError && name == QLatin1String("Simone FM")) {
                    QFile::copy(QStringLiteral(":/assets/logos/simone-fm.png"), file);
                    QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner);
                    const int r = rowOfName(name);
                    if (r >= 0) {
                        Q_EMIT dataChanged(index(r), index(r), {IconRole});
                    }
                    ++m_revision;
                    Q_EMIT revisionChanged();
                }
                return;
            }
            image.save(file, "PNG");
            const int row = rowOfName(name);
            if (row >= 0) {
                Q_EMIT dataChanged(index(row), index(row), {IconRole});
            }
            ++m_revision;
            Q_EMIT revisionChanged();
        });
    }
}

// Programma-info van de NPO-zenders. De sites van de zenders geven open JSON (geen sleutel nodig);
// de oude Backstage-API voor radio bestaat niet meer.
void RadioModel::requestProgramme(int row)
{
    const int token = ++m_programmeToken;
    QString host;
    QString name;
    if (isStation(row)) {
        name = st(row).name;
        const QString site = st(row).site;
        if (site.contains(QLatin1String("npoklassiek"))) {
            host = QStringLiteral("www.nporadio4.nl");
        } else if (site.contains(QLatin1String("nporadio1.nl")) || site.contains(QLatin1String("nporadio2.nl"))
                   || site.contains(QLatin1String("nporadio5.nl")) || site.contains(QLatin1String("npo3fm.nl"))
                   || site.contains(QLatin1String("funx.nl"))) {
            host = site;
        }
    }
    if (host.isEmpty()) {
        if (!m_programme.isEmpty()) {
            m_programme.clear();
            Q_EMIT programmeChanged();
        }
        return;
    }
    QNetworkRequest request(QUrl(QStringLiteral("https://%1/api/broadcasts").arg(host)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PP Kiosk"));
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, token, name] {
        reply->deleteLater();
        if (token != m_programmeToken) {
            return;   // intussen een andere zender
        }
        QVariantMap result;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonArray items = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("data")).toArray();
            QTimeZone zone(QByteArrayLiteral("Europe/Amsterdam"));
            auto seconds = [&zone](const QString &text) -> qint64 {
                QDateTime t = QDateTime::fromString(text.left(19), Qt::ISODate);
                if (!t.isValid()) {
                    return 0;
                }
                t.setTimeZone(zone.isValid() ? zone : QTimeZone::LocalTime);
                return t.toSecsSinceEpoch();
            };
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            for (int i = 0; i < items.size(); ++i) {
                const QJsonObject o = items.at(i).toObject();
                const qint64 start = seconds(o.value(QStringLiteral("startdatetime")).toString());
                qint64 stop = seconds(o.value(QStringLiteral("stopdatetime")).toString());
                if (i + 1 < items.size()) {
                    const qint64 nextStart = seconds(items.at(i + 1).toObject().value(QStringLiteral("startdatetime")).toString());
                    if (stop <= start || stop > nextStart) {
                        stop = nextStart;
                    }
                }
                if (start == 0 || start > now || (stop != 0 && stop <= now)) {
                    continue;
                }
                result = {{QStringLiteral("station"), name},
                          {QStringLiteral("title"), o.value(QStringLiteral("title")).toString()},
                          {QStringLiteral("presenters"), o.value(QStringLiteral("presenters")).toString()},
                          {QStringLiteral("broadcaster"), o.value(QStringLiteral("broadcaster")).toString()},
                          {QStringLiteral("start"), double(start)},
                          {QStringLiteral("stop"), double(stop)}};
                if (i + 1 < items.size()) {
                    const QJsonObject n = items.at(i + 1).toObject();
                    result.insert(QStringLiteral("nextTitle"), n.value(QStringLiteral("title")).toString());
                    result.insert(QStringLiteral("nextStart"), double(seconds(n.value(QStringLiteral("startdatetime")).toString())));
                }
                break;
            }
        } else {
            qWarning() << "Radio programme failed" << name << reply->errorString();
        }
        qInfo() << "Radio programme" << (result.isEmpty() ? "none" : "ok") << name << result.value(QStringLiteral("title")).toString();
        m_programme = result;
        Q_EMIT programmeChanged();
    });
}
