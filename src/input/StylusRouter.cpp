#include "input/StylusRouter.h"

namespace widemelon
{
void StylusRouter::beginFrame()
{
    togglePressHasPriority = false;
}

bool StylusRouter::submit(const StylusIntent &intent)
{
    const StylusState nextState{intent.pressed, intent.x, intent.y};

    // Any active stick claims ownership immediately. This is intentional:
    // after a left-stick cooldown begins, a newly moved right stick must not
    // wait for that cooldown to finish before taking control.
    if (intent.sourceActive)
    {
        // Toggle is the unambiguous interpretation of an R2 edge when a
        // manual mode and a Cursor Hold mode are configured together.
        if (intent.pressSemantics == StylusPressSemantics::Hold && togglePressHasPriority)
            return false;

        const bool ownerChanged = currentOwner != intent.source;
        const bool stateChanged = stylusState.pressed != nextState.pressed
            || stylusState.x != nextState.x || stylusState.y != nextState.y;
        currentOwner = intent.source;
        stylusState = nextState;
        if (intent.pressSemantics == StylusPressSemantics::Toggle && intent.pressStateChanged)
            togglePressHasPriority = true;
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

void StylusRouter::reset()
{
    stylusState = {};
    currentOwner.reset();
    togglePressHasPriority = false;
}
}
