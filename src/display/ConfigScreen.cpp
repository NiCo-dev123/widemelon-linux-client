#include "display/ConfigScreen.h"

#include "display/DisplaySession.h"

namespace widemelon
{

    bool ConfigScreen::show(Config config, bool inputTest, std::string &error)
    {
        return runDisplaySession(config, inputTest, error);
    }

}
