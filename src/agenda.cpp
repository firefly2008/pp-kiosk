// SPDX-License-Identifier: GPL-2.0-or-later
#include "agenda.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

namespace {

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
{
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
        {QStringLiteral("version"), 1},
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
        {QStringLiteral("messages"), QJsonArray{QStringLiteral("Welkom! Pas deze dag aan via de beheerpagina.")}},
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
        QString color = o.value(QStringLiteral("color")).toString();
        if (!QRegularExpression(QStringLiteral("^#[0-9A-Fa-f]{6}$")).match(color).hasMatch()) {
            color = QStringLiteral("#A7D8F0");
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
        });
        if (items.size() > 100) {
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
    m_data = QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("items"), items}, {QStringLiteral("messages"), messages}};
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
    const int weekday = now.date().dayOfWeek();

    QList<QJsonObject> list;
    for (const QJsonValue &v : m_data.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("days")).toArray().contains(weekday)) {
            list.append(o);
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("start")).toString() < b.value(QStringLiteral("start")).toString();
    });

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
    Q_EMIT changed();
}
