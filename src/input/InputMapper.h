#pragma once

#include "input/InputPreset.h"

#include <array>
#include <cstdint>

namespace widemelon
{
    class EvdevInput;

    // Converts physical TSPS controls to virtual DS buttons and stylus contact.
    class InputMapper
    {
    public:
        void setMappings(const std::array<InputMapping, PhysicalInputCount> &value);
        bool update(const EvdevInput &input);

        std::uint16_t buttonMask() const { return virtualButtonMask; }
        bool touchPressed() const { return stylusPressed; }
        bool screenPressActive() const { return screenPressHeld; }
        std::uint16_t screenPressX() const { return screenPressXValue; }
        std::uint16_t screenPressY() const { return screenPressYValue; }
        std::uint8_t screenPressReleaseDelayFrames() const { return screenPressReleaseDelayFramesValue; }
        bool hasStylusMapping() const;

    private:
        static bool isPressed(const EvdevInput &input, PhysicalInput source);
        static std::uint16_t dsButtonMask(DsButton button);

        std::array<InputMapping, PhysicalInputCount> mappings{};
        std::array<bool, PhysicalInputCount> previousPressed{};
        std::array<bool, PhysicalInputCount> togglePressed{};
        std::uint16_t virtualButtonMask{0};
        std::uint16_t toggledButtonMask{0};
        bool stylusPressed{false};
        bool screenPressHeld{false};
        std::uint16_t screenPressXValue{127};
        std::uint16_t screenPressYValue{95};
        std::uint8_t screenPressReleaseDelayFramesValue{ScreenPressReleaseDelayFramesDefault};
    };
}
