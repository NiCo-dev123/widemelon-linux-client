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
        bool screenPressRequested() const { return requestScreenPress; }
        std::uint16_t screenPressX() const { return requestedScreenX; }
        std::uint16_t screenPressY() const { return requestedScreenY; }
        std::uint8_t screenPressReleaseDelayFrames() const { return requestedScreenPressReleaseDelayFrames; }
        bool hasStylusMapping() const;

    private:
        static bool isPressed(const EvdevInput &input, PhysicalInput source);
        static std::uint16_t dsButtonMask(DsButton button);

        std::array<InputMapping, PhysicalInputCount> mappings{};
        std::array<bool, PhysicalInputCount> previousPressed{};
        std::array<bool, PhysicalInputCount> togglePressed{};
        std::array<bool, PhysicalInputCount> singlePressPending{};
        std::uint16_t virtualButtonMask{0};
        bool stylusPressed{false};
        bool requestScreenPress{false};
        std::uint16_t requestedScreenX{127};
        std::uint16_t requestedScreenY{95};
        std::uint8_t requestedScreenPressReleaseDelayFrames{ScreenPressReleaseDelayFramesDefault};
    };
}
