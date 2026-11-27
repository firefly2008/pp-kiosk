// SPDX-License-Identifier: GPL-2.0-or-later
#include "calendarfeed.h"
#include "agenda.h"
#include "paths.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSettings>
#include <QTimeZone>

namespace {
QString cacheFile() { return appDataDir() + QStringLiteral("/agenda-ical.ics"); }

// Tekst uit iCal: \n, \, en \; terugzetten
QString unescape(QString v)
{
    v.replace(QStringLiteral("\\n"), QStringLiteral(" ")).replace(QStringLiteral("\\N"), QStringLiteral(" "));
    v.replace(QStringLiteral("\\,"), QStringLiteral(",")).replace(QStringLiteral("\;"), QStringLiteral(";")).replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return v.simplified();
}

// DTSTART;TZID=Europe/Amsterdam:20261127T093000  /  DTSTART:20261127T083000Z  /  DTSTART;VALUE=DATE:20261127
QDateTime parseTime(const QString &params, const QString &value, bool *allDay)
{
    *allDay = params.contains(QLatin1String("VALUE=DATE")) && !params.contains(QLatin1String("DATE-TIME"));
    if (*allDay || value.size() == 8) {
        *allDay = true;
        return QDateTime(QDate::fromString(value.left(8), QStringLiteral("yyyyMMdd")), QTime(0, 0));
    }
    QDateTime dt = QDateTime::fromString(value.left(15), QStringLiteral("yyyyMMdd'T'HHmmss"));
    if (value.endsWith(QLatin1Char('Z'))) {
        dt.setTimeZone(QTimeZone::utc());
        return dt.toLocalTime();
    }
    const int tz = params.indexOf(QLatin1String("TZID="));
    if (tz >= 0) {
        const QByteArray id = params.mid(tz + 5).section(QLatin1Char(';'), 0, 0).remove(QLatin1Char('"')).toUtf8();
        const QTimeZone zone(id);
        if (zone.isValid()) {
            dt.setTimeZone(zone);
            return dt.toLocalTime();
        }
    }
    return dt;   // zwevende tijd: gewoon de tijd hier
}
}

CalendarFeed::CalendarFeed(Agenda *agenda, QObject *parent)
    : QObject(parent)
    , m_agenda(agenda)
{
    m_timer.setInterval(15 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &CalendarFeed::refresh);
    m_timer.start();
    QFile f(cacheFile());
    if (!url().isEmpty() && f.open(QIODevice::ReadOnly)) {
        apply(f.readAll());
    }
    QTimer::singleShot(5000, this, &CalendarFeed::refresh);
}

QString CalendarFeed::url() const
{
    return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("calendar/url")).toString();
}

void CalendarFeed::setUrl(const QString &url)
{
    QString u = url.trimmed();
    if (u.startsWith(QLatin1String("webcal://"))) {
        u = QStringLiteral("https://") + u.mid(9);
    }
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("calendar/url"), u);
    if (u.isEmpty()) {
        QFile::remove(cacheFile());
        m_count = 0;
        m_status = QStringLiteral("Uit");
        m_agenda->setExternal({});
        return;
    }
    refresh();
}

void CalendarFeed::refresh()
{
    const QString u = url();
    if (u.isEmpty()) {
        return;
    }
    // Zonder internet (of offline-modus): de bewaarde agenda opnieuw indelen (de dag kan veranderd zijn)
    auto fromCache = [this] {
        QFile f(cacheFile());
        if (f.open(QIODevice::ReadOnly)) {
            apply(f.readAll());
        }
    };
    if (m_agenda->settings().value(QStringLiteral("offline")).toBool()) {
        fromCache();
        return;
    }
    QNetworkRequest request{QUrl(u)};
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fromCache] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !data.contains("BEGIN:VCALENDAR")) {
            m_status = QStringLiteral("Ophalen lukt niet (%1)").arg(reply->error() == QNetworkReply::NoError ? QStringLiteral("geen agenda") : reply->errorString());
            qInfo().noquote() << "Calendar:" << m_status;
            fromCache();
            return;
        }
        QSaveFile f(cacheFile());
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.commit();
        }
        apply(data);
    });
}

