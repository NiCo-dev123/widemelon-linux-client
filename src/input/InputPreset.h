#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace widemelon
{
    // Button mappings deliberately do not belong here yet. Phase 5 adds them
    // after the physical-to-virtual input router exists.
    struct InputPreset
    {
        std::string name{"Config 1"};
        std::string leftStickMode{"D-pad"};
        std::string rightStickMode{"Disabled"};
        std::uint16_t mphCameraSpeed{100};
        std::uint16_t mphAutoReleaseDelayMs{400};
        std::uint8_t sm64AutoCenterHoldFrames{12};
        std::uint16_t sm64AutoReleaseDelayMs{1000};
        std::uint8_t sm64DpadDeadzonePercent{75};
        std::uint16_t cursorSpeedLimit{300};
        std::array<std::uint8_t, 4> leftStickCalibration{{100, 100, 100, 100}};
    };

    class InputPresetStore
    {
    public:
        static std::optional<InputPreset> load(std::string_view name);
        static std::optional<InputPreset> loadActive();
        static std::optional<std::string> activeName();
        static bool save(const InputPreset &preset, bool makeActive, std::string &error);
        static bool setActive(std::string_view name, std::string &error);
        static bool isValidName(std::string_view name);
    };
}
