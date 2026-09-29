#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr std::uint16_t Sm64TouchPressRadius{2};
inline constexpr auto Sm64TouchReleaseDelay{std::chrono::milliseconds{500}};
inline constexpr std::uint16_t Sm64TouchMaximumRadius{35};
inline constexpr float Sm64TouchCursorAcceleration{300.0F};
inline constexpr float Sm64TouchMaximumCursorSpeed{240.0F};
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
    bool cursorPressed() const { return isCursorPressed; }
    bool cursorTracking() const { return isCursorTracking; }
    float currentCursorSpeed() const { return cursorSpeed; }

private:
    static int mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius);
    static void moveTowards(std::uint16_t currentX, std::uint16_t currentY, std::uint16_t targetX, std::uint16_t targetY,
                            float maximumDistance, std::uint16_t& nextX, std::uint16_t& nextY);
    bool isEnabled = false;
    bool isCursorPressed = false;
    bool isCursorTracking = false;
    std::chrono::steady_clock::time_point releaseStartedAt{};
    float cursorSpeed = 0.0F;
    std::chrono::steady_clock::time_point lastCursorUpdateAt{};
    Sm64TouchState touch;
};

}