void CalendarFeed::apply(const QByteArray &ics)
{
    const QList<QJsonObject> items = parse(ics, QDate::currentDate(), 8);
    m_count = items.size();
    m_status = QStringLiteral("Bijgewerkt om %1: %2 afspraken deze week").arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))).arg(m_count);
    qInfo() << "Calendar:" << m_count << "events";
    m_agenda->setExternal(items);
}

QList<QJsonObject> CalendarFeed::parse(const QByteArray &ics, const QDate &from, int days)
{
    // Regels die met een spatie beginnen horen bij de vorige regel
    QStringList lines;
    for (const QString &raw : QString::fromUtf8(ics).split(QLatin1Char('\n'))) {
        QString l = raw;
        if (l.endsWith(QLatin1Char('\r'))) l.chop(1);
        if ((l.startsWith(QLatin1Char(' ')) || l.startsWith(QLatin1Char('\t'))) && !lines.isEmpty()) {
            lines.last() += l.mid(1);
        } else {
            lines.append(l);
        }
    }
    const QDate to = from.addDays(days - 1);
    QList<QJsonObject> out;
    bool inEvent = false;
    QString summary, location, rrule, startParams, startValue, endParams, endValue, status;
    QList<QDate> exdates;
    static const QStringList dayCodes{QStringLiteral("MO"), QStringLiteral("TU"), QStringLiteral("WE"), QStringLiteral("TH"),
                                      QStringLiteral("FR"), QStringLiteral("SA"), QStringLiteral("SU")};
    for (const QString &l : std::as_const(lines)) {
        if (l == QLatin1String("BEGIN:VEVENT")) {
            inEvent = true;
            summary.clear(); location.clear(); rrule.clear(); startParams.clear(); startValue.clear();
            endParams.clear(); endValue.clear(); status.clear(); exdates.clear();
            continue;
        }
        if (!inEvent) continue;
        const int colon = l.indexOf(QLatin1Char(':'));
        if (l != QLatin1String("END:VEVENT")) {
            if (colon < 0) continue;
            const QString head = l.left(colon), value = l.mid(colon + 1);
            const QString name = head.section(QLatin1Char(';'), 0, 0).toUpper();
            const QString params = head.section(QLatin1Char(';'), 1);
            if (name == QLatin1String("SUMMARY")) summary = unescape(value);
            else if (name == QLatin1String("LOCATION")) location = unescape(value);
            else if (name == QLatin1String("RRULE")) rrule = value;
            else if (name == QLatin1String("STATUS")) status = value;
            else if (name == QLatin1String("DTSTART")) { startParams = params; startValue = value; }
            else if (name == QLatin1String("DTEND")) { endParams = params; endValue = value; }
            else if (name == QLatin1String("EXDATE")) {
                for (const QString &v : value.split(QLatin1Char(','))) {
                    bool ad = false;
                    exdates.append(parseTime(params, v, &ad).date());
                }
            }
            continue;
        }
        inEvent = false;
        if (startValue.isEmpty() || status == QLatin1String("CANCELLED")) continue;
        bool allDay = false, endAllDay = false;
        const QDateTime start = parseTime(startParams, startValue, &allDay);
        const QDateTime end = endValue.isEmpty() ? start : parseTime(endParams, endValue, &endAllDay);
        if (!start.isValid()) continue;

        // Welke dagen: eenmalig, of herhaald (dagelijks / wekelijks / maandelijks / jaarlijks, met INTERVAL, UNTIL, COUNT, BYDAY)
        QList<QDate> dates;
        if (rrule.isEmpty()) {
            dates.append(start.date());
        } else {
            QHash<QString, QString> r;
            for (const QString &part : rrule.split(QLatin1Char(';'))) r.insert(part.section(QLatin1Char('='), 0, 0).toUpper(), part.section(QLatin1Char('='), 1));
            const QString freq = r.value(QStringLiteral("FREQ"));
            const int interval = qMax(1, r.value(QStringLiteral("INTERVAL"), QStringLiteral("1")).toInt());
            int count = r.value(QStringLiteral("COUNT"), QStringLiteral("0")).toInt();
            QDate until;
            if (r.contains(QStringLiteral("UNTIL"))) { bool ad = false; until = parseTime(QString(), r.value(QStringLiteral("UNTIL")), &ad).date(); }
            QList<int> byDay;
            for (const QString &d : r.value(QStringLiteral("BYDAY")).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                const int idx = dayCodes.indexOf(d.right(2));
                if (idx >= 0) byDay.append(idx + 1);
            }
            if (freq == QLatin1String("WEEKLY") && byDay.isEmpty()) byDay.append(start.date().dayOfWeek());
            int made = 0;
            for (QDate d = start.date(); d <= to && (!until.isValid() || d <= until); d = d.addDays(1)) {
                bool hit = false;
                if (freq == QLatin1String("DAILY")) hit = start.date().daysTo(d) % interval == 0;
                else if (freq == QLatin1String("WEEKLY")) {
                    const QDate weekStart = start.date().addDays(1 - start.date().dayOfWeek());
                    hit = byDay.contains(d.dayOfWeek()) && (weekStart.daysTo(d) / 7) % interval == 0;
                } else if (freq == QLatin1String("MONTHLY")) {
                    hit = d.day() == start.date().day() && ((d.year() - start.date().year()) * 12 + d.month() - start.date().month()) % interval == 0;
                } else if (freq == QLatin1String("YEARLY")) {
                    hit = d.day() == start.date().day() && d.month() == start.date().month() && (d.year() - start.date().year()) % interval == 0;
                }
                if (!hit) continue;
                ++made;
                if (count > 0 && made > count) break;
                dates.append(d);
            }
        }
        for (const QDate &d : std::as_const(dates)) {
            if (d < from || d > to || exdates.contains(d)) continue;
            const qint64 lengthMin = start.secsTo(end) / 60;
            QJsonObject o{
                {QStringLiteral("date"), d.toString(Qt::ISODate)},
                {QStringLiteral("title"), summary.left(80)},
                {QStringLiteral("note"), location.left(120)},
                {QStringLiteral("icon"), QStringLiteral("📅")},
                {QStringLiteral("color"), QStringLiteral("#5DA9E9")},
                {QStringLiteral("external"), true},
                {QStringLiteral("allDay"), allDay},
                {QStringLiteral("start"), allDay ? QStringLiteral("00:00") : start.time().toString(QStringLiteral("HH:mm"))},
            };
            if (!allDay && lengthMin > 0 && lengthMin < 24 * 60) {
                o.insert(QStringLiteral("end"), start.time().addSecs(lengthMin * 60).toString(QStringLiteral("HH:mm")));
            }
            out.append(o);
        }
    }
    return out;
}

