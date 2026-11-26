// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "inputsystem.h"

class Win32InputSystem : public InputSystem
{
public:
    bool init() override;
    void emitKey(int virtualKeyCode, bool pressed) override;
    void emitPointerMotion(double deltaX, double deltaY) override;
    void emitPointerButton(int button, bool pressed) override;
};
