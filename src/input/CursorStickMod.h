#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"
#include "input/Sm64StickMod.h"

namespace widemelon
{

inline constexpr std::uint16_t CursorSpeedMinimum{100};
inline constexpr std::uint16_t CursorSpeedDefault{300};
inline constexpr std::uint16_t CursorSpeedMaximum{500};
inline constexpr std::uint16_t CursorSpeedStep{25};

class CursorStickMod
{
public:
    void setEnabled(bool enabled);
    void setSpeedLimit(std::uint16_t pixelsPerSecond);
    bool update(const LeftStickState& stick, bool r2Pressed);
    const Sm64TouchState& touchState() const { return touch; }

private:
    static float normalizeAxis(int value, int center, int minimum, int maximum);

    bool isEnabled = false;
    std::uint16_t speedLimit = CursorSpeedDefault;
    float cursorX = Sm64TouchCenterX;
    float cursorY = Sm64TouchCenterY;
    std::chrono::steady_clock::time_point lastUpdateAt{};
    Sm64TouchState touch;
};

}
