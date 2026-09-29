#include "input/Sm64StickMod.h"

#include <algorithm>
#include <cmath>

// Touch activation follows the DeSmuME Libretro "pressed" pointer model:
// https://github.com/libretro/desmume/blob/master/desmume/src/frontend/libretro/libretro.cpp

namespace widemelon
{

void Sm64StickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    isCursorPressed = false;
    isCursorTracking = false;
    releaseFrames = 0;
    cursorSpeed = 0.0F;
    lastCursorUpdateAt = {};
    touch = {};
}

int Sm64StickMod::mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0;
    const float normalized = std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    return static_cast<int>(std::lround(normalized * static_cast<float>(radius)));
}

void Sm64StickMod::moveTowards(std::uint16_t currentX, std::uint16_t currentY, std::uint16_t targetX, std::uint16_t targetY,
                               float maximumDistance, std::uint16_t& nextX, std::uint16_t& nextY)
{
    const int deltaX = static_cast<int>(targetX) - static_cast<int>(currentX);
    const int deltaY = static_cast<int>(targetY) - static_cast<int>(currentY);
    const float distance = std::sqrt(static_cast<float>(deltaX * deltaX + deltaY * deltaY));
    if (distance <= maximumDistance)
    {
        nextX = targetX;
        nextY = targetY;
        return;
    }

    const float scale = maximumDistance / distance;
    nextX = static_cast<std::uint16_t>(std::lround(static_cast<float>(currentX) + static_cast<float>(deltaX) * scale));
    nextY = static_cast<std::uint16_t>(std::lround(static_cast<float>(currentY) + static_cast<float>(deltaY) * scale));
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
        releaseFrames = 0;
        if (!isCursorPressed)
        {
            // Start one continuous contact from the centre, regardless of how
            // quickly the stick first reached its current position.
            isCursorPressed = true;
            isCursorTracking = false;
            cursorSpeed = 0.0F;
            lastCursorUpdateAt = now;
            next.active = true;
            next.x = Sm64TouchCenterX;
            next.y = Sm64TouchCenterY;
        }
        else
        {
            // Keep every coordinate transition continuous. Unlike the previous
            // implementation, entering tracking never teleports the stylus.
            const float elapsedSeconds = std::chrono::duration<float>(now - lastCursorUpdateAt).count();
            lastCursorUpdateAt = now;
            cursorSpeed = std::min(cursorSpeed + Sm64TouchCursorAcceleration * elapsedSeconds, Sm64TouchMaximumCursorSpeed);
            isCursorTracking = cursorSpeed >= Sm64TouchMaximumCursorSpeed;
            next.active = true;
            moveTowards(touch.x, touch.y, targetX, targetY, cursorSpeed * elapsedSeconds, next.x, next.y);
        }
    }
    else if (isCursorPressed)
    {
        // Keep the exact same contact through a four-frame centre hysteresis.
        ++releaseFrames;
        if (releaseFrames >= Sm64TouchReleaseFrames)
        {
            isCursorPressed = false;
            isCursorTracking = false;
            releaseFrames = 0;
            cursorSpeed = 0.0F;
            lastCursorUpdateAt = {};
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
