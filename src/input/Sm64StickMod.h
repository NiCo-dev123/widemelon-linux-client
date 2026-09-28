#pragma once

#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr std::uint16_t Sm64TouchDeadzoneRadius{10};
inline constexpr std::uint16_t Sm64TouchAnalogRadius{25};
inline constexpr std::uint16_t Sm64TouchMaximumRadius{40};

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
    static int mapRelativeAxis(int value, int origin, int minimum, int maximum, std::uint16_t radius);
    static std::uint16_t clampTouchCoordinate(int value, std::uint16_t maximum, std::uint16_t margin);

    bool isEnabled = false;
    bool r2WasPressed = false;
    int stickOriginX = 0;
    int stickOriginY = 0;
    std::uint16_t touchCenterX = 127;
    std::uint16_t touchCenterY = 95;
    Sm64TouchState touch;
};

}
