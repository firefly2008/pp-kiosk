// SPDX-License-Identifier: GPL-2.0-or-later
#include "agenda.h"
#include "menuboard.h"
#include "radiomodel.h"
#include "paths.h"
#include <tuple>

static QJsonArray defaultRelax();
#include <QSettings>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJSValue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

namespace {

bool validDate(const QString &t)
{
    return QDate::fromString(t, Qt::ISODate).isValid() && t.size() == 10;
}

// Alleen een kale bestandsnaam uit de map met picto's (geen paden)
QString cleanPicto(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 ._-]{0,80}\\.(png|jpg|jpeg|webp|svg)$"), QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch() ? name : QString();
}

QString cleanColor(const QString &c, const QString &fallback)
{
    static const QRegularExpression re(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return re.match(c).hasMatch() ? c : fallback;
}

bool validTime(const QString &t)
{
    static const QRegularExpression re(QStringLiteral("^([01]\\d|2[0-3]):[0-5]\\d$"));
    return re.match(t).hasMatch();
}

QJsonObject item(const char *start, const char *end, const char *title, const char *icon, const char *color)
{
    return QJsonObject{
        {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("start"), QString::fromLatin1(start)},
        {QStringLiteral("end"), QString::fromLatin1(end)},
        {QStringLiteral("title"), QString::fromUtf8(title)},
        {QStringLiteral("icon"), QString::fromUtf8(icon)},
        {QStringLiteral("color"), QString::fromLatin1(color)},
        {QStringLiteral("note"), QString()},
        {QStringLiteral("days"), QJsonArray{1, 2, 3, 4, 5, 6, 7}},
    };
}

} // namespace

Agenda::Agenda(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_file(dataDir + QStringLiteral("/agenda.json"))
    , m_pictoDir(dataDir + QStringLiteral("/pictos"))
{
    QDir().mkpath(m_pictoDir);
    load();
    recompute();
    m_timer.setInterval(15000);
    connect(&m_timer, &QTimer::timeout, this, &Agenda::recompute);
    m_timer.start();
}

QJsonObject Agenda::defaultAgenda()
{
    // Voorbeeld-dag, zodat het scherm na installatie niet leeg is
    return QJsonObject{
        {QStringLiteral("version"), 3},
        {QStringLiteral("items"), QJsonArray{
             item("07:30", "08:00", "Opstaan en aankleden", "👕", "#A7D8F0"),
             item("08:00", "08:45", "Ontbijt", "🍞", "#F6C85F"),
             item("09:00", "12:00", "Dagbesteding", "🛠️", "#8CC084"),
             item("12:00", "13:00", "Lunch", "🥪", "#F6C85F"),
             item("13:00", "14:00", "Rusten", "🛋️", "#C3B1E1"),
             item("14:00", "16:00", "Wandelen of activiteit", "🚶", "#8CC084"),
             item("16:00", "17:00", "Vrije tijd", "🎵", "#F4A6A0"),
             item("17:30", "18:30", "Avondeten", "🍽️", "#F6C85F"),
             item("19:00", "21:00", "Tv kijken", "📺", "#A7D8F0"),
             item("22:00", "22:30", "Naar bed", "🛏️", "#C3B1E1"),
         }},
        {QStringLiteral("messages"), QJsonArray{}},
        {QStringLiteral("menu"), QJsonArray{
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Zuurkoolstamppot")}, {QStringLiteral("note"), QStringLiteral("met rookworst")}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Macaroni")}, {QStringLiteral("note"), QStringLiteral("met ham en kaas")}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Kip met rijst")}, {QStringLiteral("note"), QStringLiteral("en sperziebonen")}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Erwtensoep")}, {QStringLiteral("note"), QStringLiteral("met roggebrood")}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Vis met friet")}, {QStringLiteral("note"), QStringLiteral("en sla")}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Pannenkoeken")}, {QStringLiteral("note"), QString()}},
             QJsonObject{{QStringLiteral("dish"), QStringLiteral("Hutspot")}, {QStringLiteral("note"), QStringLiteral("met gehaktbal")}},
         }},
        {QStringLiteral("settings"), QJsonObject{
             {QStringLiteral("screensaverMinutes"), 5},
             {QStringLiteral("screensaverPresentation"), QStringLiteral("Menu van de week")},
             {QStringLiteral("dinnerTime"), QStringLiteral("17:00")},
         }},
    };
}

