#pragma once

#include "input/LeftStickCalibration.h"
#include "input/CursorSettings.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace widemelon
{
    inline constexpr std::uint8_t ScreenPressReleaseDelayFramesMinimum{1};
    inline constexpr std::uint8_t ScreenPressReleaseDelayFramesDefault{2};
    inline constexpr std::uint8_t ScreenPressReleaseDelayFramesMaximum{10};
    enum class PhysicalInput : std::uint8_t
    {
        A, B, X, Y, L, R, L2, R2,
        DpadLeft, DpadRight, DpadUp, DpadDown,
        Select, Start, L3, R3,
        Count,
    };

    enum class MappingOutput : std::uint8_t
    {
        Disabled,
        KeyPress,
        StylusPress,
        StylusHold,
        ScreenPress,
    };

    enum class DsButton : std::uint8_t
    {
        A, B, X, Y, L, R, DpadLeft, DpadRight, DpadUp, DpadDown, Select, Start,
    };

    struct InputMapping
    {
        MappingOutput output{MappingOutput::Disabled};
        DsButton button{DsButton::A};
        std::uint16_t screenX{127};
        std::uint16_t screenY{95};
        std::uint8_t screenPressReleaseDelayFrames{ScreenPressReleaseDelayFramesDefault};

        bool operator==(const InputMapping &other) const
        {
            return output == other.output && button == other.button
                && screenX == other.screenX && screenY == other.screenY
                && screenPressReleaseDelayFrames == other.screenPressReleaseDelayFrames;
        }
        bool operator!=(const InputMapping &other) const { return !(*this == other); }
    };

    constexpr std::size_t PhysicalInputCount = static_cast<std::size_t>(PhysicalInput::Count);
    std::string_view physicalInputName(PhysicalInput input);
    std::string_view physicalInputStorageName(PhysicalInput input);
    std::string_view mappingOutputName(MappingOutput output);
    std::string_view dsButtonName(DsButton button);

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
        std::uint16_t leftCursorSpeedLimit{300};
        std::uint16_t rightCursorSpeedLimit{300};
        std::uint8_t leftCursorDeadzonePercent{CursorDeadzoneDefaultPercent};
        std::uint8_t rightCursorDeadzonePercent{CursorDeadzoneDefaultPercent};
        std::array<InputMapping, PhysicalInputCount> mappings{};
    };

    class InputPresetStore
    {
    public:
        static constexpr std::string_view DefaultPresetName{"Default"};

        static InputPreset defaultPreset();
        static std::optional<InputPreset> load(std::string_view name);
        // Reads the old per-preset calibration only to migrate it to
        // tsps-stick.conf. New presets never contain these values.
        static std::optional<LeftStickCalibration> legacyLeftStickCalibration(std::string_view name);
        // One-time migration from the storage locations used before
        // frontend-specific paths were introduced.
        static std::optional<LeftStickCalibration> legacyActiveStickCalibration();
        static std::optional<InputPreset> loadActive();
        static std::optional<std::string> activeName();
        static std::vector<std::string> names();
        static bool save(const InputPreset &preset, bool makeActive, std::string &error);
        static bool rename(std::string_view previousName, std::string_view nextName, std::string &error);
        static bool setActive(std::string_view name, std::string &error);
        static bool remove(std::string_view name, std::string &error);
        static bool isValidName(std::string_view name);
    };
}
