#pragma once

#include "input/EvdevInput.h"

#include <cstdint>

namespace widemelon
{
    // Converts either analog stick into the virtual DS D-pad. It intentionally
    // does not add any automatic face-button press.
    class StickDpadMod
    {
    public:
        void setEnabled(bool enabled);
        bool update(const LeftStickState &stick);
        std::uint16_t additionalButtons() const { return buttons; }

    private:
        bool isEnabled{false};
        std::uint16_t buttons{0};
    };
}