void Agenda::load()
{
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject() && doc.object().value(QStringLiteral("items")).isArray()) {
            m_data = doc.object();
            qInfo() << "Agenda: loaded" << m_data.value(QStringLiteral("items")).toArray().size() << "items";
            // Oude voorbeeldmelding over de beheerpagina niet meer op het startscherm
            {
                QJsonArray messages = m_data.value(QStringLiteral("messages")).toArray();
                for (int i = messages.size() - 1; i >= 0; --i) {
                    if (messages.at(i).toString().startsWith(QStringLiteral("Welkom! Pas deze dag aan via de beheerpagina"))) {
                        messages.removeAt(i);
                        m_data.insert(QStringLiteral("messages"), messages);
                        save();
                    }
                }
            }
            // Versie 3: screensaver standaard na 5 minuten met het menu van de week, diner om 17:00
            if (m_data.value(QStringLiteral("version")).toInt() < 3) {
                QJsonObject settings = m_data.value(QStringLiteral("settings")).toObject();
                if (settings.value(QStringLiteral("screensaverMinutes")).toInt() <= 0) {
                    settings.insert(QStringLiteral("screensaverMinutes"), 5);
                }
                if (settings.value(QStringLiteral("screensaverPresentation")).toString().isEmpty()) {
                    settings.insert(QStringLiteral("screensaverPresentation"), QStringLiteral("Menu van de week"));
                }
                if (!settings.contains(QStringLiteral("dinnerTime"))) {
                    settings.insert(QStringLiteral("dinnerTime"), QStringLiteral("17:00"));
                }
                m_data.insert(QStringLiteral("settings"), settings);
                m_data.insert(QStringLiteral("version"), 3);
                save();
                qInfo() << "Agenda: upgraded to version 3";
            }
            return;
        }
        qWarning() << "Agenda: file unreadable, using example day";
    }
    m_data = defaultAgenda();
    save();
}

