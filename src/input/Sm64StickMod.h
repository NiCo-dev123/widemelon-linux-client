#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr std::uint16_t Sm64TouchActivationRadius{2};
inline constexpr std::chrono::milliseconds Sm64TouchCentreHoldDuration{40};
inline constexpr std::chrono::milliseconds Sm64TouchReleaseHoldDuration{33};
inline constexpr std::uint16_t Sm64TouchDeadzoneRadius{10};
inline constexpr std::uint16_t Sm64TouchAnalogRadius{25};
inline constexpr std::uint16_t Sm64TouchMaximumRadius{45};
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

    bool isEnabled = false;
    bool touchStarted = false;
    bool releasePending = false;
    std::chrono::steady_clock::time_point movementAt{};
    std::chrono::steady_clock::time_point releaseAt{};
    Sm64TouchState touch;
};

}
