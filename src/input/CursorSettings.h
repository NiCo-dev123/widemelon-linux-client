#pragma once

#include <cstdint>
#include <chrono>

namespace widemelon
{
inline constexpr std::uint8_t CursorDeadzoneMinimumPercent{0};
inline constexpr std::uint8_t CursorDeadzoneDefaultPercent{5};
inline constexpr std::uint8_t CursorDeadzoneMaximumPercent{25};
inline constexpr std::uint8_t CursorDeadzoneStepPercent{1};
// Approximately one 60 Hz DS frame, measured from sending the release.
inline constexpr std::chrono::milliseconds CursorWarpReleaseDuration{17};
}
