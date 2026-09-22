#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace widemelon
{

    struct Config
    {
        std::string host;
        std::uint16_t port = 24800;
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
        static ConfigLoadResult validate(Config config);
        static ConfigLoadResult loadFile(const std::filesystem::path &path);
        static ConfigLoadResult loadNextToExecutable();
        static bool saveNextToExecutable(const Config &config, std::string &error);
    };

}
