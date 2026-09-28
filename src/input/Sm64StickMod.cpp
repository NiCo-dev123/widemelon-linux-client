#include "input/Sm64StickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

void Sm64StickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    r2WasPressed = false;
    touch = {};
}

std::uint16_t Sm64StickMod::mapAxis(int value, int minimum, int maximum, int center, std::uint16_t destinationMaximum)
{
    if (maximum <= minimum) return destinationMaximum / 2;
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return destinationMaximum / 2;
    const float normalized = std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    const float mapped = (normalized + 1.0F) * static_cast<float>(destinationMaximum) / 2.0F;
    return static_cast<std::uint16_t>(std::lround(mapped));
}

int Sm64StickMod::mapRelativeAxis(int value, int origin, int minimum, int maximum, std::uint16_t radius)
{
    const int distance = value < origin ? origin - minimum : maximum - origin;
    if (distance <= 0) return 0;
    const float normalized = std::clamp(static_cast<float>(value - origin) / static_cast<float>(distance), -1.0F, 1.0F);
    return static_cast<int>(std::lround(normalized * static_cast<float>(radius)));
}

std::uint16_t Sm64StickMod::clampTouchCoordinate(int value, std::uint16_t maximum, std::uint16_t margin)
{
    const int lower = std::min<int>(margin, maximum);
    const int upper = std::max(lower, static_cast<int>(maximum) - margin);
    return static_cast<std::uint16_t>(std::clamp(value, lower, upper));
}

bool Sm64StickMod::update(const LeftStickState& stick, bool r2Pressed)
{
    if (!isEnabled) return false;

    const std::uint16_t freeX = mapAxis(stick.x, stick.xMinimum, stick.xMaximum, stick.xCenter, 255);
    const std::uint16_t freeY = mapAxis(stick.y, stick.yMinimum, stick.yMaximum, stick.yCenter, 191);
    Sm64TouchState next;

    if (!r2Pressed)
    {
        next.active = false;
        next.x = freeX;
        next.y = freeY;
    }
    else
    {
        if (!r2WasPressed)
        {
            touchCenterX = clampTouchCoordinate(freeX, 255, Sm64TouchMaximumRadius);
            touchCenterY = clampTouchCoordinate(freeY, 191, Sm64TouchMaximumRadius);
            stickOriginX = stick.x;
            stickOriginY = stick.y;
        }
        int offsetX = mapRelativeAxis(stick.x, stickOriginX, stick.xMinimum, stick.xMaximum, Sm64TouchMaximumRadius);
        int offsetY = mapRelativeAxis(stick.y, stickOriginY, stick.yMinimum, stick.yMaximum, Sm64TouchMaximumRadius);
        const float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetY * offsetY));
        if (distance > static_cast<float>(Sm64TouchMaximumRadius))
        {
            const float scale = static_cast<float>(Sm64TouchMaximumRadius) / distance;
            offsetX = static_cast<int>(std::lround(static_cast<float>(offsetX) * scale));
            offsetY = static_cast<int>(std::lround(static_cast<float>(offsetY) * scale));
        }
        next.active = true;
        next.x = static_cast<std::uint16_t>(touchCenterX + offsetX);
        next.y = static_cast<std::uint16_t>(touchCenterY + offsetY);
    }

    r2WasPressed = r2Pressed;
    if (next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next;
    return true;
}

}
