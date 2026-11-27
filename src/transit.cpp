// SPDX-License-Identifier: GPL-2.0-or-later
#include "transit.h"
#include "agenda.h"
#include "paths.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QUrlQuery>
#include <algorithm>

namespace {
QVariantMap settingsMap(Agenda *a) { return a->settings(); }
QString hhmm(const QDateTime &t) { return t.toString(QStringLiteral("HH:mm")); }
}

Transit::Transit(Agenda *agenda, QObject *parent)
    : QObject(parent)
    , m_agenda(agenda)
{
}

QString Transit::title() const
{
    const QVariantMap s = m_agenda->settings();
    QStringList parts;
    if (!s.value(QStringLiteral("ovStopName")).toString().isEmpty()) parts.append(s.value(QStringLiteral("ovStopName")).toString());
    if (!s.value(QStringLiteral("nsStationName")).toString().isEmpty()) parts.append(QStringLiteral("station ") + s.value(QStringLiteral("nsStationName")).toString());
    return parts.join(QStringLiteral(" en "));
}

QString Transit::nsKey() const { return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("ns/key")).toString(); }
void Transit::setNsKey(const QString &key) { QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("ns/key"), key.trimmed().left(80)); }

void Transit::refresh()
{
    const QVariantMap s = settingsMap(m_agenda);
    const QString stop = s.value(QStringLiteral("ovStop")).toString();
    const QString station = s.value(QStringLiteral("nsStation")).toString();
    m_bus.clear();
    m_train.clear();
    if (s.value(QStringLiteral("offline")).toBool()) {
        m_status = QStringLiteral("De kiosk staat op offline: geen vertrektijden.");
        m_departures.clear();
        Q_EMIT changed();
        return;
    }
    if (stop.isEmpty() && station.isEmpty()) {
        m_status = QStringLiteral("Nog geen halte of station gekozen (beheerpagina: Info > Onderweg).");
        m_departures.clear();
        Q_EMIT changed();
        return;
    }
    m_pending = 0;
    m_status.clear();
    if (!stop.isEmpty()) {
        ++m_pending;
        QNetworkRequest request(QUrl(QStringLiteral("http://v0.ovapi.nl/stopareacode/") + QString::fromLatin1(QUrl::toPercentEncoding(stop))));
        request.setTransferTimeout(10000);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PP Kiosk"));
        QNetworkReply *reply = m_network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            // { stopareacode: { tpc: { Stop: {...}, Passes: { id: {...} } } } }
            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QDateTime now = QDateTime::currentDateTime();
            for (const QJsonValue &area : root) {
                for (const QJsonValue &tpc : area.toObject()) {
                    for (const QJsonValue &pv : tpc.toObject().value(QStringLiteral("Passes")).toObject()) {
                        const QJsonObject p = pv.toObject();
                        const QDateTime planned = QDateTime::fromString(p.value(QStringLiteral("TargetDepartureTime")).toString(), Qt::ISODate);
                        const QDateTime expected = QDateTime::fromString(p.value(QStringLiteral("ExpectedDepartureTime")).toString(), Qt::ISODate);
                        const QDateTime t = expected.isValid() ? expected : planned;
                        if (!t.isValid() || t < now.addSecs(-60) || p.value(QStringLiteral("TripStopStatus")).toString() == QLatin1String("CANCEL")) continue;
                        const QString type = p.value(QStringLiteral("TransportType")).toString().toLower();
                        m_bus.append(QVariantMap{
                            {QStringLiteral("at"), t},
                            {QStringLiteral("time"), hhmm(planned.isValid() ? planned : t)},
                            {QStringLiteral("delay"), planned.isValid() && expected.isValid() ? int(planned.secsTo(expected) / 60) : 0},
                            {QStringLiteral("line"), p.value(QStringLiteral("LinePublicNumber")).toString()},
                            {QStringLiteral("to"), p.value(QStringLiteral("DestinationName50")).toString()},
                            {QStringLiteral("kind"), type == QLatin1String("tram") ? QStringLiteral("tram") : type == QLatin1String("metro") ? QStringLiteral("metro")
                                                     : type == QLatin1String("boat") ? QStringLiteral("boot") : QStringLiteral("bus")},
                        });
                    }
                }
            }
            if (reply->error() != QNetworkReply::NoError) {
                m_status = QStringLiteral("Bus/tram: niet bereikbaar. Is er internet?");
                qInfo() << "Transit: OVapi failed" << reply->errorString();
            }
            finish();
        });
    }
    const QString key = nsKey();
    if (!station.isEmpty() && !key.isEmpty()) {
        ++m_pending;
        QUrl url(QStringLiteral("https://gateway.apiportal.ns.nl/reisinformatie-api/api/v2/departures"));
        url.setQuery(QStringLiteral("station=") + QString::fromLatin1(QUrl::toPercentEncoding(station)) + QStringLiteral("&maxJourneys=12"));
        QNetworkRequest request(url);
        request.setTransferTimeout(10000);
        request.setRawHeader("Ocp-Apim-Subscription-Key", key.toUtf8());
        QNetworkReply *reply = m_network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("payload")).toObject().value(QStringLiteral("departures")).toArray();
            for (const QJsonValue &v : list) {
                const QJsonObject d = v.toObject();
                const QDateTime planned = QDateTime::fromString(d.value(QStringLiteral("plannedDateTime")).toString(), Qt::ISODateWithMs).toLocalTime();
                const QDateTime actual = QDateTime::fromString(d.value(QStringLiteral("actualDateTime")).toString(), Qt::ISODateWithMs).toLocalTime();
                const QDateTime t = actual.isValid() ? actual : planned;
                if (!t.isValid()) continue;
                m_train.append(QVariantMap{
                    {QStringLiteral("at"), t},
                    {QStringLiteral("time"), hhmm(planned.isValid() ? planned : t)},
                    {QStringLiteral("delay"), planned.isValid() && actual.isValid() ? int(planned.secsTo(actual) / 60) : 0},
                    {QStringLiteral("line"), d.value(QStringLiteral("trainCategory")).toString()},
                    {QStringLiteral("to"), d.value(QStringLiteral("direction")).toString()},
                    {QStringLiteral("track"), d.value(QStringLiteral("actualTrack")).toString(d.value(QStringLiteral("plannedTrack")).toString())},
                    {QStringLiteral("cancelled"), d.value(QStringLiteral("cancelled")).toBool()},
                    {QStringLiteral("kind"), QStringLiteral("trein")},
                });
            }
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError) {
                m_status = status == 401 ? QStringLiteral("Trein: de NS-sleutel klopt niet.") : QStringLiteral("Trein: niet bereikbaar. Is er internet?");
                qInfo() << "Transit: NS failed" << status << reply->errorString();
            }
            finish();
        });
    } else if (!station.isEmpty()) {
        m_status = QStringLiteral("Voor treinen is een NS-sleutel nodig (beheerpagina: Info > Onderweg).");
    }
    if (m_pending == 0) {
        finish();
    }
}

