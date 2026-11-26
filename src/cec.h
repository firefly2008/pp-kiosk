// Bediening met de gewone tv-afstandsbediening via HDMI-CEC.
// Nodig: een USB-CEC-adapter van Pulse-Eight en libCEC (Kijkluister kan die ophalen en installeren).
// Kijkluister start cec-client.exe en vertaalt de knoppen naar dezelfde acties als een controller.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Cec : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit Cec(QObject *parent = nullptr);
    ~Cec() override;

    bool installed() const { return !clientPath().isEmpty(); }
    bool connected() const { return m_connected; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void install();      // libCEC van GitHub (Pulse-Eight/libcec) ophalen en installeren
    // Eén regel van cec-client verwerken (ook voor de rooktest)
    Q_INVOKABLE bool parseLine(const QString &line);

    static int sdlButtonFor(int cecCode);

Q_SIGNALS:
    void changed();
    void button(int sdlButton, bool pressed);
    void homeRequested();

private:
    QString clientPath() const;
    void setStatus(const QString &status, bool busy = false);

    QProcess *m_process = nullptr;
    QNetworkAccessManager m_network;
    QTimer m_retry;
    QByteArray m_buffer;
    bool m_connected = false;
    bool m_busy = false;
    QString m_status;
    int m_down = -1;
};
