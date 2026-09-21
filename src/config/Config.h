#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace widemelon
{

struct Config
{
    std::string host;
    std::uint16_t port = 24872;
    std::string pairingCode;
};

struct ConfigLoadResult
{
    bool ok = false;
    Config config;
    std::string error;
};

class ConfigLoader
{
public:
    static ConfigLoadResult loadFile(const std::filesystem::path& path);
    static ConfigLoadResult loadNextToExecutable();
};

}
