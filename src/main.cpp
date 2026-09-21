#include <iostream>

#include "config/Config.h"

int main()
{
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadNextToExecutable();
    if (!result.ok)
    {
        std::cerr << "Configuration error: " << result.error << '\n';
        return 1;
    }

    std::cout << "Configuration loaded for " << result.config.host << ':' << result.config.port << '\n';
    return 0;
}
