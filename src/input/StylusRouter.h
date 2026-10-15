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
        bool setMappedPressed(bool pressed, bool hasAbsolutePosition = false, std::uint16_t x = 127, std::uint16_t y = 95);
        // A Screen press holds the stylus at absolute coordinates while its
        // mapped physical input is held. It safely repositions an active
        // stylus by releasing it first, waiting, then pressing again.
        bool setScreenPress(bool held, std::uint16_t x = 127, std::uint16_t y = 95, std::uint8_t releaseDelayFrames = 0);
        bool advanceScreenPress();
        void reset();

        const StylusState &state() const { return stylusState; }
        std::optional<StylusSource> owner() const { return currentOwner; }

    private:
        enum class ScreenPressPhase : std::uint8_t { None, Releasing, Pressing };

        StylusState stylusState;
        std::optional<StylusSource> currentOwner;
        bool mappedPressed{false};
        std::optional<StylusState> mappedPosition;
        ScreenPressPhase screenPressPhase{ScreenPressPhase::None};
        StylusState screenPressPosition;
        std::uint8_t screenPressFramesRemaining{0};
    };
}
