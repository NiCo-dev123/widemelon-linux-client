#include "input/MphManualStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{
void MphManualStickMod::setEnabled(bool value)
{
    enabled = value; lastUpdateAt = {};
    cursorX = Sm64TouchCenterX; cursorY = Sm64TouchCenterY;
    stylusIntent = {StylusSource::RightStick, Sm64TouchCenterX, Sm64TouchCenterY, false, false, StylusPressSemantics::Toggle};
}
void MphManualStickMod::setSpeed(std::uint16_t value) { speed = std::clamp(value, MphManualSpeedMinimum, MphManualSpeedMaximum); }
float MphManualStickMod::normalizeAxis(int value, int center, int minimum, int maximum)
{
    const int distance = value < center ? center - minimum : maximum - center;
    return distance > 0 ? std::clamp(static_cast<float>(value - center) / distance, -1.0F, 1.0F) : 0.0F;
}
bool MphManualStickMod::update(const LeftStickState &stick)
{
    if (!enabled) return false;
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = lastUpdateAt == std::chrono::steady_clock::time_point{} ? 0.0F : std::min(std::chrono::duration<float>(now - lastUpdateAt).count(), 0.05F);
    lastUpdateAt = now;
    const float nextX = cursorX + normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum) * speed * elapsed;
    cursorY += normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum) * speed * elapsed;
    // MPH needs continuous horizontal camera motion, so X wraps at the
    // touchscreen edges. Lift for one snapshot before restoring contact on
    // the other side, otherwise the game treats the wrap as a huge drag.
    cursorX = std::fmod(nextX, 256.0F);
    if (cursorX < 0.0F) cursorX += 256.0F;
    // Vertical camera movement must stop at the screen edges.
    cursorY = std::clamp(cursorY, 0.0F, 191.0F);
    const float horizontal = normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    const float vertical = normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    StylusIntent next = stylusIntent;
    next.sourceActive = std::sqrt(horizontal * horizontal + vertical * vertical) > 0.05F;
    next.x = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorX)), 0, 255));
    next.y = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorY)), 0, 191));
    next.pressed = false;
    if (sameStylusIntent(next, stylusIntent)) return false;
    stylusIntent = next; return true;
}
}
