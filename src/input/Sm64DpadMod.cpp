#include "input/Sm64DpadMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

namespace
{
constexpr std::uint16_t ButtonB = 1U << 1;
}

void Sm64DpadMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    bHeld = false;
    releasePending = false;
    releaseAt = {};
}

float Sm64DpadMod::normalizedAxis(int value, int minimum, int maximum, int center)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0.0F;
    return std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
}

bool Sm64DpadMod::update(const LeftStickState& stick)
{
    const bool previous = bHeld;
    if (!isEnabled)
    {
        bHeld = false;
        releasePending = false;
        return previous != bHeld;
    }

    const float horizontal = normalizedAxis(stick.x, stick.xMinimum, stick.xMaximum, stick.xCenter);
    const float vertical = normalizedAxis(stick.y, stick.yMinimum, stick.yMaximum, stick.yCenter);
    const float magnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    const auto now = std::chrono::steady_clock::now();

    if (magnitude > Sm64DpadBActivationThreshold)
    {
        bHeld = true;
        releasePending = false;
    }
    else if (bHeld)
    {
        if (!releasePending)
        {
            releasePending = true;
            releaseAt = now + Sm64DpadBReleaseDelay;
        }
        if (now >= releaseAt)
        {
            bHeld = false;
            releasePending = false;
        }
    }
    return previous != bHeld;
}

std::uint16_t Sm64DpadMod::additionalButtons() const
{
    return bHeld ? ButtonB : 0;
}

}
