#include "input/TspsStickConfig.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace widemelon
{
namespace
{
std::filesystem::path configPath()
{
    std::error_code error;
    const std::filesystem::path root = std::filesystem::exists("/mnt/SDCARD/spruce", error) && !error
        ? "/mnt/SDCARD/Saves/WideMelonClient"
        : "/mnt/SDCARD/.userdata/shared/WideMelonClient";
    return root / "tsps-stick.conf";
}

std::optional<int> valueFor(std::string_view line, std::string_view key)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0) return std::nullopt;
    try { return std::stoi(std::string(line.substr(prefix.size()))); }
    catch (...) { return std::nullopt; }
}

std::uint8_t clamped(std::optional<int> value)
{
    return static_cast<std::uint8_t>(std::clamp(value.value_or(LeftStickScaleDefaultPercent),
        static_cast<int>(LeftStickScaleMinimumPercent), static_cast<int>(LeftStickScaleMaximumPercent)));
}
}

std::optional<LeftStickCalibration> TspsStickConfig::load()
{
    std::ifstream input(configPath());
    if (!input) return std::nullopt;

    std::optional<int> left, right, up, down;
    std::string line;
    while (std::getline(input, line))
    {
        if (const auto value = valueFor(line, "left")) left = value;
        else if (const auto value = valueFor(line, "right")) right = value;
        else if (const auto value = valueFor(line, "up")) up = value;
        else if (const auto value = valueFor(line, "down")) down = value;
    }
    if (!left && !right && !up && !down) return std::nullopt;
    return LeftStickCalibration{clamped(left), clamped(right), clamped(up), clamped(down)};
}

LeftStickCalibration TspsStickConfig::loadOrMigrate(const LeftStickCalibration &legacyValue)
{
    if (const std::optional<LeftStickCalibration> current = load()) return *current;
    std::string ignoredError;
    save(legacyValue, ignoredError);
    return legacyValue;
}

bool TspsStickConfig::save(const LeftStickCalibration &calibration, std::string &error)
{
    const std::filesystem::path path = configPath();
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) { error = "Cannot create TSPS stick configuration directory"; return false; }

    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) { error = "Cannot write TSPS stick configuration"; return false; }
    output << "left=" << static_cast<int>(calibration.left) << '\n'
           << "right=" << static_cast<int>(calibration.right) << '\n'
           << "up=" << static_cast<int>(calibration.up) << '\n'
           << "down=" << static_cast<int>(calibration.down) << '\n';
    output.close();
    if (!output || std::rename(temporary.c_str(), path.c_str()) != 0)
    {
        std::filesystem::remove(temporary, directoryError);
        error = "Cannot save TSPS stick configuration";
        return false;
    }
    return true;
}
}
