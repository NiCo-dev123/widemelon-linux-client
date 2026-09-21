#pragma once

#include <filesystem>
#include <string>

namespace widemelon
{

class Logger
{
public:
    static void open(const std::filesystem::path& path);
    static void info(const std::string& message);
    static void error(const std::string& message);
};

}
