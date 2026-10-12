#pragma once

#include "input/LeftStickCalibration.h"

#include <optional>
#include <string>

namespace widemelon
{
// TSPS-specific correction for the analog-stick range reported by its input
// driver. This intentionally does not belong to portable input presets.
class TspsStickConfig
{
public:
    static std::optional<LeftStickCalibration> load();
    static LeftStickCalibration loadOrMigrate(const LeftStickCalibration &legacyValue);
    static bool save(const LeftStickCalibration &calibration, std::string &error);
};
}
