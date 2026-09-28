#pragma once

#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

struct Sm64TouchState
{
    bool active = false;
    std::uint16_t x = 127;
    std::uint16_t y = 95;
};

class Sm64StickMod
{
public:
    void setEnabled(bool enabled);
    bool enabled() const { return isEnabled; }
    bool update(const LeftStickState& stick, bool r2Pressed);
    const Sm64TouchState& touchState() const { return touch; }

private:
    static std::uint16_t mapAxis(int value, int minimum, int maximum, int center, std::uint16_t destinationMaximum);

    bool isEnabled = false;
    Sm64TouchState touch;
};

}
