#pragma once

#include <string>

#include "config/Config.h"

namespace widemelon
{

class ConfigScreen
{
public:
    static bool show(const Config& config, std::string& error);
};

}