void Agenda::save() const
{
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

QByteArray Agenda::toJson() const
{
    QJsonObject out = m_data;
    if (!out.contains(QStringLiteral("relax"))) {
        out.insert(QStringLiteral("relax"), defaultRelax());
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

bool Agenda::setJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        *error = QStringLiteral("Geen geldige gegevens: ") + parseError.errorString();
        return false;
    }
    const QJsonObject in = doc.object();
    QJsonArray items;
    for (const QJsonValue &v : in.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString start = o.value(QStringLiteral("start")).toString();
        const QString end = o.value(QStringLiteral("end")).toString();
        const QString title = o.value(QStringLiteral("title")).toString().trimmed().left(80);
        if (!validTime(start) || (!end.isEmpty() && !validTime(end)) || title.isEmpty()) {
            *error = QStringLiteral("Activiteit \"%1\": vul een titel en tijden als 09:30 in.").arg(title);
            return false;
        }
        QJsonArray days;
        for (const QJsonValue &d : o.value(QStringLiteral("days")).toArray()) {
            const int day = d.toInt();
            if (day >= 1 && day <= 7) {
                days.append(day);
            }
        }
        const QString color = cleanColor(o.value(QStringLiteral("color")).toString(), QStringLiteral("#A7D8F0"));
        const QString date = o.value(QStringLiteral("date")).toString();
        if (!date.isEmpty() && !validDate(date)) {
            *error = QStringLiteral("Activiteit \"%1\": datum klopt niet.").arg(title);
            return false;
        }
        QString id = o.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        items.append(QJsonObject{
            {QStringLiteral("id"), id.left(40)},
            {QStringLiteral("start"), start},
            {QStringLiteral("end"), end},
            {QStringLiteral("title"), title},
            {QStringLiteral("icon"), o.value(QStringLiteral("icon")).toString().left(16)},
            {QStringLiteral("color"), color},
            {QStringLiteral("note"), o.value(QStringLiteral("note")).toString().left(200)},
            {QStringLiteral("days"), days},
            {QStringLiteral("date"), date},
            {QStringLiteral("image"), cleanPicto(o.value(QStringLiteral("image")).toString())},
        });
        if (items.size() > 400) {
            break;
        }
    }
    QJsonArray messages;
    for (const QJsonValue &v : in.value(QStringLiteral("messages")).toArray()) {
        const QString text = v.toString().trimmed().left(300);
        if (!text.isEmpty() && messages.size() < 10) {
            messages.append(text);
        }
    }
    // Weekmenu: 7 dagen (maandag eerst)
    QJsonArray menu;
    const QJsonArray menuIn = in.value(QStringLiteral("menu")).toArray();
    for (int d = 0; d < 7; ++d) {
        const QJsonObject o = menuIn.at(d).toObject();
        menu.append(QJsonObject{
            {QStringLiteral("dish"), o.value(QStringLiteral("dish")).toString().trimmed().left(80)},
            {QStringLiteral("note"), o.value(QStringLiteral("note")).toString().trimmed().left(120)},
            {QStringLiteral("image"), cleanPicto(o.value(QStringLiteral("image")).toString())},
            // Zelf gekozen standaardfoto (leeg = automatisch bij het gerecht)
            {QStringLiteral("food"), MenuBoard::photoNames().contains(o.value(QStringLiteral("food")).toString()) ? o.value(QStringLiteral("food")).toString() : QString()},
        });
    }
    // Verjaardagen (MM-DD) en bijzondere dagen (JJJJ-MM-DD of MM-DD)
    static const QRegularExpression dayRe(QStringLiteral("^(\\d{4}-)?(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01])$"));
    QJsonArray birthdays;
    for (const QJsonValue &v : in.value(QStringLiteral("birthdays")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString().trimmed().left(60);
        QString date = o.value(QStringLiteral("date")).toString().trimmed();
        if (date.size() == 10) {
            date = date.mid(5);   // geboortedatum: alleen maand en dag tellen
        }
        if (name.isEmpty() || !dayRe.match(date).hasMatch() || birthdays.size() >= 200) {
            continue;
        }
        birthdays.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("date"), date}});
    }
    QJsonArray special;
    for (const QJsonValue &v : in.value(QStringLiteral("special")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString title = o.value(QStringLiteral("title")).toString().trimmed().left(80);
        const QString date = o.value(QStringLiteral("date")).toString().trimmed();
        if (title.isEmpty() || !dayRe.match(date).hasMatch() || special.size() >= 200) {
            continue;
        }
        special.append(QJsonObject{{QStringLiteral("title"), title}, {QStringLiteral("date"), date},
                                   {QStringLiteral("icon"), o.value(QStringLiteral("icon")).toString().left(16)}});
    }
    // Ontspanning: eigen links (meezingen, verhalen, dieren, stoelgym) en muziek-favorieten
    static const QStringList relaxGroups{QStringLiteral("zingen"), QStringLiteral("verhalen"), QStringLiteral("dieren"), QStringLiteral("gym")};
    auto okUrl = [](const QString &u) { return u.startsWith(QLatin1String("https://")) || u.startsWith(QLatin1String("http://")); };
    QJsonArray relax;
    for (const QJsonValue &v : in.value(QStringLiteral("relax")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString title = o.value(QStringLiteral("title")).toString().trimmed().left(80);
        const QString url = o.value(QStringLiteral("url")).toString().trimmed().left(600);
        const QString group = o.value(QStringLiteral("group")).toString();
        if (title.isEmpty() || !okUrl(url) || !relaxGroups.contains(group) || relax.size() >= 120) {
            continue;
        }
        relax.append(QJsonObject{{QStringLiteral("group"), group}, {QStringLiteral("title"), title}, {QStringLiteral("url"), url},
                                 {QStringLiteral("icon"), o.value(QStringLiteral("icon")).toString().left(16)},
                                 {QStringLiteral("kids"), o.value(QStringLiteral("kids")).toBool(false)}});
    }
    QJsonArray favorites;
    for (const QJsonValue &v : in.value(QStringLiteral("favorites")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString title = o.value(QStringLiteral("title")).toString().trimmed().left(80);
        const QString url = o.value(QStringLiteral("url")).toString().trimmed().left(600);
        if (title.isEmpty() || (!url.isEmpty() && !okUrl(url)) || favorites.size() >= 60) {
            continue;
        }
        favorites.append(QJsonObject{{QStringLiteral("title"), title}, {QStringLiteral("url"), url},
                                     {QStringLiteral("artist"), o.value(QStringLiteral("artist")).toString().trimmed().left(80)},
                                     {QStringLiteral("cover"), cleanPicto(o.value(QStringLiteral("cover")).toString())}});
    }
    // Scherminstellingen
    const QJsonObject si = in.value(QStringLiteral("settings")).toObject();
    const QString nightFrom = si.value(QStringLiteral("nightFrom")).toString();
    const QString nightTo = si.value(QStringLiteral("nightTo")).toString();
    const double scale = si.value(QStringLiteral("textScale")).toDouble(1.0);
    const QString nightMode = si.value(QStringLiteral("nightMode")).toString();
    const QString dinnerTime = si.value(QStringLiteral("dinnerTime")).toString();
    const QJsonObject music = si.value(QStringLiteral("music")).toObject();
    QJsonObject bgIn = si.value(QStringLiteral("background")).toObject();
    const QString bgType = bgIn.value(QStringLiteral("type")).toString();
    // Foto van de dag: de foto kiest de kiosk zelf, dus de huidige foto en fotograaf houden
    if (bgType == QLatin1String("daily")) {
        const QJsonObject cur = m_data.value(QStringLiteral("settings")).toObject().value(QStringLiteral("background")).toObject();
        bgIn.insert(QStringLiteral("photo"), cur.value(QStringLiteral("photo")));
        bgIn.insert(QStringLiteral("credit"), cur.value(QStringLiteral("credit")));
    }
    const QString bgColor = bgIn.value(QStringLiteral("color")).toString();
    static const QRegularExpression colorRe(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    const QJsonObject settings{
        {QStringLiteral("name"), si.value(QStringLiteral("name")).toString().trimmed().left(40)},
        {QStringLiteral("speak"), si.value(QStringLiteral("speak")).toBool(false)},
        {QStringLiteral("chime"), si.value(QStringLiteral("chime")).toBool(true)},
        {QStringLiteral("textScale"), qBound(1.0, scale, 1.5)},
        {QStringLiteral("contrast"), si.value(QStringLiteral("contrast")).toBool(false)},
        {QStringLiteral("screensaverMinutes"), qBound(0, si.value(QStringLiteral("screensaverMinutes")).toInt(0), 240)},
        {QStringLiteral("radioSaverMinutes"), qBound(0, si.value(QStringLiteral("radioSaverMinutes")).toInt(1), 60)},
        {QStringLiteral("screensaverPresentation"), si.value(QStringLiteral("screensaverPresentation")).toString().left(60)},
        {QStringLiteral("nightFrom"), validTime(nightFrom) ? nightFrom : QString()},
        {QStringLiteral("nightTo"), validTime(nightTo) ? nightTo : QString()},
        {QStringLiteral("nightMode"), nightMode == QLatin1String("black") ? nightMode : QStringLiteral("dim")},
        {QStringLiteral("dinnerTime"), validTime(dinnerTime) ? dinnerTime : QStringLiteral("17:00")},
        {QStringLiteral("news"), si.value(QStringLiteral("news")).toBool(true)},
        {QStringLiteral("games"), si.value(QStringLiteral("games")).toBool(true)},
        {QStringLiteral("glassTransparency"), qBound(50, si.value(QStringLiteral("glassTransparency")).toInt(80), 95)},
        {QStringLiteral("voice"), si.value(QStringLiteral("voice")).toBool(false)},
        {QStringLiteral("remindMinutes"), qBound(0, si.value(QStringLiteral("remindMinutes")).toInt(0), 60)},
        {QStringLiteral("remindSound"), si.value(QStringLiteral("remindSound")).toBool(true)},
        {QStringLiteral("weatherTile"), si.value(QStringLiteral("weatherTile")).toBool(false)},
        {QStringLiteral("holidays"), si.value(QStringLiteral("holidays")).toBool(true)},
        {QStringLiteral("bigButtons"), si.value(QStringLiteral("bigButtons")).toBool(false)},
        {QStringLiteral("seasonDecor"), si.value(QStringLiteral("seasonDecor")).toBool(false)},
        {QStringLiteral("clockStyle"), si.value(QStringLiteral("clockStyle")).toString() == QLatin1String("analoog") ? QStringLiteral("analoog") : QStringLiteral("digitaal")},
        {QStringLiteral("dayBig"), si.value(QStringLiteral("dayBig")).toBool(false)},
        {QStringLiteral("party"), si.value(QStringLiteral("party")).toBool(true)},
        {QStringLiteral("nightDim"), qBound(50, si.value(QStringLiteral("nightDim")).toInt(88), 95)},
        {QStringLiteral("watchdog"), si.value(QStringLiteral("watchdog")).toBool(true)},
        {QStringLiteral("offline"), si.value(QStringLiteral("offline")).toBool(false)},
        {QStringLiteral("relax"), si.value(QStringLiteral("relax")).toBool(true)},
        {QStringLiteral("readAloud"), si.value(QStringLiteral("readAloud")).toBool(true)},
        {QStringLiteral("talkTile"), si.value(QStringLiteral("talkTile")).toBool(false)},
        {QStringLiteral("talkAlert"), si.value(QStringLiteral("talkAlert")).toBool(true)},
        {QStringLiteral("travelTile"), si.value(QStringLiteral("travelTile")).toBool(false)},
        {QStringLiteral("ovStop"), si.value(QStringLiteral("ovStop")).toString().trimmed().left(40)},
        {QStringLiteral("ovStopName"), si.value(QStringLiteral("ovStopName")).toString().trimmed().left(80)},
        {QStringLiteral("nsStation"), si.value(QStringLiteral("nsStation")).toString().trimmed().left(10)},
        {QStringLiteral("nsStationName"), si.value(QStringLiteral("nsStationName")).toString().trimmed().left(60)},
        {QStringLiteral("trafficUrl"), si.value(QStringLiteral("trafficUrl")).toString().startsWith(QLatin1String("https://")) ? si.value(QStringLiteral("trafficUrl")).toString().left(300) : QString()},
        {QStringLiteral("tilePres"), si.value(QStringLiteral("tilePres")).toBool(true)},
        {QStringLiteral("tileStream"), si.value(QStringLiteral("tileStream")).toBool(true)},
        {QStringLiteral("tileLinks"), si.value(QStringLiteral("tileLinks")).toBool(true)},
        {QStringLiteral("saverFade"), si.value(QStringLiteral("saverFade")).toBool(true)},
        {QStringLiteral("radioRegion"), RadioModel::regionalNames().contains(si.value(QStringLiteral("radioRegion")).toString()) ? si.value(QStringLiteral("radioRegion")).toString() : QString()},
        {QStringLiteral("theme"), (si.value(QStringLiteral("theme")).toString() == QLatin1String("volwassen") || si.value(QStringLiteral("theme")).toString() == QLatin1String("glas"))
                                          ? si.value(QStringLiteral("theme")).toString() : QStringLiteral("standaard")},
        {QStringLiteral("background"), QJsonObject{
             {QStringLiteral("type"), (bgType == QLatin1String("color") || bgType == QLatin1String("photo") || bgType == QLatin1String("daily")) ? bgType : QStringLiteral("daypart")},
             // Foto van de dag: waar de foto over gaat (zoekwoord voor Unsplash)
             {QStringLiteral("topic"), bgIn.value(QStringLiteral("topic")).toString().trimmed().left(60)},
             {QStringLiteral("color"), colorRe.match(bgColor).hasMatch() ? bgColor : QStringLiteral("#ECE6DA")},
             {QStringLiteral("photo"), QFileInfo(bgIn.value(QStringLiteral("photo")).toString()).fileName().left(120)},
             // Naam van de fotograaf (Unsplash), staat klein in de hoek van het scherm
             {QStringLiteral("credit"), bgIn.value(QStringLiteral("credit")).toString().trimmed().left(80)},
         }},
        {QStringLiteral("music"), QJsonObject{
             {QStringLiteral("ochtend"), music.value(QStringLiteral("ochtend")).toString().trimmed().left(80)},
             {QStringLiteral("middag"), music.value(QStringLiteral("middag")).toString().trimmed().left(80)},
             {QStringLiteral("avond"), music.value(QStringLiteral("avond")).toString().trimmed().left(80)},
             {QStringLiteral("auto"), music.value(QStringLiteral("auto")).toBool(false)},
         }},
    };
    m_data = QJsonObject{{QStringLiteral("version"), 3}, {QStringLiteral("items"), items}, {QStringLiteral("messages"), messages},
                         {QStringLiteral("menu"), menu}, {QStringLiteral("birthdays"), birthdays}, {QStringLiteral("special"), special},
                         {QStringLiteral("relax"), relax}, {QStringLiteral("favorites"), favorites},
                         {QStringLiteral("settings"), settings}};
    if (!in.contains(QStringLiteral("relax"))) {
        m_data.remove(QStringLiteral("relax"));   // geen eigen lijst: de standaardlinks
    }
    save();
    m_stamp.clear();
    recompute();
    qInfo() << "Agenda: saved" << items.size() << "items";
    return true;
}

// Standaardlinks voor Ontspanning (zolang er in het beheer nog niets is opgeslagen)
static QJsonArray defaultRelax()
{
    auto yt = [](const char *q) { return QStringLiteral("https://www.youtube.com/results?search_query=") + QString::fromUtf8(q); };
    auto item = [](const char *group, const char *icon, const char *title, const QString &url, bool kids = false) {
        return QJsonObject{{QStringLiteral("group"), QString::fromUtf8(group)}, {QStringLiteral("icon"), QString::fromUtf8(icon)},
                           {QStringLiteral("title"), QString::fromUtf8(title)}, {QStringLiteral("url"), url}, {QStringLiteral("kids"), kids}};
    };
    return QJsonArray{
        item("zingen", "🎤", "Meezingen: oude liedjes met tekst", yt("meezingen+oude+nederlandse+liedjes+met+tekst")),
        item("zingen", "🎶", "Karaoke: Nederlandse liedjes", yt("karaoke+nederlandstalig+met+tekst")),
        item("zingen", "🧸", "Kinderliedjes om mee te zingen", yt("kinderliedjes+meezingen+met+tekst"), true),
        item("verhalen", "📖", "Voorleesverhalen voor kinderen", yt("voorleesverhaal+voor+kinderen+nederlands"), true),
        item("verhalen", "🏰", "Sprookjes luisteren", yt("sprookjes+luisterboek+nederlands"), true),
        item("verhalen", "🎧", "Luisterboeken voor volwassenen", yt("luisterboek+nederlands+volledig")),
        item("verhalen", "📚", "Gratis luisterboeken (LibriVox)", QStringLiteral("https://librivox.org/")),
        item("dieren", "🦁", "Dieren live bekijken", QStringLiteral("https://explore.org/livecams")),
        item("dieren", "🐧", "Dierentuin-webcams", yt("live+webcam+dierentuin")),
        item("gym", "🪑", "Stoelgym video", yt("stoelgym+ouderen+nederlands")),
    };
}

QVariantList Agenda::relaxLinks() const
{
    return (m_data.contains(QStringLiteral("relax")) ? m_data.value(QStringLiteral("relax")).toArray() : defaultRelax()).toVariantList();
}

QVariantList Agenda::favorites() const
{
    QVariantList out;
    for (const QJsonValue &v : m_data.value(QStringLiteral("favorites")).toArray()) {
        QVariantMap m = v.toObject().toVariantMap();
        const QString cover = m.value(QStringLiteral("cover")).toString();
        const QString path = m_pictoDir + QLatin1Char('/') + cover;
        m.insert(QStringLiteral("coverUrl"), !cover.isEmpty() && QFileInfo::exists(path) ? QUrl::fromLocalFile(path).toString() : QString());
        out.append(m);
    }
    return out;
}

QStringList Agenda::messages() const
{
    QStringList out;
    for (const QJsonValue &v : m_data.value(QStringLiteral("messages")).toArray()) {
        out.append(v.toString());
    }
    // Afspraken voor de hele dag uit de gekoppelde agenda
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    for (const QJsonObject &o : m_external) {
        if (o.value(QStringLiteral("allDay")).toBool() && o.value(QStringLiteral("date")).toString() == today) {
            out.append(QStringLiteral("📅 Vandaag: ") + o.value(QStringLiteral("title")).toString());
        }
    }
    return out;
}

QVariantMap Agenda::settings() const
{
    QVariantMap m = m_data.value(QStringLiteral("settings")).toObject().toVariantMap();
    // Standaardwaarden voor een agenda van een oudere versie
    if (!m.contains(QStringLiteral("textScale"))) {
        m.insert(QStringLiteral("textScale"), 1.0);
    }
    if (!m.contains(QStringLiteral("chime"))) {
        m.insert(QStringLiteral("chime"), true);
    }
    if (!m.contains(QStringLiteral("nightMode"))) {
        m.insert(QStringLiteral("nightMode"), QStringLiteral("dim"));
    }
    if (!m.contains(QStringLiteral("games"))) {
        m.insert(QStringLiteral("games"), true);
    }
    if (!m.contains(QStringLiteral("music"))) {
        m.insert(QStringLiteral("music"), QVariantMap{{QStringLiteral("auto"), false}});
    }
    if (!m.contains(QStringLiteral("voice"))) {
        m.insert(QStringLiteral("voice"), false);
    }
    if (!m.contains(QStringLiteral("theme"))) {
        m.insert(QStringLiteral("theme"), QStringLiteral("standaard"));
    }
    // Eigen achtergrondfoto: volledige adres voor het kioskscherm
    const QVariantMap bg = m.value(QStringLiteral("background")).toMap();
    const QString photo = bg.value(QStringLiteral("photo")).toString();
    const QString photoPath = m_pictoDir + QLatin1Char('/') + photo;
    m.insert(QStringLiteral("backgroundUrl"), (bg.value(QStringLiteral("type")).toString() == QLatin1String("photo") || bg.value(QStringLiteral("type")).toString() == QLatin1String("daily")) && !photo.isEmpty()
                                                     && QFileInfo::exists(photoPath)
                                                 ? QUrl::fromLocalFile(photoPath).toString() : QString());
    if (!m.contains(QStringLiteral("news"))) {
        m.insert(QStringLiteral("news"), true);
    }
    if (!m.contains(QStringLiteral("dinnerTime"))) {
        m.insert(QStringLiteral("dinnerTime"), QStringLiteral("17:00"));
    }
    static const QList<QPair<QString, QVariant>> extraDefaults{
        {QStringLiteral("remindMinutes"), 0}, {QStringLiteral("remindSound"), true}, {QStringLiteral("weatherTile"), false},
        {QStringLiteral("holidays"), true}, {QStringLiteral("bigButtons"), false}, {QStringLiteral("seasonDecor"), false},
        {QStringLiteral("clockStyle"), QStringLiteral("digitaal")}, {QStringLiteral("dayBig"), false}, {QStringLiteral("party"), true},
        {QStringLiteral("nightDim"), 88}, {QStringLiteral("watchdog"), true}, {QStringLiteral("offline"), false},
        {QStringLiteral("relax"), true}, {QStringLiteral("saverFade"), true}, {QStringLiteral("radioRegion"), QString()},
        {QStringLiteral("readAloud"), true}, {QStringLiteral("talkTile"), false}, {QStringLiteral("talkAlert"), true}, {QStringLiteral("travelTile"), false},
        {QStringLiteral("tilePres"), true}, {QStringLiteral("tileStream"), true}, {QStringLiteral("tileLinks"), true}};
    for (const auto &d : extraDefaults) {
        if (!m.contains(d.first)) m.insert(d.first, d.second);
    }
    if (!m.contains(QStringLiteral("radioSaverMinutes"))) {
        m.insert(QStringLiteral("radioSaverMinutes"), 1);
    }
    if (!m.contains(QStringLiteral("screensaverMinutes"))) {
        m.insert(QStringLiteral("screensaverMinutes"), 5);
        m.insert(QStringLiteral("screensaverPresentation"), QStringLiteral("Menu van de week"));
    }
    return m;
}

// Nederlandse feestdagen en bekende dagen. Pasen berekend met het algoritme van Meeus.
QPair<QString, QString> Agenda::holidayFor(const QDate &d)
{
    const int y = d.year();
    const int a = y % 19, b = y / 100, c = y % 100, dd = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    const int h = (19 * a + b - dd - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const QDate easter(y, (h + l - 7 * m + 114) / 31, ((h + l - 7 * m + 114) % 31) + 1);
    QDate king(y, 4, 27);
    if (king.dayOfWeek() == 7) king = king.addDays(-1);
    QDate mother(y, 5, 1);
    mother = mother.addDays((7 - mother.dayOfWeek()) % 7 + 7);
    QDate father(y, 6, 1);
    father = father.addDays((7 - father.dayOfWeek()) % 7 + 14);
    const QList<std::tuple<QDate, QString, QString>> list{
        {QDate(y, 1, 1), QStringLiteral("Gelukkig nieuwjaar!"), QStringLiteral("🎆")},
        {QDate(y, 2, 14), QStringLiteral("Valentijnsdag"), QStringLiteral("❤️")},
        {easter.addDays(-2), QStringLiteral("Goede Vrijdag"), QStringLiteral("✝️")},
        {easter, QStringLiteral("Eerste Paasdag"), QStringLiteral("🐣")},
        {easter.addDays(1), QStringLiteral("Tweede Paasdag"), QStringLiteral("🐣")},
        {king, QStringLiteral("Koningsdag"), QStringLiteral("👑")},
        {QDate(y, 5, 4), QStringLiteral("Dodenherdenking"), QStringLiteral("🕯️")},
        {QDate(y, 5, 5), QStringLiteral("Bevrijdingsdag"), QStringLiteral("🇳🇱")},
        {mother, QStringLiteral("Moederdag"), QStringLiteral("💐")},
        {easter.addDays(39), QStringLiteral("Hemelvaartsdag"), QStringLiteral("☁️")},
        {easter.addDays(49), QStringLiteral("Eerste Pinksterdag"), QStringLiteral("🕊️")},
        {easter.addDays(50), QStringLiteral("Tweede Pinksterdag"), QStringLiteral("🕊️")},
        {father, QStringLiteral("Vaderdag"), QStringLiteral("👔")},
        {QDate(y, 10, 31), QStringLiteral("Halloween"), QStringLiteral("🎃")},
        {QDate(y, 11, 11), QStringLiteral("Sint-Maarten"), QStringLiteral("🏮")},
        {QDate(y, 12, 5), QStringLiteral("Sinterklaasavond"), QStringLiteral("🎁")},
        {QDate(y, 12, 24), QStringLiteral("Kerstavond"), QStringLiteral("🎄")},
        {QDate(y, 12, 25), QStringLiteral("Eerste Kerstdag"), QStringLiteral("🎄")},
        {QDate(y, 12, 26), QStringLiteral("Tweede Kerstdag"), QStringLiteral("🎄")},
        {QDate(y, 12, 31), QStringLiteral("Oudejaarsavond"), QStringLiteral("🎆")},
    };
    for (const auto &t : list) {
        if (std::get<0>(t) == d) return {std::get<1>(t), std::get<2>(t)};
    }
    return {};
}

QVariantList Agenda::itemsFor(const QDate &date) const
{
    const QString iso = date.toString(Qt::ISODate);
    const int weekday = date.dayOfWeek();
    QList<QJsonObject> list;
    for (const QJsonValue &v : m_data.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString only = o.value(QStringLiteral("date")).toString();
        if (only.isEmpty() ? o.value(QStringLiteral("days")).toArray().contains(weekday) : only == iso) {
            QJsonObject copy = o;
            const QString image = o.value(QStringLiteral("image")).toString();
            const QString path = m_pictoDir + QLatin1Char('/') + image;
            copy.insert(QStringLiteral("imageUrl"), !image.isEmpty() && QFileInfo::exists(path) ? QUrl::fromLocalFile(path).toString() : QString());
            list.append(copy);
        }
    }
    for (const QJsonObject &o : m_external) {
        if (o.value(QStringLiteral("date")).toString() == iso && !o.value(QStringLiteral("allDay")).toBool()) {
            list.append(o);
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("start")).toString() < b.value(QStringLiteral("start")).toString();
    });
    QVariantList out;
    for (const QJsonObject &o : list) {
        out.append(o.toVariantMap());
    }
    return out;
}

void Agenda::setTestTime(const QString &time)
{
    m_testTime = validTime(time) ? time : QString();
    m_stamp.clear();
    recompute();
}

void Agenda::setExternal(const QList<QJsonObject> &items)
{
    m_external = items;
    m_stamp.clear();
    recompute();
    Q_EMIT changed();
}

void Agenda::setBackgroundPhoto(const QString &name, const QString &credit)
{
    QJsonObject settings = m_data.value(QStringLiteral("settings")).toObject();
    QJsonObject bg = settings.value(QStringLiteral("background")).toObject();
    bg.insert(QStringLiteral("photo"), QFileInfo(name).fileName());
    bg.insert(QStringLiteral("credit"), credit.left(80));
    settings.insert(QStringLiteral("background"), bg);
    m_data.insert(QStringLiteral("settings"), settings);
    save();
    Q_EMIT changed();
}

void Agenda::setTestSetting(const QString &key, const QVariant &value)
{
    QJsonObject settings = m_data.value(QStringLiteral("settings")).toObject();
    QVariant v = value;
    if (v.userType() == qMetaTypeId<QJSValue>()) {
        v = v.value<QJSValue>().toVariant();
    }
    settings.insert(key, QJsonValue::fromVariant(v));
    m_data.insert(QStringLiteral("settings"), settings);
    Q_EMIT changed();
}

QString Agenda::testPhoto()
{
    const QString name = QStringLiteral("test-achtergrond.jpg");
    QFile::remove(m_pictoDir + QLatin1Char('/') + name);
    QFile::copy(QStringLiteral(":/food/salade.jpg"), m_pictoDir + QLatin1Char('/') + name);
    QFile::setPermissions(m_pictoDir + QLatin1Char('/') + name, QFile::ReadOwner | QFile::WriteOwner);
    return name;
}

void Agenda::recompute()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString nowTime = m_testTime.isEmpty() ? now.toString(QStringLiteral("HH:mm")) : m_testTime;
    const QString stamp = now.date().toString(Qt::ISODate) + nowTime;
    if (stamp == m_stamp) {
        return;
    }
    m_stamp = stamp;
    const QDate today = now.date();
    const int weekday = today.dayOfWeek();

    QList<QJsonObject> list;
    for (const QVariant &v : itemsFor(today)) {
        list.append(QJsonObject::fromVariantMap(v.toMap()));
    }

    // Dagdeel (voor kleur en begroeting op het startscherm)
    const int hour = nowTime.left(2).toInt();
    m_daypart = hour < 6 ? QStringLiteral("nacht") : hour < 12 ? QStringLiteral("ochtend") : hour < 18 ? QStringLiteral("middag") : QStringLiteral("avond");

    // Eten van vandaag
    const QJsonObject dish = m_data.value(QStringLiteral("menu")).toArray().at(weekday - 1).toObject();
    m_menu = dish.toVariantMap();
    const QString dishImage = dish.value(QStringLiteral("image")).toString();
    m_menu.insert(QStringLiteral("imageUrl"), !dishImage.isEmpty() && QFileInfo::exists(m_pictoDir + QLatin1Char('/') + dishImage)
                                                 ? QUrl::fromLocalFile(m_pictoDir + QLatin1Char('/') + dishImage).toString() : QString());
    // Foto bij "Diner van de dag": de eigen foto, anders een standaardfoto die bij het gerecht past
    const QString dishName = dish.value(QStringLiteral("dish")).toString();
    m_menu.insert(QStringLiteral("photoUrl"), !m_menu.value(QStringLiteral("imageUrl")).toString().isEmpty() ? m_menu.value(QStringLiteral("imageUrl")).toString()
                                              : dishName.isEmpty() ? QString()
                                              : MenuBoard::photoNames().contains(dish.value(QStringLiteral("food")).toString())
                                                  ? QStringLiteral("qrc:/food/") + dish.value(QStringLiteral("food")).toString() + QStringLiteral(".jpg")
                                              : QStringLiteral("qrc:/food/") + MenuBoard::photoFor(dishName, dish.value(QStringLiteral("note")).toString()) + QStringLiteral(".jpg"));

    // Verjaardagen en bijzondere dagen
    const QString md = today.toString(QStringLiteral("MM-dd"));
    const QString ymd = today.toString(Qt::ISODate);
    m_celebrations.clear();
    for (const QJsonValue &v : m_data.value(QStringLiteral("birthdays")).toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("date")).toString() == md) {
            m_celebrations.append(QVariantMap{{QStringLiteral("title"), QStringLiteral("Gefeliciteerd %1!").arg(o.value(QStringLiteral("name")).toString())},
                                              {QStringLiteral("icon"), QStringLiteral("🎂")}, {QStringLiteral("kind"), QStringLiteral("birthday")}});
        }
    }
    // Nederlandse feestdagen
    if (m_data.value(QStringLiteral("settings")).toObject().value(QStringLiteral("holidays")).toBool(true)) {
        const auto h = holidayFor(today);
        if (!h.first.isEmpty()) {
            m_celebrations.append(QVariantMap{{QStringLiteral("title"), h.first}, {QStringLiteral("icon"), h.second},
                                              {QStringLiteral("kind"), QStringLiteral("holiday")}});
        }
    }
    for (const QJsonValue &v : m_data.value(QStringLiteral("special")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString d = o.value(QStringLiteral("date")).toString();
        if (d == md || d == ymd) {
            const QString icon = o.value(QStringLiteral("icon")).toString();
            m_celebrations.append(QVariantMap{{QStringLiteral("title"), o.value(QStringLiteral("title")).toString()},
                                              {QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("🎉") : icon}});
        }
    }

    // Bezig: begonnen en (eind nog niet bereikt, of zonder eind: tot de volgende activiteit)
    m_current = -1;
    m_next = -1;
    for (int i = 0; i < list.size(); ++i) {
        const QString start = list[i].value(QStringLiteral("start")).toString();
        QString end = list[i].value(QStringLiteral("end")).toString();
        if (end.isEmpty()) {
            end = i + 1 < list.size() ? list[i + 1].value(QStringLiteral("start")).toString() : QStringLiteral("23:59");
        }
        if (start <= nowTime && nowTime < end) {
            m_current = i;
        }
        if (m_next < 0 && start > nowTime) {
            m_next = i;
        }
    }

    m_today.clear();
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap m = list[i].toVariantMap();
        const QString end = m.value(QStringLiteral("end")).toString();
        const bool past = i != m_current && ((!end.isEmpty() && end <= nowTime) || (end.isEmpty() && i < m_next) || (m_next < 0 && i != m_current));
        m.insert(QStringLiteral("state"), i == m_current ? QStringLiteral("now")
                                          : i == m_next  ? QStringLiteral("next")
                                          : past         ? QStringLiteral("past")
                                                         : QStringLiteral("later"));
        m_today.append(m);
    }

    // Nieuwe activiteit begonnen: melden (voorlezen), niet bij het opstarten
    const QString currentId = m_current >= 0 ? list[m_current].value(QStringLiteral("id")).toString() : QString();
    if (currentId != m_currentId) {
        m_currentId = currentId;
        if (!m_first && m_current >= 0) {
            Q_EMIT activityStarted(list[m_current].value(QStringLiteral("title")).toString(), list[m_current].value(QStringLiteral("note")).toString());
        }
    }
    m_first = false;
    Q_EMIT changed();
}

QVariant Agenda::uiValue(const QString &key, const QVariant &fallback) const
{
    return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("ui/") + key, fallback);
}

void Agenda::setUiValue(const QString &key, const QVariant &value)
{
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("ui/") + key, value);
}
