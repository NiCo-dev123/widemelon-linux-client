#pragma once

#include <chrono>
#include <string>

#include "config/Config.h"

namespace widemelon
{

class WebSocketClient
{
public:
    std::string connectAndAuthenticate(const Config& config, std::chrono::seconds timeout);
};

}
