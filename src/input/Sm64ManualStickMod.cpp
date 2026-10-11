#include "input/Sm64ManualStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

void Sm64ManualStickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    previousR2Pressed = false;
    stylusIntent = {StylusSource::LeftStick, Sm64TouchCenterX, Sm64TouchCenterY, false, false, StylusPressSemantics::Toggle};
}

int Sm64ManualStickMod::mapRelativeAxis(int value, int center, int minimum, int maximum, std::uint16_t radius)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0;
    const float normalized = std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    return static_cast<int>(std::lround(normalized * static_cast<float>(radius)));
}

bool Sm64ManualStickMod::update(const LeftStickState& stick, bool r2Pressed)
{
    if (!isEnabled) return false;

    int offsetX = mapRelativeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum, Sm64TouchMaximumRadius);
    int offsetY = mapRelativeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum, Sm64TouchMaximumRadius);
    const float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetY * offsetY));
    if (distance > static_cast<float>(Sm64TouchMaximumRadius))
    {
        const float scale = static_cast<float>(Sm64TouchMaximumRadius) / distance;
        offsetX = static_cast<int>(std::lround(static_cast<float>(offsetX) * scale));
        offsetY = static_cast<int>(std::lround(static_cast<float>(offsetY) * scale));
    }

    StylusIntent next = stylusIntent;
    next.x = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterX) + offsetX);
    next.y = static_cast<std::uint16_t>(static_cast<int>(Sm64TouchCenterY) + offsetY);
    next.sourceActive = distance > static_cast<float>(Sm64TouchPressRadius);
    if (r2Pressed && !previousR2Pressed) next.pressed = !next.pressed;
    previousR2Pressed = r2Pressed;

    if (sameStylusIntent(next, stylusIntent)) return false;
    stylusIntent = next;
    return true;
}

}
