#pragma once

#include <string>

#include "config/Config.h"

namespace widemelon
{

inline constexpr std::chrono::milliseconds UiNavigationCooldown{100};

class ConfigScreen
{
public:
    static bool show(Config config, bool inputTest, std::string& error);
};

}
