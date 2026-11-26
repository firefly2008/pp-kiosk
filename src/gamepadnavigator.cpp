// SPDX-License-Identifier: GPL-2.0-or-later
#include "gamepadnavigator.h"
#include "mpcplayer.h"
#include "homebridge.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>

GamepadNavigator::GamepadNavigator(QObject *parent)
    : QObject(parent)
{
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(700);
    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        m_holdFired = true;
        sendKey(Qt::Key_Menu, true);
        sendKey(Qt::Key_Menu, false);
    });
    // Geen SDL-venster: controller-events ook ontvangen zonder SDL-focus.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        qWarning() << "SDL_Init(SDL_INIT_GAMEPAD) failed:" << SDL_GetError();
        return;
    }
    connect(&m_timer, &QTimer::timeout, this, &GamepadNavigator::poll);
    m_timer.start(16);
}

GamepadNavigator::~GamepadNavigator()
{
    for (SDL_Gamepad *pad : std::as_const(m_pads)) {
        SDL_CloseGamepad(pad);
    }
    SDL_Quit();
}

void GamepadNavigator::poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (SDL_Gamepad *pad = SDL_OpenGamepad(event.gdevice.which)) {
                m_pads.append(pad);
                qInfo() << "Gamepad connected:" << SDL_GetGamepadName(pad);
                Q_EMIT connectedCountChanged();
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (SDL_Gamepad *pad = SDL_GetGamepadFromID(event.gdevice.which)) {
                m_pads.removeAll(pad);
                SDL_CloseGamepad(pad);
                qInfo() << "Gamepad disconnected";
                Q_EMIT connectedCountChanged();
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            handleButton(event.gbutton.button, true);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            handleButton(event.gbutton.button, false);
            break;
        default:
            break;
        }
    }
    handleStick();
}

void GamepadNavigator::handleButton(int button, bool pressed)
{
    // Speelt MPC-HC (film/serie), dan bedient de controller die
    if (MpcPlayer *mpc = MpcPlayer::instance(); mpc && mpc->handleButton(button, pressed)) {
        return;
    }
    // Website voorop (Netflix, nieuws...): controller en tv-afstandsbediening bedienen de browser
    if (HomeBridge *home = HomeBridge::instance(); home && home->handleBrowserButton(button, pressed)) {
        return;
    }
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        sendKey(Qt::Key_Up, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        sendKey(Qt::Key_Down, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        sendKey(Qt::Key_Left, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        sendKey(Qt::Key_Right, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_SOUTH: // A / kruisje: kort = openen, lang (0,7 s) = opties
        if (pressed) {
            m_holdFired = false;
            m_holdTimer.start();
        } else if (m_holdTimer.isActive() || !m_holdFired) {
            m_holdTimer.stop();
            sendKey(Qt::Key_Return, true);
            sendKey(Qt::Key_Return, false);
        }
        break;
    case SDL_GAMEPAD_BUTTON_EAST: // B / rondje
        sendKey(Qt::Key_Escape, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_WEST: // X / vierkantje: tegel aanpassen
    case SDL_GAMEPAD_BUTTON_START:
        sendKey(Qt::Key_Menu, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_NORTH: // Y / driehoekje: tv-gids
        sendKey(Qt::Key_I, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: // RB / R1: opnemen
        sendKey(Qt::Key_R, pressed);
        break;
    case SDL_GAMEPAD_BUTTON_GUIDE: // Home-knop: Bigscreen naar voren halen
        if (pressed) {
            Q_EMIT guidePressed();
        }
        break;
    default:
        break;
    }
}

void GamepadNavigator::handleStick()
{
    int key = 0;
    const int threshold = 20000; // van 32767
    for (SDL_Gamepad *pad : std::as_const(m_pads)) {
        const int x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const int y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (qAbs(x) > qAbs(y)) {
            key = x > threshold ? Qt::Key_Right : (x < -threshold ? Qt::Key_Left : 0);
        } else {
            key = y > threshold ? Qt::Key_Down : (y < -threshold ? Qt::Key_Up : 0);
        }
        if (key) {
            break;
        }
    }

    if (key != m_stickKey) {
        m_stickKey = key;
        m_stickRepeating = false;
        if (key) {
            tapKey(key);
            m_stickTimer.start();
        }
        return;
    }
    // Stick vastgehouden: herhalen, eerst na 400 ms, daarna elke 150 ms
    if (key && m_stickTimer.elapsed() > (m_stickRepeating ? 150 : 400)) {
        m_stickRepeating = true;
        tapKey(key);
        m_stickTimer.start();
    }
}

void GamepadNavigator::sendKey(int qtKey, bool pressed)
{
    // Alleen navigeren als Bigscreen zelf op de voorgrond staat; anders zou de
    // controller de launcher bedienen terwijl er een spel of app draait.
    QWindow *window = QGuiApplication::focusWindow();
    if (!window) {
        return;
    }
    QCoreApplication::postEvent(window,
                                new QKeyEvent(pressed ? QEvent::KeyPress : QEvent::KeyRelease, qtKey, Qt::NoModifier));
}

void GamepadNavigator::tapKey(int qtKey)
{
    sendKey(qtKey, true);
    sendKey(qtKey, false);
}