void Transit::finish()
{
    if (m_pending > 0 && --m_pending > 0) {
        return;
    }
    QVariantList all = m_bus + m_train;
    std::sort(all.begin(), all.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("at")).toDateTime() < b.toMap().value(QStringLiteral("at")).toDateTime();
    });
    m_departures = all.mid(0, 12);
    qInfo() << "Transit:" << m_departures.size() << "departures";
    Q_EMIT changed();
}

void Transit::loadSample()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_bus = {
        QVariantMap{{QStringLiteral("at"), now.addSecs(240)}, {QStringLiteral("time"), hhmm(now.addSecs(240))}, {QStringLiteral("delay"), 0}, {QStringLiteral("line"), QStringLiteral("12")}, {QStringLiteral("to"), QStringLiteral("Centraal Station")}, {QStringLiteral("kind"), QStringLiteral("bus")}},
        QVariantMap{{QStringLiteral("at"), now.addSecs(900)}, {QStringLiteral("time"), hhmm(now.addSecs(720))}, {QStringLiteral("delay"), 3}, {QStringLiteral("line"), QStringLiteral("4")}, {QStringLiteral("to"), QStringLiteral("Ziekenhuis")}, {QStringLiteral("kind"), QStringLiteral("tram")}},
    };
    m_train = {
        QVariantMap{{QStringLiteral("at"), now.addSecs(600)}, {QStringLiteral("time"), hhmm(now.addSecs(600))}, {QStringLiteral("delay"), 0}, {QStringLiteral("line"), QStringLiteral("IC")}, {QStringLiteral("to"), QStringLiteral("Amsterdam Centraal")}, {QStringLiteral("track"), QStringLiteral("5b")}, {QStringLiteral("kind"), QStringLiteral("trein")}},
    };
    m_pending = 1;
    finish();
}

