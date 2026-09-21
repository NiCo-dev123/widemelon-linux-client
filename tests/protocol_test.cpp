#include "config/Config.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace
{

int failures = 0;

void expect(bool condition, const std::string& description)
{
    if (!condition)
    {
        std::cerr << "Failed: " << description << '\n';
        ++failures;
    }
}

widemelon::ConfigLoadResult loadText(const std::string& text)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("widemelon-client-config-" + std::to_string(getpid()) + ".conf");
    {
        std::ofstream file(path);
        file << text;
    }
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadFile(path);
    std::filesystem::remove(path);
    return result;
}

}

int main()
{
    const auto valid = loadText("host=192.168.1.20\nport=24872\npairing_code=1234567890\n");
    expect(valid.ok, "accepts a valid configuration");
    expect(valid.config.host == "192.168.1.20", "loads host");
    expect(valid.config.port == 24872, "loads port");

    const auto defaultPort = loadText("host=192.168.1.20\npairing_code=1234567890\n");
    expect(defaultPort.ok && defaultPort.config.port == 24872, "uses the default port");

    expect(!loadText("host=example.com\npairing_code=1234567890\n").ok,
           "rejects a non-IPv4 host");
    expect(!loadText("host=192.168.1.20\nport=70000\npairing_code=1234567890\n").ok,
           "rejects an invalid port");
    expect(!loadText("host=192.168.1.20\npairing_code=123\n").ok,
           "rejects an invalid pairing code");
    expect(!loadText("host=192.168.1.20\nunknown=value\npairing_code=1234567890\n").ok,
           "rejects unknown settings");

    return failures == 0 ? 0 : 1;
}
