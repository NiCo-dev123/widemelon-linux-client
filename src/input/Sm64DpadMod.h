#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr float Sm64DpadDirectionalThreshold{0.10F};
inline constexpr std::uint8_t Sm64DpadDeadzoneMinimumPercent{25};
inline constexpr std::uint8_t Sm64DpadDeadzoneDefaultPercent{75};
inline constexpr std::uint8_t Sm64DpadDeadzoneMaximumPercent{95};
inline constexpr std::chrono::milliseconds Sm64DpadBReleaseDelay{33};

class Sm64DpadMod
{
public:
    void setEnabled(bool enabled);
    void setDeadzonePercent(std::uint8_t percent);
    bool update(const LeftStickState& stick);
    std::uint16_t additionalButtons() const;

private:
    static float normalizedAxis(int value, int minimum, int maximum, int center);

    bool isEnabled = false;
    float yActivationThreshold = static_cast<float>(Sm64DpadDeadzoneDefaultPercent) / 100.0F;
    bool bHeld = false;
    bool releasePending = false;
    std::chrono::steady_clock::time_point releaseAt{};
};

}
