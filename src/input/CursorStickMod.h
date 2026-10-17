#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"
#include "input/CursorSettings.h"
#include "input/Sm64StickMod.h"
#include "input/StylusIntent.h"

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
    void setSource(StylusSource value);
    void setSpeedLimit(std::uint16_t pixelsPerSecond);
    void setDeadzonePercent(std::uint8_t percent);
    bool isStickActive(const LeftStickState &stick) const;
    void setCursorPosition(std::uint16_t x, std::uint16_t y);
    bool update(const LeftStickState& stick);
    const StylusIntent &intent() const { return stylusIntent; }

private:
    static float normalizeAxis(int value, int center, int minimum, int maximum);
    bool outsideDeadzone(float magnitude) const;

    bool isEnabled = false;
    std::uint16_t speedLimit = CursorSpeedDefault;
    std::uint8_t deadzonePercent = CursorDeadzoneDefaultPercent;
    float cursorX = Sm64TouchCenterX;
    float cursorY = Sm64TouchCenterY;
    std::chrono::steady_clock::time_point lastUpdateAt{};
    StylusSource source{StylusSource::LeftStick};
    StylusIntent stylusIntent{StylusSource::LeftStick, Sm64TouchCenterX, Sm64TouchCenterY, false, false, StylusPressSemantics::Hold};
};

}
