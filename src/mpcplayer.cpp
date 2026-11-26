// SPDX-License-Identifier: GPL-2.0-or-later
#include "mpcplayer.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <SDL3/SDL.h>

#include "appmodel.h" // mpcHcPath(): een eventueel zelf geïnstalleerde MPC-HC
#include "components.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
MpcPlayer *s_instance = nullptr;

}

MpcPlayer::MpcPlayer(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_portableDir(dataDir + QStringLiteral("/mpc-hc"))
{
    s_instance = this;
}

MpcPlayer::~MpcPlayer()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

MpcPlayer *MpcPlayer::instance()
{
    return s_instance;
}

bool MpcPlayer::bundled() const
{
    return Components::instance() && !Components::instance()->executable(Components::MpcHc).isEmpty();
}

bool MpcPlayer::running() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

// MPC-HC haalt Kijkluister zelf op (zie components.cpp) in %LOCALAPPDATA%\Kijkluister\mpc-hc.
// Daar staat ook mpc-hc64.ini: zolang die er is gebruikt MPC-HC geen register maar dit bestand.
void MpcPlayer::configure(const QString &dir)
{
    const QString ini = dir + QStringLiteral("/mpc-hc64.ini");
    if (QFileInfo::exists(ini)) {
        return; // eigen instellingen van de gebruiker niet overschrijven
    }
    // Nederlands, schermvullend, onthouden waar je was, geen updatevraag (Kijkluister houdt MPC-HC bij)
    // (namen uit MPC-HC SettingsDefines.h; alles in de sectie [Settings])
    QFile f(ini);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write("[Settings]\r\n"
                "InterfaceLanguage=1043\r\n"
                "LaunchFullScreen=1\r\n"
                "RememberFilePos=1\r\n"
                "UpdaterAutoCheck=0\r\n");
    }
}

QString MpcPlayer::executable()
{
    if (Components *c = Components::instance()) {
        const QString exe = c->executable(Components::MpcHc);
        if (!exe.isEmpty()) {
            configure(QFileInfo(exe).absolutePath());
            return exe;
        }
    }
    const QString installed = mpcHcPath();
    return QFileInfo::exists(installed) ? installed : QString();
}

bool MpcPlayer::play(const QStringList &files, bool closeAtEnd)
{
    if (files.isEmpty()) {
        return false;
    }
    const QString exe = executable();
    if (exe.isEmpty()) {
        // Nog niet aanwezig: ophalen en daarna meteen afspelen
        Components *c = Components::instance();
        if (!c) {
            return false;
        }
        qInfo() << "MPC-HC: not present yet, downloading first";
        c->ensure(Components::MpcHc, [this, files, closeAtEnd](bool ok) {
            if (ok) {
                play(files, closeAtEnd);
            }
        });
        return true;
    }
    if (running()) {
        // Nieuwe keuze: het lopende venster netjes sluiten, MPC-HC onthoudt de positie
        m_process->terminate();
        m_process->waitForFinished(3000);
    }
    QStringList args;
    for (const QString &f : files) {
        args << QDir::toNativeSeparators(f);
    }
    // /new: altijd een eigen venster dat bij Kijkluister hoort; /fullscreen /play;
    // bij een film na afloop sluiten, bij een serie de volgende aflevering laten spelen
    args << QStringLiteral("/new") << QStringLiteral("/fullscreen") << QStringLiteral("/play");
    if (closeAtEnd) {
        args << QStringLiteral("/close");
    }
    delete m_process;
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, [this](int code) {
        qInfo() << "MPC-HC: closed" << code;
        Q_EMIT runningChanged();
        Q_EMIT finished();
    });
    m_process->setProgram(exe);
    m_process->setArguments(args);
    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "MPC-HC: could not start" << exe << m_process->errorString();
        return false;
    }
    qInfo().noquote() << "MPC-HC: playing" << files.size() << "file(s), first" << files.first();
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(DWORD(m_process->processId()));
#endif
    Q_EMIT runningChanged();
    return true;
}

void MpcPlayer::sendVirtualKey(unsigned short vk, bool pressed, bool alt)
{
#ifdef Q_OS_WIN
    INPUT inputs[2] = {};
    int n = 0;
    if (alt && pressed) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_MENU;
        ++n;
    }
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = vk;
    inputs[n].ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    ++n;
    SendInput(UINT(n), inputs, sizeof(INPUT));
    if (alt && !pressed) {
        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_MENU;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &up, sizeof(INPUT));
    }
#else
    Q_UNUSED(vk) Q_UNUSED(pressed) Q_UNUSED(alt)
#endif
}

// Standaardtoetsen van MPC-HC: spatie pauze, ←/→ spoelen, ↑/↓ volume,
// PageUp/PageDown vorige/volgende in de lijst, Alt+X afsluiten
bool MpcPlayer::handleButton(int sdlButton, bool pressed)
{
#ifdef Q_OS_WIN
    if (!running()) {
        return false;
    }
    // Alleen als MPC-HC zelf voorop staat (anders bedient de controller iets anders)
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != DWORD(m_process->processId())) {
        return false;
    }
    switch (sdlButton) {
    case SDL_GAMEPAD_BUTTON_SOUTH: sendVirtualKey(VK_SPACE, pressed); return true;           // A: pauze/verder
    case SDL_GAMEPAD_BUTTON_EAST: sendVirtualKey('X', pressed, true); return true;           // B: stoppen
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: sendVirtualKey(VK_LEFT, pressed); return true;        // terugspoelen
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: sendVirtualKey(VK_RIGHT, pressed); return true;      // vooruitspoelen
    case SDL_GAMEPAD_BUTTON_DPAD_UP: sendVirtualKey(VK_UP, pressed); return true;            // harder
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: sendVirtualKey(VK_DOWN, pressed); return true;        // zachter
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: sendVirtualKey(VK_PRIOR, pressed); return true;   // vorige aflevering
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: sendVirtualKey(VK_NEXT, pressed); return true;   // volgende aflevering
    case SDL_GAMEPAD_BUTTON_NORTH: sendVirtualKey('S', pressed); return true;                // Y: ondertitels wisselen
    case SDL_GAMEPAD_BUTTON_WEST: sendVirtualKey('A', pressed); return true;                 // X: audiospoor wisselen
    default: return false;
    }
#else
    Q_UNUSED(sdlButton) Q_UNUSED(pressed)
    return false;
#endif
}

void MpcPlayer::stop()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "MPC-HC: stop (sleep timer)";
        m_process->terminate();   // WM_CLOSE: MPC-HC slaat de positie op
    }
}
