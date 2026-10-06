#pragma once

#include "display/PageDefinition.h"

#include <string_view>

namespace widemelon::display::pages
{

    std::string_view leftStickModeDescription(std::string_view mode);
    PageDefinition leftStickModPage(std::string_view mode);

}
