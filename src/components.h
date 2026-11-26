// Onderdelen die Kijkluister zelf ophaalt van GitHub, los van de installer:
//   - MPC-HC (clsid2/mpc-hc)          -> %LOCALAPPDATA%\Kijkluister\mpc-hc
//   - ffmpeg (BtbN/FFmpeg-Builds, GPL) -> %LOCALAPPDATA%\Kijkluister\ffmpeg
// Op de achtergrond na het opstarten, of meteen als een film/opname erom vraagt.
// Eens per week wordt gekeken of er een nieuwe versie is.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <functional>

class QNetworkReply;

class Components : public QObject
{
    Q_OBJECT
    // Voor de melding onderin: "MPC-HC wordt opgehaald… 42%"
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)       // 0..100, -1 = onbekend
    Q_PROPERTY(QVariantMap installed READ installed NOTIFY changed)

public:
    enum Kind { MpcHc, Ffmpeg };
    Q_ENUM(Kind)

    explicit Components(const QString &dataDir, QObject *parent = nullptr);
    static Components *instance();
    // Gedeelde hulpjes voor alles wat ffmpeg start (opnemen, doorgifte): pad naar ffmpeg (leeg = niet aanwezig)
    // en een vrije UDP-poort op 127.0.0.1
    static QString ffmpegPath();
    static quint16 freeUdpPort();

    QString dir(Kind kind) const;
    QString executable(Kind kind) const;      // leeg als nog niet aanwezig
    bool busy() const { return !m_queue.isEmpty() || m_current >= 0; }
    QString status() const { return m_status; }
    int progress() const { return m_progress; }
    QVariantMap installed() const;

    // Zorg dat het onderdeel er is; done(true) als het klaarstaat (meteen als het er al is)
    void ensure(Kind kind, std::function<void(bool)> done = {});
    // Achtergrond: ontbrekende onderdelen ophalen, en eens per week op updates controleren
    Q_INVOKABLE void startBackground();

Q_SIGNALS:
    void changed();
    void ready(Kind kind, bool ok);

private:
    struct Job {
        Kind kind;
        bool updateCheckOnly = false;
    };
    void enqueue(Kind kind, bool updateCheckOnly);
    void next();
    void finish(bool ok);
    void fetchRelease();
    void download(const QString &url, const QString &tag);
    void extract(const QString &archive, const QString &tag);
    QString name(Kind kind) const;
    void setStatus(const QString &status, int progress);
    QNetworkReply *get(const QUrl &url);

    QString m_dataDir;
    QSettings m_settings;
    QNetworkAccessManager m_network;
    QList<Job> m_queue;
    int m_current = -1;           // Kind van de lopende taak
    QString m_expectedDigest;     // sha256 van het pakket dat wordt opgehaald
    bool m_currentUpdateOnly = false;
    QHash<int, QList<std::function<void(bool)>>> m_waiting;
    QString m_status;
    int m_progress = -1;
};
