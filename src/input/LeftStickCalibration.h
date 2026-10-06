#pragma once

#include "input/EvdevInput.h"

#include <cstdint>

namespace widemelon
{

    inline constexpr std::uint8_t LeftStickScaleMinimumPercent{50};
    inline constexpr std::uint8_t LeftStickScaleDefaultPercent{100};
    inline constexpr std::uint8_t LeftStickScaleMaximumPercent{150};

    struct LeftStickCalibration
    {
        std::uint8_t left{LeftStickScaleDefaultPercent};
        std::uint8_t right{LeftStickScaleDefaultPercent};
        std::uint8_t up{LeftStickScaleDefaultPercent};
        std::uint8_t down{LeftStickScaleDefaultPercent};
    };

    LeftStickState applyLeftStickCalibration(const LeftStickState &state, const LeftStickCalibration &calibration);

}
