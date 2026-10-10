#include "input/MphAutoStickMod.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{
void MphAutoStickMod::setEnabled(bool value)
{
    enabled = value;
    reactivateAfterWrap = false;
    lastUpdateAt = {};
    releaseStartedAt = {};
    cursorX = Sm64TouchCenterX;
    cursorY = Sm64TouchCenterY;
    touch = {};
}

void MphAutoStickMod::setSpeed(std::uint16_t value)
{
    speed = std::clamp(value, MphManualSpeedMinimum, MphManualSpeedMaximum);
}

void MphAutoStickMod::setReleaseDelayMs(std::uint16_t milliseconds)
{
    releaseDelay = std::chrono::milliseconds(std::clamp(milliseconds, MphAutoReleaseDelayMinimumMs, MphAutoReleaseDelayMaximumMs));
}

float MphAutoStickMod::normalizeAxis(int value, int center, int minimum, int maximum)
{
    const int distance = value < center ? center - minimum : maximum - center;
    return distance > 0 ? std::clamp(static_cast<float>(value - center) / distance, -1.0F, 1.0F) : 0.0F;
}

bool MphAutoStickMod::update(const LeftStickState &stick)
{
    if (!enabled) return false;

    const auto now = std::chrono::steady_clock::now();
    const float elapsed = lastUpdateAt == std::chrono::steady_clock::time_point{}
        ? 0.0F
        : std::min(std::chrono::duration<float>(now - lastUpdateAt).count(), 0.05F);
    lastUpdateAt = now;

    const float horizontal = normalizeAxis(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    const float vertical = normalizeAxis(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    const bool outsideDeadzone = std::sqrt(horizontal * horizontal + vertical * vertical) > MphAutoDeadzone;
    Sm64TouchState next = touch;

    if (outsideDeadzone)
    {
        releaseStartedAt = {};
        const float nextX = cursorX + horizontal * speed * elapsed;
        cursorY = std::clamp(cursorY + vertical * speed * elapsed, 0.0F, 191.0F);
        const bool wrappedHorizontally = nextX < 0.0F || nextX >= 256.0F;
        cursorX = std::fmod(nextX, 256.0F);
        if (cursorX < 0.0F) cursorX += 256.0F;

        next.x = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorX)), 0, 255));
        next.y = static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(cursorY)), 0, 191));
        if (reactivateAfterWrap)
        {
            reactivateAfterWrap = false;
            next.active = true;
        }
        else if (wrappedHorizontally && touch.active)
        {
            reactivateAfterWrap = true;
            next.active = false;
        }
        else
        {
            next.active = true;
        }
    }
    else if (touch.active || reactivateAfterWrap)
    {
        reactivateAfterWrap = false;
        if (releaseStartedAt == std::chrono::steady_clock::time_point{}) releaseStartedAt = now;
        if (now - releaseStartedAt >= releaseDelay)
        {
            cursorX = Sm64TouchCenterX;
            cursorY = Sm64TouchCenterY;
            next.active = false;
            next.x = Sm64TouchCenterX;
            next.y = Sm64TouchCenterY;
            releaseStartedAt = {};
        }
        else
        {
            next.active = true;
        }
    }
    else
    {
        cursorX = Sm64TouchCenterX;
        cursorY = Sm64TouchCenterY;
        next.active = false;
        next.x = Sm64TouchCenterX;
        next.y = Sm64TouchCenterY;
    }

    if (next.active == touch.active && next.x == touch.x && next.y == touch.y) return false;
    touch = next;
    return true;
}
}
