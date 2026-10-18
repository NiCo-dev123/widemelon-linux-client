#include "input/InputPreset.h"
#include "input/configs/Default.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace widemelon
{
namespace
{
constexpr std::string_view ActivePresetFile{"input-presets.conf"};
constexpr std::string_view ActivePresetKey{"active-preset"};

// SpruceOS keeps application data in Saves. Other front-ends currently use
// the shared .userdata location. Select one root up front: never probe/write
// several locations, otherwise a preset can appear to vanish between runs.
std::filesystem::path storageRoot()
{
    std::error_code error;
    if (std::filesystem::exists("/mnt/SDCARD/spruce", error) && !error)
        return "/mnt/SDCARD/Saves/WideMelonClient";
    return "/mnt/SDCARD/.userdata/shared/WideMelonClient";
}

std::filesystem::path presetPath(const std::filesystem::path &root, std::string_view name)
{
    return root / "input-presets" / (std::string(name) + ".json");
}

bool writeAtomically(const std::filesystem::path &path, std::string_view content, std::string &error)
{
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError)
    {
        error = "Cannot create preset directory";
        return false;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
        {
            error = "Cannot write preset file";
            return false;
        }
        output << content;
        if (!output)
        {
            error = "Cannot write preset file";
            return false;
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);
    if (!renameError) return true;
    std::filesystem::remove(temporary, renameError);
    error = "Cannot save preset file";
    return false;
}

std::optional<std::string> readFile(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::optional<std::string> jsonString(std::string_view document, std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyAt = document.find(needle);
    if (keyAt == std::string_view::npos) return std::nullopt;
    const std::size_t colon = document.find(':', keyAt + needle.size());
    if (colon == std::string_view::npos) return std::nullopt;
    const std::size_t firstQuote = document.find('"', colon + 1);
    if (firstQuote == std::string_view::npos) return std::nullopt;
    const std::size_t lastQuote = document.find('"', firstQuote + 1);
    if (lastQuote == std::string_view::npos) return std::nullopt;
    return std::string(document.substr(firstQuote + 1, lastQuote - firstQuote - 1));
}

std::optional<int> jsonInt(std::string_view document, std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyAt = document.find(needle);
    if (keyAt == std::string_view::npos) return std::nullopt;
    const std::size_t colon = document.find(':', keyAt + needle.size());
    if (colon == std::string_view::npos) return std::nullopt;
    const std::size_t valueAt = document.find_first_of("-0123456789", colon + 1);
    if (valueAt == std::string_view::npos) return std::nullopt;
    const std::size_t end = document.find_first_not_of("-0123456789", valueAt);
    int value = 0;
    const auto result = std::from_chars(document.data() + valueAt, document.data() + (end == std::string_view::npos ? document.size() : end), value);
    if (result.ec != std::errc{}) return std::nullopt;
    return value;
}

int clampValue(const std::optional<int> &value, int defaultValue, int minimum, int maximum)
{
    return value ? std::clamp(*value, minimum, maximum) : defaultValue;
}

MappingOutput mappingOutputFromName(const std::optional<std::string> &value)
{
    if (value && *value == "key-press") return MappingOutput::KeyPress;
    if (value && *value == "stylus-press") return MappingOutput::StylusPress;
    if (value && *value == "stylus-hold") return MappingOutput::StylusHold;
    if (value && *value == "screen-press") return MappingOutput::ScreenPress;
    return MappingOutput::Disabled;
}

DsButton dsButtonFromName(const std::optional<std::string> &value)
{
    for (std::size_t index = 0; index <= static_cast<std::size_t>(DsButton::Start); ++index)
    {
        const DsButton button = static_cast<DsButton>(index);
        if (value && *value == dsButtonName(button)) return button;
    }
    return DsButton::A;
}

std::string json(const InputPreset &preset)
{
    std::string result = "{\n"
           "  \"name\": \"" + preset.name + "\",\n"
           "  \"left_stick_mode\": \"" + preset.leftStickMode + "\",\n"
           "  \"right_stick_mode\": \"" + preset.rightStickMode + "\",\n"
           "  \"mph_camera_speed\": " + std::to_string(preset.mphCameraSpeed) + ",\n"
           "  \"mph_auto_release_delay_ms\": " + std::to_string(preset.mphAutoReleaseDelayMs) + ",\n"
           "  \"sm64_auto_center_hold_frames\": " + std::to_string(preset.sm64AutoCenterHoldFrames) + ",\n"
           "  \"sm64_auto_release_delay_ms\": " + std::to_string(preset.sm64AutoReleaseDelayMs) + ",\n"
           "  \"sm64_dpad_deadzone_percent\": " + std::to_string(preset.sm64DpadDeadzonePercent) + ",\n"
           "  \"left_cursor_speed_limit\": " + std::to_string(preset.leftCursorSpeedLimit) + ",\n"
           "  \"right_cursor_speed_limit\": " + std::to_string(preset.rightCursorSpeedLimit) + ",\n"
           "  \"left_cursor_deadzone_percent\": " + std::to_string(preset.leftCursorDeadzonePercent) + ",\n"
           "  \"right_cursor_deadzone_percent\": " + std::to_string(preset.rightCursorDeadzonePercent) + ",\n";
    for (std::size_t index = 0; index < preset.mappings.size(); ++index)
    {
        const PhysicalInput input = static_cast<PhysicalInput>(index);
        const InputMapping &mapping = preset.mappings[index];
        const std::string key(physicalInputStorageName(input));
        result += "  \"map_" + key + "_output\": \"" + std::string(mappingOutputName(mapping.output)) + "\",\n";
        result += "  \"map_" + key + "_button\": \"" + std::string(dsButtonName(mapping.button)) + "\",\n";
        result += "  \"map_" + key + "_screen_x\": " + std::to_string(mapping.screenX) + ",\n";
        result += "  \"map_" + key + "_screen_y\": " + std::to_string(mapping.screenY) + ",\n";
        result += "  \"map_" + key + "_screen_press_release_delay_frames\": " + std::to_string(mapping.screenPressReleaseDelayFrames);
        result += index + 1 == preset.mappings.size() ? "\n" : ",\n";
    }
    return result + "}\n";
}

std::optional<InputPreset> parse(std::string_view document)
{
    const std::optional<std::string> name = jsonString(document, "name");
    if (!name || !InputPresetStore::isValidName(*name)) return std::nullopt;

    InputPreset preset = InputPresetStore::defaultPreset();
    preset.name = *name;
    preset.leftStickMode = normalizeStickMode(jsonString(document, "left_stick_mode").value_or(preset.leftStickMode));
    preset.rightStickMode = normalizeStickMode(jsonString(document, "right_stick_mode").value_or(preset.rightStickMode));
    preset.mphCameraSpeed = static_cast<std::uint16_t>(clampValue(jsonInt(document, "mph_camera_speed"), 100, 50, 500));
    preset.mphAutoReleaseDelayMs = static_cast<std::uint16_t>(clampValue(jsonInt(document, "mph_auto_release_delay_ms"), 400, 100, 1000));
    preset.sm64AutoCenterHoldFrames = static_cast<std::uint8_t>(clampValue(jsonInt(document, "sm64_auto_center_hold_frames"), 12, 2, 20));
    preset.sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(clampValue(jsonInt(document, "sm64_auto_release_delay_ms"), 1000, 0, 2000));
    preset.sm64DpadDeadzonePercent = static_cast<std::uint8_t>(clampValue(jsonInt(document, "sm64_dpad_deadzone_percent"), 75, 25, 95));
    // `cursor_speed_limit` was shared by both sticks in older presets.
    const int legacyCursorSpeed = clampValue(jsonInt(document, "cursor_speed_limit"), 300, 100, 500);
    preset.leftCursorSpeedLimit = static_cast<std::uint16_t>(clampValue(jsonInt(document, "left_cursor_speed_limit"), legacyCursorSpeed, 100, 500));
    preset.rightCursorSpeedLimit = static_cast<std::uint16_t>(clampValue(jsonInt(document, "right_cursor_speed_limit"), legacyCursorSpeed, 100, 500));
    preset.leftCursorDeadzonePercent = static_cast<std::uint8_t>(clampValue(jsonInt(document, "left_cursor_deadzone_percent"), CursorDeadzoneDefaultPercent, CursorDeadzoneMinimumPercent, CursorDeadzoneMaximumPercent));
    preset.rightCursorDeadzonePercent = static_cast<std::uint8_t>(clampValue(jsonInt(document, "right_cursor_deadzone_percent"), CursorDeadzoneDefaultPercent, CursorDeadzoneMinimumPercent, CursorDeadzoneMaximumPercent));
    for (std::size_t index = 0; index < preset.mappings.size(); ++index)
    {
        const PhysicalInput input = static_cast<PhysicalInput>(index);
        const std::string key(physicalInputStorageName(input));
        const std::optional<std::string> output = jsonString(document, "map_" + key + "_output");
        if (output) preset.mappings[index].output = mappingOutputFromName(output);
        preset.mappings[index].button = dsButtonFromName(jsonString(document, "map_" + key + "_button"));
        preset.mappings[index].screenX = static_cast<std::uint16_t>(clampValue(jsonInt(document, "map_" + key + "_screen_x"), 127, 0, 255));
        preset.mappings[index].screenY = static_cast<std::uint16_t>(clampValue(jsonInt(document, "map_" + key + "_screen_y"), 95, 0, 191));
        preset.mappings[index].screenPressReleaseDelayFrames = static_cast<std::uint8_t>(clampValue(
            jsonInt(document, "map_" + key + "_screen_press_release_delay_frames"), ScreenPressReleaseDelayFramesDefault,
            ScreenPressReleaseDelayFramesMinimum, ScreenPressReleaseDelayFramesMaximum));
    }
    // Compatibility with the L2/R2 stylus POC stored by older presets.
    const auto legacyStylusOutput = [&](std::string_view key)
    {
        const std::optional<std::string> legacy = jsonString(document, key);
        return legacy && *legacy == "press" ? MappingOutput::StylusPress
             : legacy && *legacy == "hold" ? MappingOutput::StylusHold : MappingOutput::Disabled;
    };
    const std::size_t l2 = static_cast<std::size_t>(PhysicalInput::L2);
    const std::size_t r2 = static_cast<std::size_t>(PhysicalInput::R2);
    if (!jsonString(document, "map_l2_output")) preset.mappings[l2].output = legacyStylusOutput("l2_stylus_mapping");
    if (!jsonString(document, "map_r2_output")) preset.mappings[r2].output = legacyStylusOutput("r2_stylus_mapping");
    return preset;
}

std::optional<LeftStickCalibration> parseLegacyCalibration(std::string_view document)
{
    if (document.find("\"left_stick_calibration\"") == std::string_view::npos) return std::nullopt;
    return LeftStickCalibration{
        static_cast<std::uint8_t>(clampValue(jsonInt(document, "left"), 100, 50, 150)),
        static_cast<std::uint8_t>(clampValue(jsonInt(document, "right"), 100, 50, 150)),
        static_cast<std::uint8_t>(clampValue(jsonInt(document, "up"), 100, 50, 150)),
        static_cast<std::uint8_t>(clampValue(jsonInt(document, "down"), 100, 50, 150))};
}

std::optional<LeftStickCalibration> legacyCalibrationInRoot(const std::filesystem::path &root)
{
    const std::optional<std::string> active = readFile(root / ActivePresetFile);
    const std::string prefix = std::string(ActivePresetKey) + "=";
    if (!active || active->compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    const std::string name = active->substr(prefix.size(), active->find_first_of("\r\n", prefix.size()) - prefix.size());
    if (!InputPresetStore::isValidName(name)) return std::nullopt;
    const std::optional<std::string> preset = readFile(presetPath(root, name));
    return preset ? parseLegacyCalibration(*preset) : std::nullopt;
}
}

std::string_view physicalInputName(PhysicalInput input)
{
    static constexpr std::array<std::string_view, PhysicalInputCount> Names{
        "A", "B", "X", "Y", "L", "R", "L2", "R2", "D-pad left", "D-pad right", "D-pad up", "D-pad down", "Select", "Start", "L3", "R3"};
    return Names[static_cast<std::size_t>(input)];
}

std::string_view physicalInputStorageName(PhysicalInput input)
{
    static constexpr std::array<std::string_view, PhysicalInputCount> Names{
        "a", "b", "x", "y", "l", "r", "l2", "r2", "dpad_left", "dpad_right", "dpad_up", "dpad_down", "select", "start", "l3", "r3"};
    return Names[static_cast<std::size_t>(input)];
}

std::string_view mappingOutputName(MappingOutput output)
{
    switch (output)
    {
    case MappingOutput::KeyPress: return "key-press";
    case MappingOutput::StylusPress: return "stylus-press";
    case MappingOutput::StylusHold: return "stylus-hold";
    case MappingOutput::ScreenPress: return "screen-press";
    default: return "disabled";
    }
}

std::string_view dsButtonName(DsButton button)
{
    static constexpr std::array<std::string_view, 12> Names{
        "A", "B", "X", "Y", "L", "R", "D-pad left", "D-pad right", "D-pad up", "D-pad down", "Select", "Start"};
    return Names[static_cast<std::size_t>(button)];
}

bool InputPresetStore::isValidName(std::string_view name)
{
    if (name.empty() || name.size() > 48) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char character)
    {
        return std::isalnum(character) || character == ' ' || character == '-' || character == '_';
    });
}

InputPreset InputPresetStore::defaultPreset()
{
    return input_configs::defaultInputPreset();
}

std::optional<InputPreset> InputPresetStore::load(std::string_view name)
{
    if (!isValidName(name)) return std::nullopt;
    const std::optional<std::string> document = readFile(presetPath(storageRoot(), name));
    if (document)
    {
        const std::optional<InputPreset> preset = parse(*document);
        if (preset && preset->name == name) return preset;
        if (name == DefaultPresetName) return defaultPreset();
        return std::nullopt;
    }
    return name == DefaultPresetName ? std::optional<InputPreset>(defaultPreset()) : std::nullopt;
}

std::optional<LeftStickCalibration> InputPresetStore::legacyLeftStickCalibration(std::string_view name)
{
    if (!isValidName(name)) return std::nullopt;
    const std::optional<std::string> document = readFile(presetPath(storageRoot(), name));
    return document ? parseLegacyCalibration(*document) : std::nullopt;
}

std::optional<LeftStickCalibration> InputPresetStore::legacyActiveStickCalibration()
{
    const std::array<std::filesystem::path, 4> roots{
        storageRoot(),
        "/mnt/SDCARD/.userdata/shared/WideMelonClient",
        "/mnt/SDCARD/.userdata/tg5050/WideMelonClient",
        "/mnt/sdcard/Saves/WideMelonClient"};
    for (const std::filesystem::path &root : roots)
        if (const std::optional<LeftStickCalibration> calibration = legacyCalibrationInRoot(root)) return calibration;
    return std::nullopt;
}

std::optional<std::string> InputPresetStore::activeName()
{
    const std::string prefix = std::string(ActivePresetKey) + "=";
    const std::optional<std::string> document = readFile(storageRoot() / ActivePresetFile);
    if (!document || document->compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    const std::string name = document->substr(prefix.size());
    const std::size_t newline = name.find_first_of("\r\n");
    const std::string trimmed = name.substr(0, newline);
    if (isValidName(trimmed)) return trimmed;
    return std::nullopt;
}

std::vector<std::string> InputPresetStore::names()
{
    std::vector<std::string> result{std::string(DefaultPresetName)};
    std::error_code error;
    const std::filesystem::path directory = storageRoot() / "input-presets";
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error || !entry.is_regular_file(error) || entry.path().extension() != ".json") continue;
        const std::string name = entry.path().stem().string();
        if (isValidName(name)) result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::optional<InputPreset> InputPresetStore::loadActive()
{
    const std::optional<std::string> name = activeName();
    if (!name) return std::nullopt;
    if (const std::optional<InputPreset> preset = load(*name)) return preset;

    InputPreset fallback = defaultPreset();
    std::string error;
    save(fallback, true, error);
    return fallback;
}

bool InputPresetStore::save(const InputPreset &preset, bool makeActive, std::string &error)
{
    if (!isValidName(preset.name))
    {
        error = "Invalid preset name";
        return false;
    }
    const std::filesystem::path root = storageRoot();
    if (!writeAtomically(presetPath(root, preset.name), json(preset), error)) return false;
    if (!makeActive) return true;
    return writeAtomically(root / ActivePresetFile, std::string(ActivePresetKey) + "=" + preset.name + "\n", error);
}

bool InputPresetStore::rename(std::string_view previousName, std::string_view nextName, std::string &error)
{
    if (previousName == DefaultPresetName)
    {
        error = "The Default preset cannot be renamed";
        return false;
    }
    if (!isValidName(nextName))
    {
        error = "Invalid preset name";
        return false;
    }
    if (previousName == nextName) return true;
    if (load(nextName))
    {
        error = "A preset already uses this name";
        return false;
    }
    std::optional<InputPreset> preset = load(previousName);
    if (!preset)
    {
        error = "Preset does not exist";
        return false;
    }

    const bool wasActive = activeName() == previousName;
    preset->name = std::string(nextName);
    if (!save(*preset, wasActive, error)) return false;
    if (!remove(previousName, error)) return false;
    return true;
}

bool InputPresetStore::setActive(std::string_view name, std::string &error)
{
    if (!isValidName(name) || !load(name))
    {
        error = "Preset does not exist";
        return false;
    }
    return writeAtomically(storageRoot() / ActivePresetFile, std::string(ActivePresetKey) + "=" + std::string(name) + "\n", error);
}

bool InputPresetStore::remove(std::string_view name, std::string &error)
{
    if (!isValidName(name))
    {
        error = "Invalid preset name";
        return false;
    }
    if (name == DefaultPresetName)
    {
        error = "The Default preset cannot be deleted";
        return false;
    }

    std::error_code removeError;
    const std::filesystem::path path = presetPath(storageRoot(), name);
    if (!std::filesystem::exists(path, removeError))
    {
        error = "Preset does not exist";
        return false;
    }
    if (removeError || !std::filesystem::remove(path, removeError))
    {
        error = "Cannot remove preset file";
        return false;
    }
    return true;

}
}
