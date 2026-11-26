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
class Presentations;
class Shortcuts;
class QTcpSocket;
class QTemporaryFile;

class AdminServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)     // http://192.168.1.20:8642
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QUrl qrImage READ qrImage NOTIFY changed)

public:
    AdminServer(const QString &dataDir, Agenda *agenda, Presentations *presentations, Shortcuts *shortcuts, QObject *parent = nullptr);

    bool listening() const { return m_server.isListening(); }
    QString address() const;
    QString code() const { return m_code; }
    QUrl qrImage() const { return m_qr; }

    Q_INVOKABLE void newCode();
    // Rooktest: een aanvraag naar de eigen server doen en het resultaat loggen
    Q_INVOKABLE void selfTest();
    // Rooktest: een PDF van 2 pagina's uploaden en wachten tot er dia's van gemaakt zijn
    Q_INVOKABLE void uploadTest();

Q_SIGNALS:
    void changed();

private:
    void start();
    // Een aanvraag in opbouw: kop eerst (dan controle), daarna de inhoud (uploads direct naar schijf)
    struct Pending {
        QByteArray buffer;
        bool headersDone = false;
        bool done = false;
        QByteArray method;
        QUrl url;
        qint64 length = 0;
        qint64 received = 0;
        QTemporaryFile *file = nullptr;
    };
    void handle(QTcpSocket *socket);
    bool checkAccess(QTcpSocket *socket, const QByteArray &host, const QString &code, const QString &path);
    void dispatch(QTcpSocket *socket, Pending &p);
    void respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body, const QByteArray &extraHeaders = {});
    void makeQr();

    QString m_dataDir;
    Agenda *m_agenda;
    Presentations *m_presentations;
    Shortcuts *m_shortcuts;
    QHash<QTcpSocket *, Pending> m_pending;
    QTcpServer m_server;
    QString m_code;
    QUrl m_qr;
    QHash<QString, QList<qint64>> m_failures;   // ip → tijden van foute codes
    QHash<QString, qint64> m_blockedUntil;      // ip → geblokkeerd tot
    int m_open = 0;
};
