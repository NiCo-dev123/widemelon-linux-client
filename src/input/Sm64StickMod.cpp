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

std::uint16_t Sm64StickMod::mapAxis(int value, int minimum, int maximum, int center, std::uint16_t destinationMaximum)
{
    if (maximum <= minimum) return destinationMaximum / 2;
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return destinationMaximum / 2;
    const float normalized = std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    const float mapped = (normalized + 1.0F) * static_cast<float>(destinationMaximum) / 2.0F;
    return static_cast<std::uint16_t>(std::lround(mapped));
}

bool Sm64StickMod::update(const LeftStickState& stick, bool r2Pressed)
{
    if (!isEnabled) return false;
    Sm64TouchState next;
    next.active = r2Pressed;
    next.x = mapAxis(stick.x, stick.xMinimum, stick.xMaximum, stick.xCenter, 255);
    next.y = mapAxis(stick.y, stick.yMinimum, stick.yMaximum, stick.yCenter, 191);
    if (next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next;
    return true;
}

}
