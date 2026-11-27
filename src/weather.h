// Weer naast de klok (Open-Meteo, gratis en zonder sleutel).
// Plaats: zelf gekozen, anders bij de eerste start bepaald via het internetadres (geojs.io).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QObject>
#include "paths.h"
#include <QSettings>
#include <QTimer>
#include <QVariantList>

class Weather : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString place READ place NOTIFY changed)
    Q_PROPERTY(int temperature READ temperature NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QString icon READ icon NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)   // "8° bewolkt"
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)    // vandaag, morgen, overmorgen
    Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged)
    Q_PROPERTY(bool offline READ offline WRITE setOffline NOTIFY changed)

public:
    explicit Weather(QObject *parent = nullptr);

    bool available() const { return m_available && enabled(); }
    bool enabled() const { return m_settings.value(QStringLiteral("weather/enabled"), true).toBool(); }
    void setEnabled(bool on);
    QString place() const { return m_settings.value(QStringLiteral("weather/place")).toString(); }
    int temperature() const { return m_temperature; }
    QString text() const { return m_text; }
    QString icon() const { return m_icon; }
    QString summary() const;
    QVariantList days() const { return m_days; }
    QVariantList places() const { return m_places; }
    bool offline() const { return m_offline; }
    void setOffline(bool on);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void searchPlaces(const QString &query);
    Q_INVOKABLE void choosePlace(int index);

    static QString describe(int code);
    static QString iconFor(int code, bool day);

Q_SIGNALS:
    void changed();
    void placesChanged();

private:
    void locate();
    void fetch();
    bool apply(const QJsonObject &o);
    void loadCache();
    bool m_offline = false;

    QSettings m_settings{appSettingsFile(), QSettings::IniFormat};
    QNetworkAccessManager m_network;
    QTimer m_timer;
    bool m_available = false;
    int m_temperature = 0;
    QString m_text;
    QString m_icon;
    QVariantList m_days;
    QVariantList m_places;
};
