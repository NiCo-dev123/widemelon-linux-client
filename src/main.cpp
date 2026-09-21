#include <iostream>
#include <string>

#include "config/Config.h"
#include "display/ConfigScreen.h"

int main(int argc, char** argv)
{
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadNextToExecutable();
    if (!result.ok)
    {
        std::cerr << "Configuration error: " << result.error << '\n';
        return 1;
    }

    std::string screenError;
    const bool inputTest = argc == 2 && std::string(argv[1]) == "--input-test";
    if (!widemelon::ConfigScreen::show(result.config, inputTest, screenError))
    {
        std::cerr << "Display error: " << screenError << '\n';
        return 1;
    }
    return 0;
}
