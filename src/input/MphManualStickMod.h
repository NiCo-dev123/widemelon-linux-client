#pragma once

#include "input/EvdevInput.h"
#include "input/Sm64StickMod.h"

#include <chrono>
#include <cstdint>

namespace widemelon
{
    inline constexpr std::uint16_t MphManualSpeedMinimum{100};
    inline constexpr std::uint16_t MphManualSpeedDefault{100};
    inline constexpr std::uint16_t MphManualSpeedMaximum{1000};
    inline constexpr std::uint16_t MphManualSpeedStep{25};

    class MphManualStickMod
    {
    public:
        void setEnabled(bool enabled);
        void setSpeed(std::uint16_t pixelsPerSecond);
        bool update(const LeftStickState &stick, bool r2Pressed);
        const Sm64TouchState &touchState() const { return touch; }

    private:
        static float normalizeAxis(int value, int center, int minimum, int maximum);
        bool enabled{false};
        bool previousR2Pressed{false};
        std::uint16_t speed{MphManualSpeedDefault};
        float cursorX{Sm64TouchCenterX};
        float cursorY{Sm64TouchCenterY};
        std::chrono::steady_clock::time_point lastUpdateAt{};
        Sm64TouchState touch;
    };
}
