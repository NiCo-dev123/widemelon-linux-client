#include "input/CursorStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

void CursorStickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    cursorX = Sm64TouchCenterX;
    cursorY = Sm64TouchCenterY;
    lastUpdateAt = {};
    stylusIntent = {source, Sm64TouchCenterX, Sm64TouchCenterY, false, false, StylusPressSemantics::Hold};
}

void CursorStickMod::setSource(StylusSource value)
{
    source = value;
    stylusIntent.source = value;
}

void CursorStickMod::setSpeedLimit(std::uint16_t pixelsPerSecond)
{
    speedLimit = std::clamp(pixelsPerSecond, CursorSpeedMinimum, CursorSpeedMaximum);
}

float CursorStickMod::normalizeAxis(int value, int center, int minimum, int maximum)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0.0F;
    return std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
}

bool CursorStickMod::update(const LeftStickState& stick, bool r2Pressed)
{
    if (!isEnabled) return false;

    const auto now = std::chrono::steady_clock::now();
    float elapsedSeconds = 0.0F;
    if (lastUpdateAt != std::chrono::steady_clock::time_point{})
        elapsedSeconds = std::min(std::chrono::duration<float>(now - lastUpdateAt).count(), 0.050F);
    lastUpdateAt = now;

    float horizontal = normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    float vertical = normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    const float magnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    if (magnitude > 1.0F)
    {
        horizontal /= magnitude;
        vertical /= magnitude;
    }

    const float speed = speedLimit * std::min(magnitude, 1.0F);
    cursorX = std::clamp(cursorX + horizontal * speed * elapsedSeconds, 0.0F, 255.0F);
    cursorY = std::clamp(cursorY + vertical * speed * elapsedSeconds, 0.0F, 191.0F);

    StylusIntent next = stylusIntent;
    next.source = source;
    next.pressed = r2Pressed;
    next.sourceActive = magnitude > 0.05F;
    next.x = static_cast<std::uint16_t>(std::lround(cursorX));
    next.y = static_cast<std::uint16_t>(std::lround(cursorY));
    if (sameStylusIntent(next, stylusIntent)) return false;
    stylusIntent = next;
    return true;
}

}
