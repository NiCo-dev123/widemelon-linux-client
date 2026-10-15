#include "input/StylusRouter.h"

namespace widemelon
{
bool StylusRouter::submit(const StylusIntent &intent)
{
    const bool releaseForScreenPress = screenPressPhase == ScreenPressPhase::Releasing;
    const bool pressForScreenPress = screenPressPhase == ScreenPressPhase::Pressing;
    const StylusState nextState{releaseForScreenPress ? false : pressForScreenPress || intent.pressed || mappedPressed,
        pressForScreenPress ? screenPressPosition.x : mappedPressed && mappedPosition ? mappedPosition->x : intent.x,
        pressForScreenPress ? screenPressPosition.y : mappedPressed && mappedPosition ? mappedPosition->y : intent.y};

    // Any active stick claims ownership immediately. This is intentional:
    // after a left-stick cooldown begins, a newly moved right stick must not
    // wait for that cooldown to finish before taking control.
    if (intent.sourceActive)
    {
        const bool ownerChanged = currentOwner != intent.source;
        const bool stateChanged = stylusState.pressed != nextState.pressed
            || stylusState.x != nextState.x || stylusState.y != nextState.y;
        currentOwner = intent.source;
        stylusState = nextState;
        return ownerChanged || stateChanged;
    }

    // A source outside ownership cannot overwrite the stylus merely because
    // it is publishing its inactive state. The owner may still be holding a
    // touch during its mode-specific release cooldown.
    if (!currentOwner || *currentOwner != intent.source) return false;

    const bool stateChanged = stylusState.pressed != nextState.pressed
        || stylusState.x != nextState.x || stylusState.y != nextState.y;
    stylusState = nextState;
    if (!intent.pressed) currentOwner.reset();
    return stateChanged;
}

bool StylusRouter::setMappedPressed(bool pressed, bool hasAbsolutePosition, std::uint16_t x, std::uint16_t y)
{
    const std::optional<StylusState> nextPosition = pressed && hasAbsolutePosition
        ? std::optional<StylusState>(StylusState{true, x, y}) : std::nullopt;
    const bool samePosition = mappedPosition.has_value() == nextPosition.has_value()
        && (!mappedPosition || (mappedPosition->x == nextPosition->x && mappedPosition->y == nextPosition->y));
    if (mappedPressed == pressed && samePosition) return false;
    mappedPressed = pressed;
    mappedPosition = nextPosition;
    const bool releaseForScreenPress = screenPressPhase == ScreenPressPhase::Releasing;
    const bool pressForScreenPress = screenPressPhase == ScreenPressPhase::Pressing;
    const StylusState nextState{releaseForScreenPress ? false : pressForScreenPress || mappedPressed,
        pressForScreenPress ? screenPressPosition.x : mappedPosition ? mappedPosition->x : stylusState.x,
        pressForScreenPress ? screenPressPosition.y : mappedPosition ? mappedPosition->y : stylusState.y};
    const bool changed = stylusState.pressed != nextState.pressed || stylusState.x != nextState.x || stylusState.y != nextState.y;
    stylusState = nextState;
    return changed;
}

bool StylusRouter::setScreenPress(bool held, std::uint16_t x, std::uint16_t y, std::uint8_t releaseDelayFrames)
{
    if (!held)
    {
        if (screenPressPhase == ScreenPressPhase::None) return false;
        screenPressPhase = ScreenPressPhase::None;
        screenPressFramesRemaining = 0;
        const StylusState released{mappedPressed, mappedPosition ? mappedPosition->x : stylusState.x,
            mappedPosition ? mappedPosition->y : stylusState.y};
        const bool changed = stylusState.pressed != released.pressed || stylusState.x != released.x || stylusState.y != released.y;
        stylusState = released;
        return changed;
    }

    screenPressPosition = {true, x, y};
    if (screenPressPhase == ScreenPressPhase::Pressing)
    {
        const StylusState pressed{true, x, y};
        const bool changed = stylusState.x != pressed.x || stylusState.y != pressed.y;
        stylusState = pressed;
        return changed;
    }
    if (screenPressPhase == ScreenPressPhase::Releasing) return false;
    if (stylusState.pressed)
    {
        screenPressPhase = ScreenPressPhase::Releasing;
        screenPressFramesRemaining = releaseDelayFrames;
        const StylusState released{false, stylusState.x, stylusState.y};
        const bool changed = stylusState.pressed != released.pressed;
        stylusState = released;
        return changed;
    }

    screenPressPhase = ScreenPressPhase::Pressing;
    screenPressFramesRemaining = 0;
    const StylusState pressed{true, x, y};
    const bool changed = stylusState.pressed != pressed.pressed || stylusState.x != pressed.x || stylusState.y != pressed.y;
    stylusState = pressed;
    return changed;
}

bool StylusRouter::advanceScreenPress()
{
    if (screenPressPhase == ScreenPressPhase::None) return false;
    if (screenPressFramesRemaining > 0) --screenPressFramesRemaining;
    if (screenPressFramesRemaining > 0) return false;

    if (screenPressPhase == ScreenPressPhase::Releasing)
    {
        screenPressPhase = ScreenPressPhase::Pressing;
        screenPressFramesRemaining = 0;
        const StylusState pressed{true, screenPressPosition.x, screenPressPosition.y};
        const bool changed = stylusState.pressed != pressed.pressed || stylusState.x != pressed.x || stylusState.y != pressed.y;
        stylusState = pressed;
        return changed;
    }

    return false;
}

void StylusRouter::reset()
{
    stylusState = {};
    currentOwner.reset();
    mappedPressed = false;
    mappedPosition.reset();
    screenPressPhase = ScreenPressPhase::None;
    screenPressFramesRemaining = 0;
}
}
