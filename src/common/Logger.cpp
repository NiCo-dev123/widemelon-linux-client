#include "common/Logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace
{

std::mutex logMutex;
std::ofstream logFile;

void write(const char* level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex);
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_r(&now, &localTime);

    std::ostream& output = logFile.is_open() ? static_cast<std::ostream&>(logFile) : std::cerr;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [" << level << "] " << message << '\n';
    output.flush();
}

}

namespace widemelon
{

void Logger::open(const std::filesystem::path& path)
{
    std::lock_guard<std::mutex> lock(logMutex);
    logFile.open(path, std::ios::out | std::ios::app);
    if (!logFile)
        std::cerr << "Cannot open application log: " << path << '\n';
}

void Logger::info(const std::string& message)
{
    write("INFO", message);
}

void Logger::error(const std::string& message)
{
    write("ERROR", message);
}

}
