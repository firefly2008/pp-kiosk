// Kijkluister werkt zichzelf bij: kijkt wekelijks bij de GitHub-releases of er een nieuwere
// versie is, haalt op verzoek de installer op en start die stil (daarna start Kijkluister opnieuw).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include "paths.h"
#include <QSettings>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

class Updater : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString current READ current CONSTANT)
    Q_PROPERTY(QString latest READ latest NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    // Stil bijwerken via de geplande taak: "", "downloading", "ready", "installing"
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(bool silent READ silent NOTIFY changed)

public:
    explicit Updater(QObject *parent = nullptr);

    QString current() const;
    QString latest() const { return m_latest; }
    bool updateAvailable() const;
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }
    QString notes() const { return m_notes; }
    QString phase() const { return m_phase; }
    bool silent() const { return m_silent; }
    // Start de regelmatige controle (elke 6 uur, de eerste keer na een minuut)
    Q_INVOKABLE void startAutomatic();
    // Er speelt niets: geplande taak mag installeren (Kijkluister sluit en start daarna opnieuw)
    Q_INVOKABLE void installNow();

    // Automatisch: hooguit eens per week. manual = altijd, met melding als er niets nieuws is.
    Q_INVOKABLE void check(bool manual = false);
    Q_INVOKABLE void install();
    Q_INVOKABLE QString releaseNotesJson() const;   // releasenotes.json uit de bronnen van het programma
    Q_INVOKABLE bool shouldOffer() const;   // nieuw én nog niet weggeklikt
    Q_INVOKABLE void dismiss();

    static int compare(const QString &a, const QString &b);

Q_SIGNALS:
    void changed();
    void checked(bool manual);
    void readyToInstall();

private:
    void setStatus(const QString &status, bool busy);
    void startSilentDownload();
    void readTaskStatus();
    void setPhase(const QString &phase);
    QString repo() const;

    QSettings m_settings{appSettingsFile(), QSettings::IniFormat};
    QNetworkAccessManager m_network;
    QString m_latest;
    QUrl m_asset;
    QString m_digest;   // "sha256:…" van GitHub
    QString m_notes;
    bool m_busy = false;
    double m_progress = 0;
    QString m_status;
    QString m_phase;
    bool m_silent = false;
    QTimer *m_checkTimer = nullptr;
    QTimer *m_statusTimer = nullptr;
};
