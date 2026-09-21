#pragma once

#include <string>

#include "config/Config.h"

namespace widemelon
{

class ConfigScreen
{
public:
    static bool show(Config config, bool inputTest, std::string& error);
};

}
