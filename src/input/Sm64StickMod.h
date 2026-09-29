#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr std::uint16_t Sm64TouchPressRadius{2};
inline constexpr std::uint16_t Sm64TouchReleaseRadius{1};
inline constexpr std::chrono::milliseconds Sm64TouchCentreHoldDuration{33};
inline constexpr std::chrono::milliseconds Sm64TouchStabilizationDuration{100};
inline constexpr std::chrono::milliseconds Sm64TouchReleaseHoldDuration{33};
inline constexpr std::uint16_t Sm64TouchMaximumRadius{35};
inline constexpr std::uint16_t Sm64TouchInitialMaximumStep{5};
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
    bool enabled() const { return isEnabled; }
    bool update(const LeftStickState& stick);
    const Sm64TouchState& touchState() const { return touch; }

private:
    static int mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius);
    static void moveTowards(std::uint16_t currentX, std::uint16_t currentY, std::uint16_t targetX, std::uint16_t targetY,
                            std::uint16_t maximumStep, std::uint16_t& nextX, std::uint16_t& nextY);
    bool isEnabled = false;
    bool touchStarted = false;
    bool releasePending = false;
    std::chrono::steady_clock::time_point movementAt{};
    std::chrono::steady_clock::time_point stabilizationEndsAt{};
    std::chrono::steady_clock::time_point releaseAt{};
    Sm64TouchState touch;
};

}
