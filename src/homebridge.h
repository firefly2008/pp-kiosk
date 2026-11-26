// Terug naar het startscherm vanuit elke app of website:
// - Home-toets van een (Bluetooth-)afstandsbediening of toetsenbord (VK_BROWSER_HOME), overal
// - Esc een seconde ingedrukt houden (kort Esc blijft van de app: bv. volledig scherm verlaten)
// - een Home-knop linksboven (ons eigen zwevende venster) zolang een app of website voorop staat,
//   in plaats van de grijze titelbalk van de browser
// Een browservenster of MPC-HC wordt daarbij gesloten; een ander programma geminimaliseerd.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QTimer>

class QWindow;

class HomeBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool external READ external NOTIFY changed)       // er staat een app/website voorop
    Q_PROPERTY(bool showButton READ showButton NOTIFY changed)   // Home-knop tonen

public:
    explicit HomeBridge(QWindow *main, QObject *parent = nullptr);
    ~HomeBridge() override;

    bool external() const { return m_external; }
    bool showButton() const { return m_showButton; }

    // Na het openen van een app/website/film aanroepen
    Q_INVOKABLE void externalLaunched();
    // Home-knop of Home-toets: app sluiten/minimaliseren en PP Kiosk naar voren
    Q_INVOKABLE void goHome();
    // Esc in een externe app: "hold" (1 s vasthouden), "double" (twee keer snel) of "direct"
    Q_INVOKABLE void setEscMode(const QString &mode);
    // PP Kiosk naar voren halen zonder iets te sluiten (bv. bij een herinnering)
    Q_INVOKABLE void raiseWindow();

    static HomeBridge *instance();
    // Alle browservensters die PP Kiosk opende (eigen profielmap) sluiten; geeft het aantal
    static int closeOwnBrowserWindows();
    // Spraak: Windows-dicteren (Win+H) openen; de tekst komt in het veld met de focus
    Q_INVOKABLE void startDictation();

    // Voor de toetsenbord-hook (Windows)
    void keyEvent(unsigned long vk, bool down, bool &swallow);

Q_SIGNALS:
    void changed();
    void homeRequested();

private:
    void poll();
    void setShowButton(bool show);

    QWindow *m_main;
    QTimer m_poll;
    QTimer m_escHold;
    QString m_escMode = QStringLiteral("hold");
    qint64 m_escLastMs = 0;
    void escPressed();
    QTimer m_backHold;
    bool m_backFired = false;
    bool m_external = false;
    bool m_showButton = false;
    bool m_escDown = false;
    qint64 m_launchedAt = 0;
    qint64 m_lastNearTop = 0;
};
