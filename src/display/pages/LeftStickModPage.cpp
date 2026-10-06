#include "display/pages/LeftStickModPage.h"

#include "display/LeftStickModeDescriptions.h"
#include "input/CursorStickMod.h"
#include "input/Sm64DpadMod.h"
#include "input/Sm64StickMod.h"

namespace widemelon::display::pages
{

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
                {"Mode hint", {"Select the desired behaviour", "for the left stick while in-game."}, {}},
                {"", {}, {{FieldType::Choice, {}, ValueId::LeftStickMode}}},
            },
        };

        const LeftStickModeDescription description = leftStickModeDescription(mode);
        SectionDefinition descriptionSection;
        for (const std::string_view line : description.lines)
            if (!line.empty()) descriptionSection.hintLines.push_back(line);
        if (!descriptionSection.hintLines.empty()) page.sections.push_back(std::move(descriptionSection));

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
