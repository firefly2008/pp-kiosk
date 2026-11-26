// SPDX-License-Identifier: GPL-2.0-or-later
#include "homebridge.h"

#include <QCursor>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <SDL3/SDL_gamepad.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>


namespace {
HomeBridge *g_bridge = nullptr;
HHOOK g_hook = nullptr;

LRESULT CALLBACK keyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_bridge) {
        const auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        bool swallow = false;
        g_bridge->keyEvent(info->vkCode, down, swallow);
        if (swallow) {
            return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

QString processNameOf(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return {};
    }
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    QString name;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        name = QFileInfo(QString::fromWCharArray(path, int(size))).fileName().toLower();
    }
    CloseHandle(process);
    return name;
}

// Opdrachtregel van een ander proces (Windows 8.1+: ProcessCommandLineInformation)
QString commandLineOf(DWORD pid)
{
    using NtQip = LONG(NTAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static const auto query = reinterpret_cast<NtQip>(
        reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess")));
    if (!query) {
        return {};
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return {};
    }
    QString result;
    ULONG needed = 0;
    query(process, 60, nullptr, 0, &needed);
    if (needed > 0 && needed < 1 << 20) {
        QByteArray buffer(int(needed), Qt::Uninitialized);
        if (query(process, 60, buffer.data(), needed, &needed) >= 0) {
            struct Us { USHORT length; USHORT maximum; PWSTR text; };
            const auto *us = reinterpret_cast<const Us *>(buffer.constData());
            if (us->text) {
                result = QString::fromWCharArray(us->text, us->length / 2);
            }
        }
    }
    CloseHandle(process);
    return result;
}

BOOL CALLBACK collectOwnBrowser(HWND hwnd, LPARAM param)
{
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) {
        return TRUE;
    }
    const QString exe = processNameOf(hwnd);
    if (exe != QLatin1String("msedge.exe") && exe != QLatin1String("chrome.exe")) {
        return TRUE;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    // Onze profielmappen: %LOCALAPPDATA%\Kijkluister\browser en \browser-nieuws
    if (commandLineOf(pid).contains(QLatin1String("\\Kijkluister\\browser"), Qt::CaseInsensitive)) {
        reinterpret_cast<QList<HWND> *>(param)->append(hwnd);
    }
    return TRUE;
}
}
#endif

int HomeBridge::closeOwnBrowserWindows()
{
#ifdef Q_OS_WIN
    QList<HWND> windows;
    EnumWindows(collectOwnBrowser, reinterpret_cast<LPARAM>(&windows));
    for (HWND w : std::as_const(windows)) {
        PostMessageW(w, WM_CLOSE, 0, 0);
    }
    if (!windows.isEmpty()) {
        qInfo() << "Closed" << windows.size() << "own browser window(s)";
    }
    return int(windows.size());
#else
    return 0;
#endif
}

namespace {
HomeBridge *g_instance = nullptr;
}

HomeBridge *HomeBridge::instance()
{
    return g_instance;
}

HomeBridge::HomeBridge(QWindow *main, QObject *parent)
    : QObject(parent)
    , m_main(main)
{
    g_instance = this;
    m_backHold.setSingleShot(true);
    m_backHold.setInterval(900);
    connect(&m_backHold, &QTimer::timeout, this, [this] {
        m_backFired = true;
        qInfo() << "Home: back held";
        goHome();
    });
    m_poll.setInterval(250);
    connect(&m_poll, &QTimer::timeout, this, &HomeBridge::poll);
    m_escHold.setSingleShot(true);
    m_escHold.setInterval(800);
    connect(&m_escHold, &QTimer::timeout, this, [this] {
        if (m_escDown && m_external) {
            qInfo() << "Home: Esc held";
            goHome();
        }
    });
#ifdef Q_OS_WIN
    g_bridge = this;
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, GetModuleHandleW(nullptr), 0);
    qInfo() << "Home: keyboard hook" << (g_hook ? "installed" : "FAILED");
#endif
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && m_external) {
            m_external = false;
            setShowButton(false);
            m_poll.stop();
            Q_EMIT changed();
        }
    });
}

HomeBridge::~HomeBridge()
{
    g_instance = nullptr;
#ifdef Q_OS_WIN
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
    }
    g_bridge = nullptr;
#endif
}

void HomeBridge::externalLaunched()
{
    m_external = true;
    m_launchedAt = QDateTime::currentMSecsSinceEpoch();
    m_poll.start();
    Q_EMIT changed();
}

// Home-knop: de eerste 6 seconden na het openen, en zolang de muis bovenaan het scherm is
void HomeBridge::poll()
{
    if (!m_external) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QPoint pos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(pos);
    if (screen && pos.y() - screen->geometry().top() < screen->geometry().height() / 12) {
        m_lastNearTop = now;
    }
    setShowButton(now - m_launchedAt < 6000 || now - m_lastNearTop < 2500);
}

void HomeBridge::setShowButton(bool show)
{
    if (show != m_showButton) {
        m_showButton = show;
        Q_EMIT changed();
    }
}

void HomeBridge::keyEvent(unsigned long vk, bool down, bool &swallow)
{
#ifdef Q_OS_WIN
    if (vk == VK_BROWSER_HOME) {
        // Home-toets van afstandsbediening/toetsenbord: altijd naar Kijkluister (niet de startpagina van de browser)
        swallow = true;
        if (down) {
            qInfo() << "Home: home key";
            QMetaObject::invokeMethod(this, &HomeBridge::goHome, Qt::QueuedConnection);
        }
        return;
    }
    if (vk == VK_ESCAPE) {
        if (down && !m_escDown) {
            m_escDown = true;
            QMetaObject::invokeMethod(this, &HomeBridge::escPressed, Qt::QueuedConnection);
        } else if (!down) {
            m_escDown = false;
            QMetaObject::invokeMethod(&m_escHold, &QTimer::stop, Qt::QueuedConnection);
        }
    }
#else
    Q_UNUSED(vk)
    Q_UNUSED(down)
    Q_UNUSED(swallow)
#endif
}

