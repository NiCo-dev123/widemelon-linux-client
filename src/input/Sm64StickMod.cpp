#include "input/Sm64StickMod.h"

#include <algorithm>
#include <cmath>

// Touch activation follows the DeSmuME Libretro "pressed" pointer model:
// https://github.com/libretro/desmume/blob/master/desmume/src/frontend/libretro/libretro.cpp

namespace widemelon
{

void Sm64StickMod::setCenterHoldFrames(std::uint8_t frames)
{
    centerHoldFrameLimit = std::clamp(frames, Sm64TouchCenterHoldFramesMinimum, Sm64TouchCenterHoldFramesMaximum);
}

void Sm64StickMod::setReleaseDelayMs(std::uint16_t milliseconds)
{
    releaseDelay = std::chrono::milliseconds(std::clamp(milliseconds, Sm64TouchReleaseDelayMinimumMs, Sm64TouchReleaseDelayMaximumMs));
}

void Sm64StickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    isCursorPressed = false;
    releaseStartedAt = {};
    centerHoldFrames = 0;
    touch = {};
}

int Sm64StickMod::mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0;
    const float normalized = std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    return static_cast<int>(std::lround(normalized * static_cast<float>(radius)));
}

bool Sm64StickMod::update(const LeftStickState& stick)
{
    if (!isEnabled) return false;

    int offsetX = mapRelativeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum, Sm64TouchMaximumRadius);
    int offsetY = mapRelativeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum, Sm64TouchMaximumRadius);
    float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetY * offsetY));
    if (distance > static_cast<float>(Sm64TouchMaximumRadius))
    {
        const float scale = static_cast<float>(Sm64TouchMaximumRadius) / distance;
        offsetX = static_cast<int>(std::lround(static_cast<float>(offsetX) * scale));
        offsetY = static_cast<int>(std::lround(static_cast<float>(offsetY) * scale));
        distance = static_cast<float>(Sm64TouchMaximumRadius);
    }

    Sm64TouchState next = touch;
    const std::uint16_t targetX = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterX) + offsetX);
    const std::uint16_t targetY = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterY) + offsetY);

    const auto now = std::chrono::steady_clock::now();
    if (distance > static_cast<float>(Sm64TouchPressRadius))
    {
        releaseStartedAt = {};
        if (!isCursorPressed)
        {
            // Start one continuous contact from the centre, regardless of how
            // quickly the stick first reached its current position.
            isCursorPressed = true;
            centerHoldFrames = 1;
            next.active = true;
            next.x = Sm64TouchCenterX;
            next.y = Sm64TouchCenterY;
        }
        else
        {
            next.active = true;
            if (centerHoldFrames < centerHoldFrameLimit)
            {
                ++centerHoldFrames;
                next.x = Sm64TouchCenterX;
                next.y = Sm64TouchCenterY;
            }
            else
            {
                next.x = targetX;
                next.y = targetY;
            }
        }
    }
    else if (isCursorPressed)
    {
        // Keep the exact same contact through a time-based release hysteresis.
        if (releaseStartedAt == std::chrono::steady_clock::time_point{})
            releaseStartedAt = now;
        if (now - releaseStartedAt >= releaseDelay)
        {
            isCursorPressed = false;
            releaseStartedAt = {};
            centerHoldFrames = 0;
            next.active = false;
            next.x = Sm64TouchCenterX;
            next.y = Sm64TouchCenterY;
        }
        else
            next.active = true;
    }
    else
    {
        // An inactive cursor is always shown at the centre of the touch zone.
        next.active = false;
        next.x = Sm64TouchCenterX;
        next.y = Sm64TouchCenterY;
    }

    if (next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next;
    return true;
}

}
