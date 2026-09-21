#include <iostream>
#include <string>

#include "common/Logger.h"
#include "config/Config.h"
#include "display/ConfigScreen.h"

int main(int argc, char** argv)
{
    widemelon::Logger::open("widemelon-client.log");
    widemelon::Logger::info("Application started");
    widemelon::Logger::info("Reading configuration");
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadNextToExecutable();
    if (!result.ok)
    {
        widemelon::Logger::error("Configuration error: " + result.error);
        widemelon::Logger::info("Opening configuration form with default values");
    }
    else widemelon::Logger::info("Configuration loaded for host " + result.config.host + ':'
        + std::to_string(result.config.port));

    std::string screenError;
    const bool inputTest = argc == 2 && std::string(argv[1]) == "--input-test";
    if (!widemelon::ConfigScreen::show(result.config, inputTest, screenError))
    {
        widemelon::Logger::error("Display error: " + screenError);
        std::cerr << "Display error: " << screenError << '\n';
        return 1;
    }
    widemelon::Logger::info("Application stopped");
    return 0;
}
