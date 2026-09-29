#pragma once

#include <cstdint>

#include "input/EvdevInput.h"
#include "input/Sm64StickMod.h"

namespace widemelon
{

class Sm64ManualStickMod
{
public:
    void setEnabled(bool enabled);
    bool enabled() const { return isEnabled; }
    bool update(const LeftStickState& stick, bool r2Pressed);
    const Sm64TouchState& touchState() const { return touch; }
    bool cursorPressed() const { return touch.active; }
    bool cursorTracking() const { return false; }
    float currentCursorSpeed() const { return 0.0F; }

private:
    static int mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius);

    bool isEnabled = false;
    bool previousR2Pressed = false;
    Sm64TouchState touch;
};

}
