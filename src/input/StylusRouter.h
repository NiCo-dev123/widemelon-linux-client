#pragma once

#include "input/StylusIntent.h"

#include <cstdint>
#include <optional>

namespace widemelon
{
    struct StylusState
    {
        bool pressed{false};
        std::uint16_t x{127};
        std::uint16_t y{95};
    };

    // Owns the single touchscreen state sent to the DS. Call submit() only
    // when a source has a new intent: submission order deliberately defines
    // the winner when both sticks are being moved at once.
    class StylusRouter
    {
    public:
        // Starts one controller polling iteration. Toggle priority is scoped
        // to this frame so it resolves an R2 event without blocking later
        // intentional stick ownership hand-offs.
        void beginFrame();
        // Returns true when the owner or outgoing touchscreen state changed.
        bool submit(const StylusIntent &intent);
        void reset();

        const StylusState &state() const { return stylusState; }
        std::optional<StylusSource> owner() const { return currentOwner; }

    private:
        StylusState stylusState;
        std::optional<StylusSource> currentOwner;
        bool togglePressHasPriority{false};
    };
}