QString CalendarFeed::selfTest() const
{
    const QDate today = QDate::currentDate();
    const QString d = today.toString(QStringLiteral("yyyyMMdd"));
    const QString weekAgo = today.addDays(-7).toString(QStringLiteral("yyyyMMdd"));
    const QByteArray ics = QStringLiteral(
        "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"
        "BEGIN:VEVENT\r\nDTSTART;TZID=Europe/Amsterdam:%1T100000\r\nDTEND;TZID=Europe/Amsterdam:%1T110000\r\nSUMMARY:Bezoek van\r\n  de huisarts\r\nLOCATION:Kamer 2\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nDTSTART:%2T140000\r\nDTEND:%2T150000\r\nRRULE:FREQ=WEEKLY\r\nSUMMARY:Zwemmen\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:%1\r\nDTEND;VALUE=DATE:%1\r\nSUMMARY:Uitje\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nDTSTART:%1T160000\r\nSUMMARY:Afgezegd\r\nSTATUS:CANCELLED\r\nEND:VEVENT\r\n"
        "END:VCALENDAR\r\n").arg(d, weekAgo).toUtf8();
    const QList<QJsonObject> items = parse(ics, today, 1);
    QStringList parts;
    for (const QJsonObject &o : items) {
        parts.append(o.value(QStringLiteral("start")).toString() + QLatin1Char(' ') + o.value(QStringLiteral("title")).toString()
                     + (o.value(QStringLiteral("allDay")).toBool() ? QStringLiteral(" (hele dag)") : QString()));
    }
    return QString::number(items.size()) + QStringLiteral(" events: ") + parts.join(QStringLiteral(", "));
}
