#include "config/Config.h"

#include <array>
#include <arpa/inet.h>
#include <charconv>
#include <fstream>
#include <string_view>
#include <unistd.h>

namespace
{

std::string trim(std::string value)
{
    constexpr std::string_view whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool isIPv4Address(const std::string& value)
{
    in_addr address{};
    return inet_pton(AF_INET, value.c_str(), &address) == 1;
}

widemelon::ConfigLoadResult failure(std::string error)
{
    return {false, {}, std::move(error)};
}

}

namespace widemelon
{

ConfigLoadResult ConfigLoader::loadFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) return failure("Cannot read configuration file: " + path.string());

    Config config;
    bool hasHost = false;
    bool hasPort = false;
    bool hasPairingCode = false;
    std::string line;
    unsigned int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        line = trim(std::move(line));
        if (line.empty() || line.front() == '#') continue;

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
            return failure("Invalid configuration line " + std::to_string(lineNumber));
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key.empty() || value.empty())
            return failure("Empty configuration key or value on line " + std::to_string(lineNumber));

        if (key == "host")
        {
            if (hasHost) return failure("Duplicate host setting");
            config.host = value;
            hasHost = true;
        }
        else if (key == "port")
        {
            if (hasPort) return failure("Duplicate port setting");
            unsigned int parsedPort = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsedPort);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()
                || parsedPort == 0 || parsedPort > 65535)
                return failure("Invalid port");
            config.port = static_cast<std::uint16_t>(parsedPort);
            hasPort = true;
        }
        else if (key == "pairing_code")
        {
            if (hasPairingCode) return failure("Duplicate pairing_code setting");
            config.pairingCode = value;
            hasPairingCode = true;
        }
        else return failure("Unknown configuration key: " + key);
    }

    if (!hasHost) return failure("Missing host setting");
    if (!hasPairingCode) return failure("Missing pairing_code setting");
    return validate(std::move(config));
}

ConfigLoadResult ConfigLoader::validate(Config config)
{
    if (!isIPv4Address(config.host)) return failure("host must be an IPv4 address");
    if (config.port == 0) return failure("Invalid port");
    if (config.pairingCode.size() != 10
        || config.pairingCode.find_first_not_of("0123456789") != std::string::npos)
        return failure("pairing_code must contain exactly ten digits");
    return {true, std::move(config), {}};
}

ConfigLoadResult ConfigLoader::loadNextToExecutable()
{
    std::array<char, 4096> executablePath{};
    const ssize_t length = readlink("/proc/self/exe", executablePath.data(), executablePath.size() - 1);
    if (length <= 0 || static_cast<std::size_t>(length) >= executablePath.size() - 1)
        return failure("Cannot determine executable path");
    executablePath[static_cast<std::size_t>(length)] = '\0';
    return loadFile(std::filesystem::path(executablePath.data()).parent_path() / "widemelon-client.conf");
}

bool ConfigLoader::saveNextToExecutable(const Config& config, std::string& error)
{
    const ConfigLoadResult checked = validate(config);
    if (!checked.ok)
    {
        error = checked.error;
        return false;
    }
    std::array<char, 4096> executablePath{};
    const ssize_t length = readlink("/proc/self/exe", executablePath.data(), executablePath.size() - 1);
    if (length <= 0 || static_cast<std::size_t>(length) >= executablePath.size() - 1)
    {
        error = "Cannot determine executable path";
        return false;
    }
    executablePath[static_cast<std::size_t>(length)] = '\0';
    const std::filesystem::path path = std::filesystem::path(executablePath.data()).parent_path()
        / "widemelon-client.conf";
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (!file)
    {
        error = "Cannot write configuration file";
        return false;
    }
    file << "host=" << config.host << '\n'
         << "port=" << config.port << '\n'
         << "pairing_code=" << config.pairingCode << '\n';
    file.close();
    if (!file || std::rename(temporary.c_str(), path.c_str()) != 0)
    {
        std::remove(temporary.c_str());
        error = "Cannot save configuration file";
        return false;
    }
    return true;
}

}
