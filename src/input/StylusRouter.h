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
        // Returns true when the owner or outgoing touchscreen state changed.
        bool submit(const StylusIntent &intent);
        // Button mappings control contact independently from the source that
        // owns the current stylus position.
        bool setMappedPressed(bool pressed);
        void reset();

        const StylusState &state() const { return stylusState; }
        std::optional<StylusSource> owner() const { return currentOwner; }

    private:
        StylusState stylusState;
        std::optional<StylusSource> currentOwner;
        bool mappedPressed{false};
    };
}
