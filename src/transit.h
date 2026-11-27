// Vertrektijden van bus, tram en metro (OVapi, gratis en zonder sleutel) en van de trein
// (NS-API, gratis sleutel nodig). Halte en station kies je in het beheer (Info > Onderweg).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QVariantList>

class Agenda;
class QTcpSocket;

class Transit : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList departures READ departures NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)

public:
    explicit Transit(Agenda *agenda, QObject *parent = nullptr);

    QVariantList departures() const { return m_departures; }
    QString status() const { return m_status; }
    QString title() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadSample();   // rooktest

    QString nsKey() const;
    void setNsKey(const QString &key);
    // Zoeken voor het beheer; het antwoord gaat via 'done' (JSON-tekst)
    void searchStops(const QString &query, const std::function<void(int, QByteArray)> &done);
    void searchStations(const QString &query, const std::function<void(int, QByteArray)> &done);

Q_SIGNALS:
    void changed();

private:
    void finish();

    Agenda *m_agenda;
    QNetworkAccessManager m_network;
    QVariantList m_bus;
    QVariantList m_train;
    QVariantList m_departures;
    QString m_status;
    int m_pending = 0;
    QJsonArray m_stopIndex;   // alle haltes van OVapi (eenmalig opgehaald voor het zoeken)
};
