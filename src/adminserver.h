// Beheerpagina: kleine webserver op deze pc en het (thuis)netwerk.
// Op de kiosk-pc zelf: http://localhost:8642 (geen code nodig).
// Vanaf een andere pc, tablet of telefoon: http://<pc>:8642 + de code die PP Kiosk toont.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

class Agenda;
class QTcpSocket;

class AdminServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)     // http://192.168.1.20:8642
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QUrl qrImage READ qrImage NOTIFY changed)

public:
    AdminServer(const QString &dataDir, Agenda *agenda, QObject *parent = nullptr);

    bool listening() const { return m_server.isListening(); }
    QString address() const;
    QString code() const { return m_code; }
    QUrl qrImage() const { return m_qr; }

    Q_INVOKABLE void newCode();
    // Rooktest: een aanvraag naar de eigen server doen en het resultaat loggen
    Q_INVOKABLE void selfTest();

Q_SIGNALS:
    void changed();

private:
    void start();
    void handle(QTcpSocket *socket);
    void respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body);
    void makeQr();

    QString m_dataDir;
    Agenda *m_agenda;
    QTcpServer m_server;
    QString m_code;
    QUrl m_qr;
    QHash<QString, QList<qint64>> m_failures;   // ip → tijden van foute codes
    QHash<QString, qint64> m_blockedUntil;      // ip → geblokkeerd tot
    int m_open = 0;
};
