#include <iostream>

#include "config/Config.h"
#include "display/ConfigScreen.h"

int main()
{
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadNextToExecutable();
    if (!result.ok)
    {
        std::cerr << "Configuration error: " << result.error << '\n';
        return 1;
    }

    std::string screenError;
    if (!widemelon::ConfigScreen::show(result.config, screenError))
    {
        std::cerr << "Display error: " << screenError << '\n';
        return 1;
    }
    return 0;
}
