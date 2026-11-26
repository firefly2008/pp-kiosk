// Vervangt inputhandler/xdgremotedesktopsystem.h uit de Linux-codebase.
// Daar ging elke aanroep via D-Bus naar org.freedesktop.portal.RemoteDesktop,
// omdat een Wayland-app niet zonder toestemming in andere vensters mag injecteren.
// Op Windows is de Host de eigenaar van het enige venster: directe injectie,
// geen portal/toestemming nodig. Zie migratieplan, code-voorbeeld 2.
//
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class InputSystem
{
public:
    virtual ~InputSystem() = default;

    virtual bool init() = 0;
    virtual void emitKey(int virtualKeyCode, bool pressed) = 0;
    virtual void emitPointerMotion(double deltaX, double deltaY) = 0;
    virtual void emitPointerButton(int button, bool pressed) = 0;
};
