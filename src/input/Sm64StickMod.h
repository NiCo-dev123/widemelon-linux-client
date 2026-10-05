#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr std::uint16_t Sm64TouchPressRadius{2};
inline constexpr std::uint16_t Sm64TouchReleaseDelayMinimumMs{0};
inline constexpr std::uint16_t Sm64TouchReleaseDelayDefaultMs{1000};
inline constexpr std::uint16_t Sm64TouchReleaseDelayMaximumMs{2000};
inline constexpr std::uint16_t Sm64TouchReleaseDelayStepMs{100};
inline constexpr std::uint8_t Sm64TouchCenterHoldFramesMinimum{2};
inline constexpr std::uint8_t Sm64TouchCenterHoldFramesDefault{12};
inline constexpr std::uint8_t Sm64TouchCenterHoldFramesMaximum{20};
inline constexpr std::uint16_t Sm64TouchMaximumRadius{35};
inline constexpr std::uint16_t Sm64TouchCenterX{127};
inline constexpr std::uint16_t Sm64TouchCenterY{95};

struct Sm64TouchState
{
    bool active = false;
    std::uint16_t x = Sm64TouchCenterX;
    std::uint16_t y = Sm64TouchCenterY;
};

class Sm64StickMod
{
public:
    void setEnabled(bool enabled);
    void setCenterHoldFrames(std::uint8_t frames);
    void setReleaseDelayMs(std::uint16_t milliseconds);
    bool enabled() const { return isEnabled; }
    bool update(const LeftStickState& stick);
    const Sm64TouchState& touchState() const { return touch; }
    bool cursorPressed() const { return isCursorPressed; }

private:
    static int mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius);
    bool isEnabled = false;
    bool isCursorPressed = false;
    std::chrono::steady_clock::time_point releaseStartedAt{};
    std::uint8_t centerHoldFrames = 0;
    std::uint8_t centerHoldFrameLimit = Sm64TouchCenterHoldFramesDefault;
    std::chrono::milliseconds releaseDelay{Sm64TouchReleaseDelayDefaultMs};
    Sm64TouchState touch;
};

}
