#include "input/InputPreset.h"

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

const std::array<std::filesystem::path, 4> StorageRoots{
    "/mnt/SDCARD/.userdata/shared/WideMelonClient",
    "/mnt/SDCARD/.userdata/tg5050/WideMelonClient",
    "/mnt/sdcard/Saves/WideMelonClient",
    "/mnt/SDCARD/Saves/WideMelonClient",
};

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

std::string json(const InputPreset &preset)
{
    return "{\n"
           "  \"name\": \"" + preset.name + "\",\n"
           "  \"left_stick_mode\": \"" + preset.leftStickMode + "\",\n"
           "  \"right_stick_mode\": \"" + preset.rightStickMode + "\",\n"
           "  \"mph_camera_speed\": " + std::to_string(preset.mphCameraSpeed) + ",\n"
           "  \"mph_auto_release_delay_ms\": " + std::to_string(preset.mphAutoReleaseDelayMs) + ",\n"
           "  \"sm64_auto_center_hold_frames\": " + std::to_string(preset.sm64AutoCenterHoldFrames) + ",\n"
           "  \"sm64_auto_release_delay_ms\": " + std::to_string(preset.sm64AutoReleaseDelayMs) + ",\n"
           "  \"sm64_dpad_deadzone_percent\": " + std::to_string(preset.sm64DpadDeadzonePercent) + ",\n"
           "  \"cursor_speed_limit\": " + std::to_string(preset.cursorSpeedLimit) + ",\n"
           "  \"left_stick_calibration\": {\n"
           "    \"left\": " + std::to_string(preset.leftStickCalibration[0]) + ",\n"
           "    \"right\": " + std::to_string(preset.leftStickCalibration[1]) + ",\n"
           "    \"up\": " + std::to_string(preset.leftStickCalibration[2]) + ",\n"
           "    \"down\": " + std::to_string(preset.leftStickCalibration[3]) + "\n"
           "  }\n"
           "}\n";
}

std::optional<InputPreset> parse(std::string_view document)
{
    const std::optional<std::string> name = jsonString(document, "name");
    if (!name || !InputPresetStore::isValidName(*name)) return std::nullopt;

    InputPreset preset;
    preset.name = *name;
    preset.leftStickMode = jsonString(document, "left_stick_mode").value_or(preset.leftStickMode);
    preset.rightStickMode = jsonString(document, "right_stick_mode").value_or(preset.rightStickMode);
    preset.mphCameraSpeed = static_cast<std::uint16_t>(clampValue(jsonInt(document, "mph_camera_speed"), 100, 50, 500));
    preset.mphAutoReleaseDelayMs = static_cast<std::uint16_t>(clampValue(jsonInt(document, "mph_auto_release_delay_ms"), 400, 100, 1000));
    preset.sm64AutoCenterHoldFrames = static_cast<std::uint8_t>(clampValue(jsonInt(document, "sm64_auto_center_hold_frames"), 12, 2, 20));
    preset.sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(clampValue(jsonInt(document, "sm64_auto_release_delay_ms"), 1000, 0, 2000));
    preset.sm64DpadDeadzonePercent = static_cast<std::uint8_t>(clampValue(jsonInt(document, "sm64_dpad_deadzone_percent"), 75, 25, 95));
    preset.cursorSpeedLimit = static_cast<std::uint16_t>(clampValue(jsonInt(document, "cursor_speed_limit"), 300, 100, 500));
    preset.leftStickCalibration[0] = static_cast<std::uint8_t>(clampValue(jsonInt(document, "left"), 100, 50, 150));
    preset.leftStickCalibration[1] = static_cast<std::uint8_t>(clampValue(jsonInt(document, "right"), 100, 50, 150));
    preset.leftStickCalibration[2] = static_cast<std::uint8_t>(clampValue(jsonInt(document, "up"), 100, 50, 150));
    preset.leftStickCalibration[3] = static_cast<std::uint8_t>(clampValue(jsonInt(document, "down"), 100, 50, 150));
    return preset;
}
}

bool InputPresetStore::isValidName(std::string_view name)
{
    if (name.empty() || name.size() > 48) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char character)
    {
        return std::isalnum(character) || character == ' ' || character == '-' || character == '_';
    });
}

std::optional<InputPreset> InputPresetStore::load(std::string_view name)
{
    if (!isValidName(name)) return std::nullopt;
    for (const std::filesystem::path &root : StorageRoots)
    {
        const std::optional<std::string> document = readFile(presetPath(root, name));
        if (!document) continue;
        return parse(*document);
    }
    return std::nullopt;
}

std::optional<std::string> InputPresetStore::activeName()
{
    const std::string prefix = std::string(ActivePresetKey) + "=";
    for (const std::filesystem::path &root : StorageRoots)
    {
        const std::optional<std::string> document = readFile(root / ActivePresetFile);
        if (!document) continue;
        if (document->compare(0, prefix.size(), prefix) != 0) continue;
        const std::string name = document->substr(prefix.size());
        const std::size_t newline = name.find_first_of("\r\n");
        const std::string trimmed = name.substr(0, newline);
        if (isValidName(trimmed)) return trimmed;
    }
    return std::nullopt;
}

std::vector<std::string> InputPresetStore::names()
{
    std::vector<std::string> result;
    for (const std::filesystem::path &root : StorageRoots)
    {
        std::error_code error;
        const std::filesystem::path directory = root / "input-presets";
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error))
        {
            if (error || !entry.is_regular_file(error) || entry.path().extension() != ".json") continue;
            const std::string name = entry.path().stem().string();
            if (isValidName(name)) result.push_back(name);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::optional<InputPreset> InputPresetStore::loadActive()
{
    const std::optional<std::string> name = activeName();
    return name ? load(*name) : std::nullopt;
}

bool InputPresetStore::save(const InputPreset &preset, bool makeActive, std::string &error)
{
    if (!isValidName(preset.name))
    {
        error = "Invalid preset name";
        return false;
    }
    for (const std::filesystem::path &root : StorageRoots)
    {
        std::string candidateError;
        if (!writeAtomically(presetPath(root, preset.name), json(preset), candidateError))
        {
            error = root.string() + ": " + candidateError;
            continue;
        }
        if (!makeActive) return true;
        if (writeAtomically(root / ActivePresetFile, std::string(ActivePresetKey) + "=" + preset.name + "\n", candidateError)) return true;
        error = root.string() + ": " + candidateError;
        return false;
    }
    return false;
}

bool InputPresetStore::setActive(std::string_view name, std::string &error)
{
    if (!isValidName(name) || !load(name))
    {
        error = "Preset does not exist";
        return false;
    }
    for (const std::filesystem::path &root : StorageRoots)
    {
        std::string candidateError;
        if (writeAtomically(root / ActivePresetFile, std::string(ActivePresetKey) + "=" + std::string(name) + "\n", candidateError)) return true;
        error = root.string() + ": " + candidateError;
    }
    return false;
}

bool InputPresetStore::remove(std::string_view name, std::string &error)
{
    if (!isValidName(name))
    {
        error = "Invalid preset name";
        return false;
    }

    std::error_code removeError;
    for (const std::filesystem::path &root : StorageRoots)
    {
        const std::filesystem::path path = presetPath(root, name);
        if (!std::filesystem::exists(path, removeError)) continue;
        if (removeError || !std::filesystem::remove(path, removeError))
        {
            error = root.string() + ": Cannot remove preset file";
            return false;
        }
        return true;
    }

    error = "Preset does not exist";
    return false;
}
}
