#include "display/pages/LeftStickModPage.h"

#include "input/CursorStickMod.h"
#include "input/Sm64DpadMod.h"
#include "input/Sm64StickMod.h"

namespace widemelon::display::pages
{

    std::string_view leftStickModeDescription(std::string_view mode)
    {
        if (mode == "D-pad")
            return "The left stick mirrors\nD-pad inputs.";
        if (mode == "SM64 D-pad")
            return "The left stick mirrors D-pad inputs.\nY is held automatically for running.";
        if (mode == "SM64 Auto")
            return "Emulates the stylus in SM64DS.\nTouch control is automatic.\nAdds input delay.";
        if (mode == "SM64 Manual")
            return "Emulates the stylus for true analog\nmovement in SM64DS.\nPress R2 to toggle touch.";
        if (mode == "Cursor")
            return "Moves the stylus freely.\nHold R2 to touch the screen.";
        return "The left stick is disabled.";
    }

    PageDefinition leftStickModPage(std::string_view mode)
    {
        PageDefinition page{
            PageId::LeftStickMod,
            "Left stick mod",
            {
                {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                {FooterItemType::Icon, FooterIcon::B, "BACK"},
            },
            {
                {"Mode hint", {"Select the desired behaviour\nfor the left stick while in-game."}, {}},
                {"", {}, {{FieldType::Choice, {}, ValueId::LeftStickMode}}},
            },
        };

        page.sections.push_back({"", {leftStickModeDescription(mode)}, {}});

        if (mode == "SM64 Auto")
        {
            page.sections.push_back({"", {}, {
                {FieldType::Range, "SM64 Auto delay", ValueId::Sm64AutoDelayFrames, PageId::Home, ActionId::None,
                 Sm64TouchCenterHoldFramesMinimum, Sm64TouchCenterHoldFramesMaximum},
                {FieldType::Range, "SM64 Auto release delay", ValueId::Sm64AutoReleaseDelayMs, PageId::Home, ActionId::None,
                 Sm64TouchReleaseDelayMinimumMs, Sm64TouchReleaseDelayMaximumMs, Sm64TouchReleaseDelayStepMs},
            }});
        }
        else if (mode == "SM64 D-pad")
        {
            page.sections.push_back({"", {}, {{FieldType::Range, "SM64 D-pad deadzone", ValueId::Sm64DpadDeadzonePercent,
                                                 PageId::Home, ActionId::None, Sm64DpadDeadzoneMinimumPercent,
                                                 Sm64DpadDeadzoneMaximumPercent}}});
        }
        else if (mode == "Cursor")
        {
            page.sections.push_back({"", {}, {{FieldType::Range, "Cursor speed limit", ValueId::CursorSpeedLimit,
                                                 PageId::Home, ActionId::None, CursorSpeedMinimum, CursorSpeedMaximum,
                                                 CursorSpeedStep}}});
        }

        page.sections.push_back({"Navigation", {}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}});
        return page;
    }

}
