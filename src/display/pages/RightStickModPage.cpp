#include "display/pages/RightStickModPage.h"

namespace widemelon::display::pages
{

    const PageDefinition &rightStickModPage()
    {
        static const PageDefinition page{
            PageId::RightStickMod,
            "Right stick mod",
            {
                {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                {FooterItemType::Icon, FooterIcon::B, "BACK"},
            },
            {
                {"", {"Select the desired behaviour\nfor the right stick while in-game."}, {}},
                {"", {}, {{FieldType::Choice, {}, ValueId::RightStickMode}}},
                {"Navigation", {}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}},
            },
        };
        return page;
    }

}
