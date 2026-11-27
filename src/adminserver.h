// Beheerpagina: kleine webserver op deze pc en het (thuis)netwerk.
// Op de kiosk-pc zelf: http://localhost:8642 (geen code nodig).
// Vanaf een andere pc, tablet of telefoon: http://<pc>:8642 + de code die PP Kiosk toont.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

class Agenda;
class AppModel;
class RadioModel;
class Presentations;
class Shortcuts;
class Stats;
class QTcpSocket;
class QTemporaryFile;

class QQuickWindow;

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

    // Keuze van diensten (tv/video, muziek, podcasts) en radiozenders op de beheerpagina
    void setMedia(const QList<AppModel *> &web, RadioModel *radio) { m_web = web; m_radio = radio; }
    // Live-beeld van het kioskscherm voor de beheerpagina
    void setWindow(QQuickWindow *window) { m_window = window; }
    // Gebruiksstatistiek (tabblad Gebruik)
    void setStats(Stats *stats) { m_stats = stats; }
    // Rooktest: back-up maken en meteen terugzetten (zonder herstart)
    Q_INVOKABLE void backupTest();
    // Rooktest: vaste inlog instellen, goed en fout proberen, weer weghalen
    Q_INVOKABLE void loginTest();

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
        QByteArray origin;      // Origin-kop (beheer van een andere kiosk uit)
    };
    void handle(QTcpSocket *socket);
    bool checkAccess(QTcpSocket *socket, const QByteArray &host, const QString &code, const QString &path, const QByteArray &origin);
    QByteArray mediaJson() const;
    bool setMediaJson(const QByteArray &json);
    void dispatch(QTcpSocket *socket, Pending &p);
    void respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body, const QByteArray &extraHeaders = {});
    void makeQr();
    // Vaste inlognaam + wachtwoord (naast de code). Wachtwoord alleen als PBKDF2-hash in settings.ini.
    bool loginSet() const;
    bool checkLogin(const QString &user, const QString &pass) const;
    void setLogin(const QString &user, const QString &pass);
    bool validToken(const QString &token);
    // Unsplash: zoeken (met eigen gratis sleutel) en een foto als achtergrond binnenhalen
    void unsplashSearch(QTcpSocket *socket, const QString &query, int page);
    void unsplashUse(QTcpSocket *socket, const QString &idOrLink);
    QString unsplashKey() const;
    void countFailure(const QString &ip);

    QString m_dataDir;
    Agenda *m_agenda;
    Presentations *m_presentations;
    Shortcuts *m_shortcuts;
    QHash<QTcpSocket *, Pending> m_pending;
    QList<AppModel *> m_web;
    RadioModel *m_radio = nullptr;
    Stats *m_stats = nullptr;
    QQuickWindow *m_window = nullptr;
    QTcpServer m_server;
    QString m_code;
    QUrl m_qr;
    QHash<QString, QList<qint64>> m_failures;   // ip → tijden van foute codes
    QHash<QString, qint64> m_blockedUntil;      // ip → geblokkeerd tot
    QNetworkAccessManager m_network;
    QHash<QString, qint64> m_tokens;            // sessie na inloggen met naam + wachtwoord → geldig tot
    int m_open = 0;
};
