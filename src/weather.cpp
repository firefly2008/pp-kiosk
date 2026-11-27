// SPDX-License-Identifier: GPL-2.0-or-later
#include "weather.h"

#include <QDate>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QUrlQuery>

Weather::Weather(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(30 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &Weather::refresh);
    m_timer.start();
}

void Weather::setEnabled(bool on)
{
    m_settings.setValue(QStringLiteral("weather/enabled"), on);
    Q_EMIT changed();
    if (on) {
        refresh();
    }
}

QString Weather::summary() const
{
    if (!m_available) {
        return {};
    }
    return QStringLiteral("%1°  %2").arg(m_temperature).arg(m_text);
}

void Weather::setOffline(bool on)
{
    if (m_offline == on) {
        return;
    }
    m_offline = on;
    qInfo() << "Weather: offline" << on;
    Q_EMIT changed();
    if (!on) {
        refresh();
    }
}

// Laatste weerbericht van schijf (offline of zonder internet); alleen als het van vandaag is
void Weather::loadCache()
{
    if (m_available) {
        return;
    }
    QFile f(appDataDir() + QStringLiteral("/weather-cache.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QString first = o.value(QStringLiteral("daily")).toObject().value(QStringLiteral("time")).toArray().at(0).toString();
    if (first == QDate::currentDate().toString(Qt::ISODate) && apply(o)) {
        qInfo() << "Weather: from cache";
    }
}

void Weather::refresh()
{
    if (!enabled()) {
        return;
    }
    if (m_offline) {
        loadCache();
        return;
    }
    if (m_settings.contains(QStringLiteral("weather/lat"))) {
        fetch();
    } else {
        locate();
    }
}

// Ongeveer waar de computer staat (plaats van het internetadres); alleen bij de eerste keer
void Weather::locate()
{
    // Niet eindeloos wachten als de dienst niet antwoordt (dan later opnieuw)
    QNetworkRequest request(QUrl(QStringLiteral("https://get.geojs.io/v1/ip/geo.json")));
    request.setTransferTimeout(8000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const double lat = o.value(QStringLiteral("latitude")).toString().toDouble();
        const double lon = o.value(QStringLiteral("longitude")).toString().toDouble();
        if (reply->error() != QNetworkReply::NoError || (lat == 0 && lon == 0)) {
            qInfo() << "Weather: location unknown" << reply->errorString();
            return;
        }
        m_settings.setValue(QStringLiteral("weather/lat"), lat);
        m_settings.setValue(QStringLiteral("weather/lon"), lon);
        m_settings.setValue(QStringLiteral("weather/place"), o.value(QStringLiteral("city")).toString());
        fetch();
    });
}

void Weather::fetch()
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(m_settings.value(QStringLiteral("weather/lat")).toDouble(), 'f', 3));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(m_settings.value(QStringLiteral("weather/lon")).toDouble(), 'f', 3));
    q.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code,is_day"));
    q.addQueryItem(QStringLiteral("daily"), QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    q.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("3"));
    url.setQuery(q);
    QNetworkRequest request(url);
    request.setTransferTimeout(8000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QByteArray raw = reply->readAll();
        const QJsonObject o = QJsonDocument::fromJson(raw).object();
        if (reply->error() != QNetworkReply::NoError || !apply(o)) {
            qInfo() << "Weather: failed" << reply->errorString();
            loadCache();
            return;
        }
        QSaveFile f(appDataDir() + QStringLiteral("/weather-cache.json"));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(raw);
            f.commit();
        }
    });
}

