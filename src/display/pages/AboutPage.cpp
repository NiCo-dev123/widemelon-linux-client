#include "display/pages/AboutPage.h"

#include <string>

namespace widemelon::display::pages
{

    const PageDefinition &aboutPage()
    {
        static const std::string version = std::string("WideMelon Client v") + WIDEMELON_VERSION;
        static const PageDefinition page{
            PageId::About,
            "About this app",
            {{FooterItemType::Icon, FooterIcon::B, "BACK"}},
            {{"", {version}, {}}},
        };
        return page;
    }

}
