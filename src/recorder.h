// Opnemen van live tv met ffmpeg, naar het voorbeeld van IPTV-Recorder (racacax):
// "ffmpeg -user_agent … -i url -c copy bestand.mp4", netjes stoppen met "q",
// en bij uitval van de stream automatisch opnieuw beginnen.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

class Recorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(QString channel READ channel NOTIFY stateChanged)
    Q_PROPERTY(QString file READ file NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY tick)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY tick)
    Q_PROPERTY(qint64 bytes READ bytes NOTIFY tick)
    Q_PROPERTY(int retries READ retries NOTIFY tick)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QVariantList recordings READ recordings NOTIFY recordingsChanged)
    // Geplande opnames uit de tv-gids
    Q_PROPERTY(QVariantList schedule READ schedule NOTIFY scheduleChanged)
    // De stream die wordt opgenomen, lokaal doorgegeven voor de speler (één verbinding met de aanbieder)
    Q_PROPERTY(QString relayUrl READ relayUrl NOTIFY stateChanged)
    Q_PROPERTY(QString url READ url NOTIFY stateChanged)
    // Opruimen: opnames ouder dan keepDays dagen (0 = altijd bewaren) en/of als er minder dan
    // minFreeGb vrij is op de schijf. Er wordt altijd eerst gevraagd (cleanupSuggested).
    Q_PROPERTY(int keepDays READ keepDays WRITE setKeepDays NOTIFY cleanupSettingsChanged)
    Q_PROPERTY(int minFreeGb READ minFreeGb WRITE setMinFreeGb NOTIFY cleanupSettingsChanged)
    Q_PROPERTY(QString diskText READ diskText NOTIFY recordingsChanged)

public:
    explicit Recorder(const QString &dataDir, QObject *parent = nullptr);
    ~Recorder() override;

    QString folder() const { return m_folder; }
    void setFolder(const QString &folder);
    bool recording() const { return m_active; }
    QString channel() const { return m_channel; }
    QString file() const { return m_file; }
    QString status() const { return m_status; }
    int elapsedSeconds() const;
    int remainingSeconds() const;
    qint64 bytes() const;
    int retries() const { return m_retries; }
    bool available() const;      // ffmpeg.exe gevonden
    QVariantList recordings() const;
    QString relayUrl() const { return m_active ? m_relayUrl : QString(); }
    QString url() const { return m_active ? m_url : QString(); }

    // Opname starten: duur in minuten (0 = tot je zelf stopt)
    Q_INVOKABLE bool start(const QString &channel, const QString &url, int minutes, const QString &title = {});
    QVariantList schedule() const { return m_schedule; }
    // Programma uit de gids inplannen (tijden in seconden sinds 1970). Begint 2 min eerder, stopt 5 min later.
    Q_INVOKABLE bool addToSchedule(const QString &channel, const QString &url, const QString &title, qint64 start, qint64 stop);
    Q_INVOKABLE void removeFromSchedule(const QString &id);
    Q_INVOKABLE bool isScheduled(const QString &url, qint64 start) const;
    Q_INVOKABLE void toggleSchedule(const QString &channel, const QString &url, const QString &title, qint64 start, qint64 stop);
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString localPath(const QUrl &url) const;
    Q_INVOKABLE QString defaultFolder() const;
    Q_INVOKABLE void openFolder();
    Q_INVOKABLE bool play(const QString &path);
    Q_INVOKABLE bool remove(const QString &path);
    Q_INVOKABLE void refresh();
    int keepDays() const { return m_settings.value(QStringLiteral("recorder/keepDays"), 0).toInt(); }
    void setKeepDays(int days);
    int minFreeGb() const { return m_settings.value(QStringLiteral("recorder/minFreeGb"), 10).toInt(); }
    void setMinFreeGb(int gb);
    QString diskText() const;
    // Welke opnames weg mogen volgens de regels (oudste eerst): [{path, name, reason, size}]
    Q_INVOKABLE QVariantList cleanupCandidates() const;
    Q_INVOKABLE int cleanup(const QStringList &paths);
    Q_INVOKABLE void checkCleanup(bool force = false);   // meldt cleanupSuggested als er iets op te ruimen is

Q_SIGNALS:
    void folderChanged();
    void stateChanged();
    void tick();
    void recordingsChanged();
    void finished(const QString &file, bool ok);
    void scheduleChanged();
    void cleanupSettingsChanged();
    void cleanupSuggested(int count, const QString &reason);

private:
    void launch();
    void onFinished(int code, QProcess::ExitStatus status);
    void setStatus(const QString &status);
    QString ffmpegPath() const;

    QSettings m_settings;
    QString m_folder;
    QProcess *m_process = nullptr;
    QTimer m_clock;
    QTimer m_retryTimer;
    QTimer m_endTimer;
    bool m_active = false;
    bool m_stopping = false;
    QString m_channel;
    QString m_url;
    QString m_file;
    QString m_status;
    QDateTime m_started;
    QDateTime m_endAt;
    int m_retries = 0;
    int m_part = 0;
    QStringList m_parts;
    QByteArray m_lastStderr;
    QString m_relayUrl;
    QString m_title;
    QString m_dataDir;
    QVariantList m_schedule;
    QString m_scheduledId;          // id van de geplande opname die nu loopt
    QTimer m_scheduleTimer;
    QTimer m_cleanupTimer;
    void checkSchedule();
    void saveSchedule();
    void keepAwake(bool on);
    static quint16 freeUdpPort();
};
