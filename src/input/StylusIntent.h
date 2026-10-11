#pragma once

#include <cstdint>

namespace widemelon
{
    // Identifies the physical control currently proposing a touchscreen state.
    // Button-to-touch sources can be added here in the later mapping phase.
    enum class StylusSource : std::uint8_t
    {
        LeftStick,
        RightStick,
    };

    // Explains how a touch mode requested its press state. The router will use
    // this to apply the Toggle-over-Hold priority rule for R2.
    enum class StylusPressSemantics : std::uint8_t
    {
        Hold,
        Toggle,
        Automatic,
    };

    // A complete proposal from one touch-capable input mode. `sourceActive`
    // means that its stick is outside its deadzone; it is deliberately distinct
    // from `pressed`, as cooldowns can keep a stylus press alive after a stick
    // has returned to its deadzone.
    struct StylusIntent
    {
        StylusSource source{StylusSource::LeftStick};
        std::uint16_t x{127};
        std::uint16_t y{95};
        bool pressed{false};
        bool sourceActive{false};
        StylusPressSemantics pressSemantics{StylusPressSemantics::Automatic};
    };
}
