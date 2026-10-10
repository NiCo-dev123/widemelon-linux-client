#pragma once

#include "input/EvdevInput.h"
#include "input/MphManualStickMod.h"
#include "input/Sm64StickMod.h"

#include <chrono>
#include <cstdint>

namespace widemelon
{
    inline constexpr float MphAutoDeadzone{0.12F};
    inline constexpr std::uint16_t MphAutoReleaseDelayMinimumMs{100};
    inline constexpr std::uint16_t MphAutoReleaseDelayMs{400};
    inline constexpr std::uint16_t MphAutoReleaseDelayMaximumMs{1000};
    inline constexpr std::uint16_t MphAutoReleaseDelayStepMs{25};

    class MphAutoStickMod
    {
    public:
        void setEnabled(bool enabled);
        void setSpeed(std::uint16_t pixelsPerSecond);
        void setReleaseDelayMs(std::uint16_t milliseconds);
        bool update(const LeftStickState &stick);
        const Sm64TouchState &touchState() const { return touch; }

    private:
        static float normalizeAxis(int value, int center, int minimum, int maximum);

        bool enabled{false};
        bool reactivateAfterWrap{false};
        std::uint16_t speed{MphManualSpeedDefault};
        float cursorX{Sm64TouchCenterX};
        float cursorY{Sm64TouchCenterY};
        std::chrono::steady_clock::time_point lastUpdateAt{};
        std::chrono::steady_clock::time_point releaseStartedAt{};
        std::chrono::milliseconds releaseDelay{MphAutoReleaseDelayMs};
        Sm64TouchState touch;
    };
}
