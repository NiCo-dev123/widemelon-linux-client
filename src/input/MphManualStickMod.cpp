#include "input/MphManualStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{
void MphManualStickMod::setEnabled(bool value)
{
    enabled = value; previousR2Pressed = false; lastUpdateAt = {};
    cursorX = Sm64TouchCenterX; cursorY = Sm64TouchCenterY; touch = {};
}
void MphManualStickMod::setSpeed(std::uint16_t value) { speed = std::clamp(value, MphManualSpeedMinimum, MphManualSpeedMaximum); }
float MphManualStickMod::normalizeAxis(int value, int center, int minimum, int maximum)
{
    const int distance = value < center ? center - minimum : maximum - center;
    return distance > 0 ? std::clamp(static_cast<float>(value - center) / distance, -1.0F, 1.0F) : 0.0F;
}
bool MphManualStickMod::update(const LeftStickState &stick, bool r2Pressed)
{
    if (!enabled) return false;
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = lastUpdateAt == std::chrono::steady_clock::time_point{} ? 0.0F : std::min(std::chrono::duration<float>(now - lastUpdateAt).count(), 0.05F);
    lastUpdateAt = now;
    const bool toggled = r2Pressed && !previousR2Pressed;
    if (toggled) touch.active = !touch.active;
    previousR2Pressed = r2Pressed;
    cursorX += normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum) * speed * elapsed;
    cursorY += normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum) * speed * elapsed;
    // MPH needs continuous horizontal camera motion, so X wraps at the
    // touchscreen edges. fmod avoids a transient invalid coordinate when a
    // large input delta crosses an edge.
    cursorX = std::fmod(cursorX, 256.0F);
    if (cursorX < 0.0F) cursorX += 256.0F;
    // Vertical camera movement must stop at the screen edges.
    cursorY = std::clamp(cursorY, 0.0F, 191.0F);
    Sm64TouchState next = touch;
    next.x = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorX)), 0, 255));
    next.y = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorY)), 0, 191));
    if (!toggled && next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next; return true;
}
}
