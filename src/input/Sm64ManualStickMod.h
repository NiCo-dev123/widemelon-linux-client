#pragma once

#include <cstdint>

#include "input/EvdevInput.h"
#include "input/Sm64StickMod.h"
#include "input/StylusIntent.h"

namespace widemelon
{

class Sm64ManualStickMod
{
public:
    void setEnabled(bool enabled);
    bool enabled() const { return isEnabled; }
    bool update(const LeftStickState& stick);
    const StylusIntent &intent() const { return stylusIntent; }
    bool cursorPressed() const { return stylusIntent.pressed; }
    bool cursorTracking() const { return false; }
    float currentCursorSpeed() const { return 0.0F; }

private:
    static int mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius);

    bool isEnabled = false;
    StylusIntent stylusIntent{StylusSource::LeftStick, Sm64TouchCenterX, Sm64TouchCenterY, false, false, StylusPressSemantics::Toggle};
};

}
