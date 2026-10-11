#include "input/StickDpadMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{
namespace
{
constexpr std::uint16_t ButtonRight = 1U << 4;
constexpr std::uint16_t ButtonLeft = 1U << 5;
constexpr std::uint16_t ButtonUp = 1U << 6;
constexpr std::uint16_t ButtonDown = 1U << 7;
constexpr float DpadThreshold{0.25F};

float normalizedAxis(int value, int minimum, int maximum, int center)
{
    const int distance = value < center ? center - minimum : maximum - center;
    return distance > 0 ? std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F) : 0.0F;
}
}

void StickDpadMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    buttons = 0;
}

bool StickDpadMod::update(const LeftStickState &stick)
{
    const std::uint16_t previous = buttons;
    if (!isEnabled)
    {
        buttons = 0;
        return previous != buttons;
    }

    const float horizontal = normalizedAxis(stick.x, stick.xMinimum, stick.xMaximum, stick.xCenter);
    const float vertical = normalizedAxis(stick.y, stick.yMinimum, stick.yMaximum, stick.yCenter);
    buttons = static_cast<std::uint16_t>(
        (horizontal <= -DpadThreshold ? ButtonLeft : 0)
        | (horizontal >= DpadThreshold ? ButtonRight : 0)
        | (vertical <= -DpadThreshold ? ButtonUp : 0)
        | (vertical >= DpadThreshold ? ButtonDown : 0));
    return previous != buttons;
}
}
