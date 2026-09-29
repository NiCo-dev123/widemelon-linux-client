#pragma once

#include <chrono>
#include <cstdint>

#include "input/EvdevInput.h"

namespace widemelon
{

inline constexpr float Sm64DpadDirectionalThreshold{0.10F};
inline constexpr float Sm64DpadBActivationThreshold{0.5F};
inline constexpr std::chrono::milliseconds Sm64DpadBReleaseDelay{33};

class Sm64DpadMod
{
public:
    void setEnabled(bool enabled);
    bool update(const LeftStickState& stick);
    std::uint16_t additionalButtons() const;

private:
    static float normalizedAxis(int value, int minimum, int maximum, int center);

    bool isEnabled = false;
    bool bHeld = false;
    bool releasePending = false;
    std::chrono::steady_clock::time_point releaseAt{};
};

}