bool Weather::apply(const QJsonObject &o)
{
    const QJsonObject current = o.value(QStringLiteral("current")).toObject();
    if (current.isEmpty()) {
        return false;
    }
    const int code = current.value(QStringLiteral("weather_code")).toInt();
    m_temperature = qRound(current.value(QStringLiteral("temperature_2m")).toDouble());
    m_text = describe(code);
    m_icon = iconFor(code, current.value(QStringLiteral("is_day")).toInt(1) == 1);
    m_days.clear();
    const QJsonObject daily = o.value(QStringLiteral("daily")).toObject();
    const QJsonArray dates = daily.value(QStringLiteral("time")).toArray();
    for (int i = 0; i < dates.size(); ++i) {
        const QDate date = QDate::fromString(dates.at(i).toString(), Qt::ISODate);
        const int dcode = daily.value(QStringLiteral("weather_code")).toArray().at(i).toInt();
        m_days.append(QVariantMap{
            {QStringLiteral("day"), i == 0 ? tr("Vandaag") : i == 1 ? tr("Morgen") : QLocale().toString(date, QStringLiteral("dddd"))},
            {QStringLiteral("icon"), iconFor(dcode, true)},
            {QStringLiteral("text"), describe(dcode)},
            {QStringLiteral("max"), qRound(daily.value(QStringLiteral("temperature_2m_max")).toArray().at(i).toDouble())},
            {QStringLiteral("min"), qRound(daily.value(QStringLiteral("temperature_2m_min")).toArray().at(i).toDouble())},
            {QStringLiteral("rain"), daily.value(QStringLiteral("precipitation_probability_max")).toArray().at(i).toInt()},
        });
    }
    m_available = true;
    qInfo().noquote() << "Weather:" << place() << summary();
    Q_EMIT changed();
    return true;
}

void Weather::searchPlaces(const QString &query)
{
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("name"), query.trimmed());
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("8"));
    q.addQueryItem(QStringLiteral("language"), QStringLiteral("nl"));
    url.setQuery(q);
    QNetworkRequest request(url);
    request.setTransferTimeout(8000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_places.clear();
        const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("results")).toArray();
        for (const QJsonValue &v : results) {
            const QJsonObject r = v.toObject();
            QStringList where{r.value(QStringLiteral("admin1")).toString(), r.value(QStringLiteral("country")).toString()};
            where.removeAll(QString());
            m_places.append(QVariantMap{
                {QStringLiteral("name"), r.value(QStringLiteral("name")).toString()},
                {QStringLiteral("detail"), where.join(QStringLiteral(", "))},
                {QStringLiteral("lat"), r.value(QStringLiteral("latitude")).toDouble()},
                {QStringLiteral("lon"), r.value(QStringLiteral("longitude")).toDouble()},
            });
        }
        Q_EMIT placesChanged();
    });
}

void Weather::choosePlace(int index)
{
    if (index < 0 || index >= m_places.size()) {
        return;
    }
    const QVariantMap p = m_places.at(index).toMap();
    m_settings.setValue(QStringLiteral("weather/lat"), p.value(QStringLiteral("lat")));
    m_settings.setValue(QStringLiteral("weather/lon"), p.value(QStringLiteral("lon")));
    m_settings.setValue(QStringLiteral("weather/place"), p.value(QStringLiteral("name")));
    m_settings.setValue(QStringLiteral("weather/enabled"), true);
    Q_EMIT changed();
    fetch();
}

QString Weather::describe(int code)
{
    if (code == 0) return tr("helder");
    if (code == 1) return tr("vrijwel helder");
    if (code == 2) return tr("half bewolkt");
    if (code == 3) return tr("bewolkt");
    if (code == 45 || code == 48) return tr("mist");
    if (code >= 51 && code <= 57) return tr("motregen");
    if (code >= 61 && code <= 67) return tr("regen");
    if (code >= 71 && code <= 77) return tr("sneeuw");
    if (code >= 80 && code <= 82) return tr("buien");
    if (code == 85 || code == 86) return tr("sneeuwbuien");
    if (code >= 95) return tr("onweer");
    return {};
}

QString Weather::iconFor(int code, bool day)
{
    if (code == 0) return day ? QStringLiteral("☀️") : QStringLiteral("🌙");
    if (code == 1 || code == 2) return day ? QStringLiteral("⛅") : QStringLiteral("☁️");
    if (code == 3) return QStringLiteral("☁️");
    if (code == 45 || code == 48) return QStringLiteral("🌫️");
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return QStringLiteral("🌧️");
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return QStringLiteral("❄️");
    if (code >= 95) return QStringLiteral("⛈️");
    return QStringLiteral("🌡️");
}