void Transit::searchStops(const QString &query, const std::function<void(int, QByteArray)> &done)
{
    auto filter = [this, query, done] {
        QJsonArray out;
        const QStringList words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QJsonValue &v : std::as_const(m_stopIndex)) {
            const QJsonObject o = v.toObject();
            const QString hay = (o.value(QStringLiteral("town")).toString() + QLatin1Char(' ') + o.value(QStringLiteral("name")).toString()).toLower();
            if (std::all_of(words.begin(), words.end(), [&](const QString &w) { return hay.contains(w); })) {
                out.append(o);
                if (out.size() >= 25) break;
            }
        }
        done(200, QJsonDocument(QJsonObject{{QStringLiteral("results"), out}}).toJson(QJsonDocument::Compact));
    };
    if (!m_stopIndex.isEmpty()) {
        filter();
        return;
    }
    QNetworkRequest request(QUrl(QStringLiteral("http://v0.ovapi.nl/stopareacode")));
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PP Kiosk"));
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, filter, done] {
        reply->deleteLater();
        const QJsonObject all = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || all.isEmpty()) {
            done(502, QByteArrayLiteral(R"({"error":"De haltelijst ophalen lukt niet. Is er internet?"})"));
            return;
        }
        for (auto it = all.begin(); it != all.end(); ++it) {
            const QJsonObject o = it.value().toObject();
            m_stopIndex.append(QJsonObject{{QStringLiteral("code"), it.key()},
                                           {QStringLiteral("name"), o.value(QStringLiteral("TimingPointName")).toString()},
                                           {QStringLiteral("town"), o.value(QStringLiteral("TimingPointTown")).toString()}});
        }
        qInfo() << "Transit: stop index" << m_stopIndex.size();
        filter();
    });
}

void Transit::searchStations(const QString &query, const std::function<void(int, QByteArray)> &done)
{
    const QString key = nsKey();
    if (key.isEmpty()) {
        done(400, QByteArrayLiteral(R"({"error":"Vul eerst de NS-sleutel in."})"));
        return;
    }
    QUrl url(QStringLiteral("https://gateway.apiportal.ns.nl/reisinformatie-api/api/v2/stations"));
    url.setQuery(QStringLiteral("q=") + QString::fromLatin1(QUrl::toPercentEncoding(query)) + QStringLiteral("&countryCodes=nl&limit=15"));
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    request.setRawHeader("Ocp-Apim-Subscription-Key", key.toUtf8());
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("payload")).toArray();
        if (reply->error() != QNetworkReply::NoError) {
            done(502, status == 401 ? QByteArrayLiteral(R"({"error":"De NS-sleutel klopt niet."})") : QByteArrayLiteral(R"({"error":"NS is niet bereikbaar. Is er internet?"})"));
            return;
        }
        QJsonArray out;
        for (const QJsonValue &v : list) {
            const QJsonObject o = v.toObject();
            out.append(QJsonObject{{QStringLiteral("code"), o.value(QStringLiteral("code")).toString()},
                                   {QStringLiteral("name"), o.value(QStringLiteral("namen")).toObject().value(QStringLiteral("lang")).toString()}});
        }
        done(200, QJsonDocument(QJsonObject{{QStringLiteral("results"), out}}).toJson(QJsonDocument::Compact));
    });
}
