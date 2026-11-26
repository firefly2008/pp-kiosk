// SPDX-License-Identifier: GPL-2.0-or-later
#include "win32inputsystem.h"

#include <windows.h>

bool Win32InputSystem::init()
{
    // Geen pre-authorize/createSession zoals de XDG-portal-versie nodig had:
    // dit proces mag altijd input genereren voor zijn eigen venster/desktop.
    return true;
}

void Win32InputSystem::emitKey(int virtualKeyCode, bool pressed)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(virtualKeyCode);
    input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void Win32InputSystem::emitPointerMotion(double deltaX, double deltaY)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = static_cast<LONG>(deltaX);
    input.mi.dy = static_cast<LONG>(deltaY);
    SendInput(1, &input, sizeof(INPUT));
}

void Win32InputSystem::emitPointerButton(int button, bool pressed)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = (button == 0)
        ? (pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
        : (pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    SendInput(1, &input, sizeof(INPUT));
}