void HomeBridge::setEscMode(const QString &mode)
{
    m_escMode = (mode == QLatin1String("double") || mode == QLatin1String("direct")) ? mode : QStringLiteral("hold");
    qInfo() << "Home: Esc mode" << m_escMode;
}

// Esc ingedrukt (hoofdthread). Alleen in een app die wij openden telt het.
void HomeBridge::escPressed()
{
    if (!m_external) {
        return;
    }
    if (m_escMode == QLatin1String("direct")) {
        qInfo() << "Home: Esc direct";
        goHome();
    } else if (m_escMode == QLatin1String("double")) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_escLastMs < 600) {
            m_escLastMs = 0;
            qInfo() << "Home: Esc double";
            goHome();
        } else {
            m_escLastMs = now;
        }
    } else {
        m_escHold.start();
    }
}

void HomeBridge::goHome()
{
#ifdef Q_OS_WIN
    // Alle diensten die we openden sluiten, ook die achter andere vensters (niet alleen het voorste)
    closeOwnBrowserWindows();
    HWND foreground = GetForegroundWindow();
    const HWND ours = m_main ? reinterpret_cast<HWND>(m_main->winId()) : nullptr;
    if (foreground && foreground != ours && m_external) {
        const QString exe = processNameOf(foreground);
        if (exe == QLatin1String("msedge.exe") || exe == QLatin1String("chrome.exe") || exe.startsWith(QLatin1String("mpc-hc"))) {
            qInfo() << "Home: closing" << exe;
            PostMessageW(foreground, WM_CLOSE, 0, 0);
        } else if (!exe.isEmpty() && exe != QLatin1String("explorer.exe")) {
            qInfo() << "Home: minimizing" << exe;
            ShowWindow(foreground, SW_MINIMIZE);
        }
    }
    if (ours) {
        // Naar voren halen, ook als Windows dat normaal tegenhoudt
        const DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        const DWORD ourThread = GetCurrentThreadId();
        if (fgThread != ourThread) {
            AttachThreadInput(fgThread, ourThread, TRUE);
        }
        ShowWindow(ours, SW_SHOW);
        SetForegroundWindow(ours);
        if (fgThread != ourThread) {
            AttachThreadInput(fgThread, ourThread, FALSE);
        }
    }
#endif
    m_external = false;
    setShowButton(false);
    m_poll.stop();
    Q_EMIT changed();
    Q_EMIT homeRequested();
}

#ifdef Q_OS_WIN
namespace {
void tapVirtualKey(WORD vk, bool down, bool withAlt = false)
{
    INPUT inputs[2] = {};
    int n = 0;
    if (withAlt && down) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_MENU;
        ++n;
    }
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = vk;
    if (!down) {
        inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) {
        inputs[n].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    ++n;
    SendInput(n, inputs, sizeof(INPUT));
    if (withAlt && !down) {
        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_MENU;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &up, sizeof(INPUT));
    }
}
}
#endif

bool HomeBridge::handleBrowserButton(int button, bool pressed)
{
#ifdef Q_OS_WIN
    if (!m_external) {
        return false;
    }
    const QString exe = processNameOf(GetForegroundWindow());
    if (exe != QLatin1String("msedge.exe") && exe != QLatin1String("chrome.exe")) {
        return false;
    }
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: tapVirtualKey(VK_UP, pressed); return true;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: tapVirtualKey(VK_DOWN, pressed); return true;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: tapVirtualKey(VK_LEFT, pressed); return true;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: tapVirtualKey(VK_RIGHT, pressed); return true;
    case SDL_GAMEPAD_BUTTON_SOUTH: tapVirtualKey(VK_RETURN, pressed); return true;
    case SDL_GAMEPAD_BUTTON_EAST:
        // Kort: vorige pagina (Alt+Links). Vasthouden: terug naar de tegels
        if (pressed) {
            m_backFired = false;
            m_backHold.start();
        } else if (!m_backFired) {
            m_backHold.stop();
            tapVirtualKey(VK_LEFT, true, true);
            tapVirtualKey(VK_LEFT, false, true);
        }
        return true;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: tapVirtualKey(VK_PRIOR, pressed); return true;   // pagina omhoog
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: tapVirtualKey(VK_NEXT, pressed); return true;  // pagina omlaag
    case SDL_GAMEPAD_BUTTON_NORTH: tapVirtualKey(VK_SPACE, pressed); return true;          // pauze/afspelen
    default:
        return false;
    }
#else
    Q_UNUSED(button)
    Q_UNUSED(pressed)
    return false;
#endif
}

void HomeBridge::startDictation()
{
#ifdef Q_OS_WIN
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_LWIN;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'H';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'H'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_LWIN; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    qInfo() << "Dictation started";
#endif
}

void HomeBridge::raiseWindow()
{
#ifdef Q_OS_WIN
    const HWND ours = m_main ? reinterpret_cast<HWND>(m_main->winId()) : nullptr;
    if (!ours) {
        return;
    }
    const DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD ourThread = GetCurrentThreadId();
    if (fgThread != ourThread) {
        AttachThreadInput(fgThread, ourThread, TRUE);
    }
    ShowWindow(ours, SW_SHOW);
    SetForegroundWindow(ours);
    if (fgThread != ourThread) {
        AttachThreadInput(fgThread, ourThread, FALSE);
    }
#endif
}
