// Nieuws op het startscherm: de laatste koppen van NOS (RSS), elke 10 minuten ververst.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class News : public QObject
{
    Q_OBJECT
    // Elk item: title, time ("19:30"), text (de tekst van het bericht, zonder HTML), image (foto-adres of leeg)
    Q_PROPERTY(QVariantList headlines READ headlines NOTIFY changed)
    // Offline-modus: niet naar internet, alleen de laatst bewaarde koppen
    Q_PROPERTY(bool offline READ offline WRITE setOffline NOTIFY changed)

public:
    explicit News(QObject *parent = nullptr);

    QVariantList headlines() const { return m_headlines; }
    bool offline() const { return m_offline; }
    void setOffline(bool on);
    // RSS-tekst omzetten naar koppen (ook voor de rooktest)
    static QVariantList parse(const QByteArray &xml, int max = 15);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadSample();   // rooktest zonder internet

Q_SIGNALS:
    void changed();

private:
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QVariantList m_headlines;
    bool m_offline = false;
    void saveCache() const;
    void loadCache();
};
