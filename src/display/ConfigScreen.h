#pragma once

#include <chrono>
#include <string>

#include "config/Config.h"

namespace widemelon
{

inline constexpr std::chrono::milliseconds UiNavigationCooldown{100};
inline constexpr std::chrono::milliseconds UiNavigationInitialDelay{250};
inline constexpr std::chrono::milliseconds UiNavigationRetriggerDelay{150};

class ConfigScreen
{
public:
    static bool show(Config config, bool inputTest, std::string& error);
};

}
