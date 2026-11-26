// Stub voor platforms buiten Windows, puur zodat dit PoC-skeleton overal
// blijft compileren. Geen onderdeel van het migratieplan zelf.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "inputsystem.h"

class NoopInputSystem : public InputSystem
{
public:
    bool init() override { return true; }
    void emitKey(int, bool) override {}
    void emitPointerMotion(double, double) override {}
    void emitPointerButton(int, bool) override {}
};
