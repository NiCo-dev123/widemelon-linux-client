#include "input/Sm64StickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

void Sm64StickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
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

    Sm64TouchState next;
    next.x = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterX) + offsetX);
    next.y = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterY) + offsetY);
    next.active = distance > static_cast<float>(Sm64TouchActivationRadius);

    if (next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next;
    return true;
}

}
