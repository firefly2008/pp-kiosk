// Telefoon als afstandsbediening (4.4): kleine webserver op het thuisnetwerk.
// De telefoon opent http://<pc>:8642/?c=<code> (QR-code in PP Kiosk) en krijgt een pagina met
// pijltjes, OK, Terug, Home, Menu, afspelen/pauze, volume en typen. Zonder de juiste code doet de pc niets.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QTcpServer>
#include <QUrl>

class QTcpSocket;

class RemoteServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)     // http://192.168.1.20:8642
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QUrl qrImage READ qrImage NOTIFY changed)        // PNG met de QR-code (adres + code)
    Q_PROPERTY(int clients READ clients NOTIFY clientsChanged)  // telefoons die de laatste 2 minuten iets stuurden

public:
    explicit RemoteServer(const QString &dataDir, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool listening() const { return m_server.isListening(); }
    QString address() const;
    QString code() const { return m_code; }
    QUrl qrImage() const { return m_qr; }
    int clients() const;

    Q_INVOKABLE void newCode();

    // Ook voor de rooktest: een opdracht uitvoeren zoals de telefoon hem stuurt
    Q_INVOKABLE bool command(const QString &key);
    Q_INVOKABLE void typeText(const QString &text);

Q_SIGNALS:
    void changed();
    void clientsChanged();
    void commandReceived(const QString &key);

private:
    void start();
    void handle(QTcpSocket *socket);
    void respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body);
    void makeQr();

    QString m_dataDir;
    QTcpServer m_server;
    bool m_enabled = false;
    QString m_code;
    QUrl m_qr;
    QHash<QString, qint64> m_seen;   // telefoon (ip) → laatst gezien
    QHash<QString, QList<qint64>> m_failures;   // ip → tijden van foute codes
    QHash<QString, qint64> m_blockedUntil;      // ip → geblokkeerd tot
    int m_open = 0;                              // open verbindingen
};
