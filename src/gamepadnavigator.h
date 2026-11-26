// Vertaalt gamepad-invoer (SDL3) naar navigatietoetsen voor de shell-UI.
// Windows-tegenhanger van inputhandler/sdlcontroller.cpp: in plaats van via het
// XDG-portal toetsen in de compositor te injecteren, krijgt ons eigen venster ze direct.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <SDL3/SDL.h>

class GamepadNavigator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY connectedCountChanged)

public:
    explicit GamepadNavigator(QObject *parent = nullptr);
    ~GamepadNavigator() override;

    int connectedCount() const { return m_pads.size(); }
    // Ook gebruikt door de tv-afstandsbediening (HDMI-CEC): knop als SDL-gamepadknop
    void handleButton(int button, bool pressed);

Q_SIGNALS:
    void connectedCountChanged();
    void homeRequested();
    void guidePressed();   // Home-knop: via HomeBridge (sluit eerst de app)

private:
    void poll();
    void sendKey(int qtKey, bool pressed);
    void tapKey(int qtKey);
    void handleStick();

    QTimer m_timer;
    QTimer m_holdTimer;            // A ingedrukt houden = opties (zoals op een telefoon)
    bool m_holdFired = false;
    QList<SDL_Gamepad *> m_pads;
    int m_stickKey = 0;
    bool m_stickRepeating = false;
    QElapsedTimer m_stickTimer;
};
