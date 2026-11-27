// Google Agenda (of een andere agenda met een iCal-link): afspraken van vandaag en de komende week
// komen vanzelf in de dagplanning. Elk kwartier opnieuw opgehaald; de laatste versie staat op schijf
// zodat het ook zonder internet werkt. Het adres staat alleen in settings.ini (het is geheim).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class Agenda;

class CalendarFeed : public QObject
{
    Q_OBJECT

public:
    explicit CalendarFeed(Agenda *agenda, QObject *parent = nullptr);

    QString url() const;
    void setUrl(const QString &url);
    QString status() const { return m_status; }
    int count() const { return m_count; }
    void refresh();
    // Rooktest: een voorbeeld-agenda inlezen en tellen
    Q_INVOKABLE QString selfTest() const;

    // iCal-tekst omzetten naar dagplanning-items van 'from' tot en met 'days' dagen later (ook voor de rooktest)
    static QList<QJsonObject> parse(const QByteArray &ics, const QDate &from, int days);

private:
    void apply(const QByteArray &ics);

    Agenda *m_agenda;
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QString m_status;
    int m_count = 0;
};
