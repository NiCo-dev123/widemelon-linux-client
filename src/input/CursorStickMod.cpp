#include "input/CursorStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{

void CursorStickMod::setEnabled(bool enabled)
{
    isEnabled = enabled;
    didWrap = false;
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

void CursorStickMod::setDeadzonePercent(std::uint8_t percent)
{
    deadzonePercent = std::clamp(percent, CursorDeadzoneMinimumPercent, CursorDeadzoneMaximumPercent);
}

void CursorStickMod::setWarpEnabled(bool enabled)
{
    warpEnabled = enabled;
}

float CursorStickMod::normalizeAxis(int value, int center, int minimum, int maximum)
{
    const int distance = value < center ? center - minimum : maximum - center;
    if (distance <= 0) return 0.0F;
    return std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
}

bool CursorStickMod::outsideDeadzone(float magnitude) const
{
    return magnitude > static_cast<float>(deadzonePercent) / 100.0F;
}

bool CursorStickMod::isStickActive(const LeftStickState &stick) const
{
    const float horizontal = normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    const float vertical = normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    return isEnabled && outsideDeadzone(std::sqrt(horizontal * horizontal + vertical * vertical));
}

void CursorStickMod::setCursorPosition(std::uint16_t x, std::uint16_t y)
{
    cursorX = std::clamp(static_cast<float>(x), 0.0F, 255.0F);
    cursorY = std::clamp(static_cast<float>(y), 0.0F, 191.0F);
    // Publish the next active intent even when rounding leaves it at the
    // same pixel, so the router can grant ownership after a screen press.
    stylusIntent.sourceActive = false;
    lastUpdateAt = std::chrono::steady_clock::now();
}

bool CursorStickMod::update(const LeftStickState& stick)
{
    didWrap = false;
    if (!isEnabled) return false;

    const auto now = std::chrono::steady_clock::now();
    float elapsedSeconds = 0.0F;
    if (lastUpdateAt != std::chrono::steady_clock::time_point{})
        elapsedSeconds = std::min(std::chrono::duration<float>(now - lastUpdateAt).count(), 0.050F);
    lastUpdateAt = now;

    float horizontal = normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    float vertical = normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    const float magnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    const bool active = outsideDeadzone(magnitude);
    if (magnitude > 1.0F)
    {
        horizontal /= magnitude;
        vertical /= magnitude;
    }

    const float speed = active ? speedLimit * std::min(magnitude, 1.0F) : 0.0F;
    const float nextX = cursorX + horizontal * speed * elapsedSeconds;
    if (warpEnabled && (nextX < 0.0F || nextX > 255.0F))
    {
        cursorX = nextX < 0.0F ? 255.0F : 0.0F;
        didWrap = true;
    }
    else cursorX = std::clamp(nextX, 0.0F, 255.0F);
    cursorY = std::clamp(cursorY + vertical * speed * elapsedSeconds, 0.0F, 191.0F);

    StylusIntent next = stylusIntent;
    next.source = source;
    next.pressed = false;
    next.sourceActive = active;
    next.x = static_cast<std::uint16_t>(std::lround(cursorX));
    next.y = static_cast<std::uint16_t>(std::lround(cursorY));
    if (sameStylusIntent(next, stylusIntent)) return false;
    stylusIntent = next;
    return true;
}

}
