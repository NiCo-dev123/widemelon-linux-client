#include "display/pages/RightStickModPage.h"

#include "input/MphManualStickMod.h"

namespace widemelon::display::pages
{

    PageDefinition rightStickModPage(std::string_view mode)
    {
        PageDefinition page{
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
        if (mode == "MPH Manual")
            page.sections.insert(page.sections.end() - 1, {"", {}, {{FieldType::Range, "MPH camera speed", ValueId::MphManualSpeed, PageId::Home, ActionId::None, MphManualSpeedMinimum, MphManualSpeedMaximum, MphManualSpeedStep}}});
        return page;
    }

}
