// SPDX-License-Identifier: GPL-2.0-or-later
#include "agenda.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
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
    return QJsonDocument(m_data).toJson(QJsonDocument::Compact);
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
    // Scherminstellingen
    const QJsonObject si = in.value(QStringLiteral("settings")).toObject();
    const QString nightFrom = si.value(QStringLiteral("nightFrom")).toString();
    const QString nightTo = si.value(QStringLiteral("nightTo")).toString();
    const double scale = si.value(QStringLiteral("textScale")).toDouble(1.0);
    const QString nightMode = si.value(QStringLiteral("nightMode")).toString();
    const QString dinnerTime = si.value(QStringLiteral("dinnerTime")).toString();
    const QJsonObject settings{
        {QStringLiteral("name"), si.value(QStringLiteral("name")).toString().trimmed().left(40)},
        {QStringLiteral("speak"), si.value(QStringLiteral("speak")).toBool(false)},
        {QStringLiteral("chime"), si.value(QStringLiteral("chime")).toBool(true)},
        {QStringLiteral("textScale"), qBound(1.0, scale, 1.5)},
        {QStringLiteral("contrast"), si.value(QStringLiteral("contrast")).toBool(false)},
        {QStringLiteral("screensaverMinutes"), qBound(0, si.value(QStringLiteral("screensaverMinutes")).toInt(0), 240)},
        {QStringLiteral("screensaverPresentation"), si.value(QStringLiteral("screensaverPresentation")).toString().left(60)},
        {QStringLiteral("nightFrom"), validTime(nightFrom) ? nightFrom : QString()},
        {QStringLiteral("nightTo"), validTime(nightTo) ? nightTo : QString()},
        {QStringLiteral("nightMode"), nightMode == QLatin1String("black") ? nightMode : QStringLiteral("dim")},
        {QStringLiteral("dinnerTime"), validTime(dinnerTime) ? dinnerTime : QStringLiteral("17:00")},
    };
    m_data = QJsonObject{{QStringLiteral("version"), 3}, {QStringLiteral("items"), items}, {QStringLiteral("messages"), messages},
                         {QStringLiteral("menu"), menu}, {QStringLiteral("birthdays"), birthdays}, {QStringLiteral("special"), special},
                         {QStringLiteral("settings"), settings}};
    save();
    m_stamp.clear();
    recompute();
    qInfo() << "Agenda: saved" << items.size() << "items";
    return true;
}

QStringList Agenda::messages() const
{
    QStringList out;
    for (const QJsonValue &v : m_data.value(QStringLiteral("messages")).toArray()) {
        out.append(v.toString());
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
    if (!m.contains(QStringLiteral("dinnerTime"))) {
        m.insert(QStringLiteral("dinnerTime"), QStringLiteral("17:00"));
    }
    if (!m.contains(QStringLiteral("screensaverMinutes"))) {
        m.insert(QStringLiteral("screensaverMinutes"), 5);
        m.insert(QStringLiteral("screensaverPresentation"), QStringLiteral("Menu van de week"));
    }
    return m;
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

    // Verjaardagen en bijzondere dagen
    const QString md = today.toString(QStringLiteral("MM-dd"));
    const QString ymd = today.toString(Qt::ISODate);
    m_celebrations.clear();
    for (const QJsonValue &v : m_data.value(QStringLiteral("birthdays")).toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("date")).toString() == md) {
            m_celebrations.append(QVariantMap{{QStringLiteral("title"), QStringLiteral("Gefeliciteerd %1!").arg(o.value(QStringLiteral("name")).toString())},
                                              {QStringLiteral("icon"), QStringLiteral("🎂")}});
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
