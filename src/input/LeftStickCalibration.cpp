#include "input/LeftStickCalibration.h"

#include <algorithm>

namespace widemelon
{
namespace
{
    int scaleAxis(int value, int center, int minimum, int maximum, std::uint8_t negativeScale, std::uint8_t positiveScale)
    {
        const int scale = value < center ? negativeScale : positiveScale;
        const int scaled = center + (value - center) * scale / LeftStickScaleDefaultPercent;
        return std::clamp(scaled, minimum, maximum);
    }
}

    LeftStickState applyLeftStickCalibration(const LeftStickState &state, const LeftStickCalibration &calibration)
    {
        LeftStickState calibrated = state;
        calibrated.x = scaleAxis(state.x, state.xCenter, state.xMinimum, state.xMaximum, calibration.left, calibration.right);
        calibrated.y = scaleAxis(state.y, state.yCenter, state.yMinimum, state.yMaximum, calibration.up, calibration.down);
        return calibrated;
    }

}
