#include "display/pages/InputPresetEditorPage.h"

#include <string>

namespace widemelon::display::pages
{
    PageDefinition inputPresetEditorPage(std::string_view presetName, std::string_view leftMode, std::string_view rightMode)
    {
        static std::string displayedPresetName;
        displayedPresetName = presetName;
        PageDefinition page{
            PageId::InputPresetEditor,
            "Edit preset",
            {{FooterItemType::Icon, FooterIcon::A, "SELECT"}, {FooterItemType::Icon, FooterIcon::B, "QUIT"}, {FooterItemType::Icon, FooterIcon::Start, "SAVE"}},
            {
                {"", {displayedPresetName}, {}},
                {"Left stick mode", {}, {{FieldType::Choice, "", ValueId::LeftStickMode}}},
                {"Right stick mode", {}, {{FieldType::Choice, "", ValueId::RightStickMode}}},
                {"File", {}, {
                    {FieldType::ActionButton, "Save and quit", ValueId::None, PageId::Home, ActionId::SaveInputPreset},
                    {FieldType::ActionButton, "Quit without saving", ValueId::None, PageId::Home, ActionId::DiscardInputPreset},
                    {FieldType::ActionButton, "Delete and quit", ValueId::None, PageId::Home, ActionId::RequestDeleteInputPreset},
                }},
            },
        };
        if (leftMode == "SM64 Auto") page.sections.insert(page.sections.begin() + 2, {"Center hold", {}, {{FieldType::Range, "", ValueId::Sm64AutoDelayFrames, PageId::Home, ActionId::None, 2, 20, 1}}});
        if (leftMode == "SM64 Auto") page.sections.insert(page.sections.begin() + 3, {"Stylus cooldown", {}, {{FieldType::Range, "", ValueId::Sm64AutoReleaseDelayMs, PageId::Home, ActionId::None, 0, 2000, 100}}});
        else if (leftMode == "SM64 D-pad") page.sections.insert(page.sections.begin() + 2, {"D-pad deadzone", {}, {{FieldType::Range, "", ValueId::Sm64DpadDeadzonePercent, PageId::Home, ActionId::None, 25, 95, 5}}});
        else if (leftMode == "Cursor") page.sections.insert(page.sections.begin() + 2, {"Cursor speed", {}, {{FieldType::Range, "", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        if (rightMode == "Cursor") page.sections.insert(page.sections.end() - 1, {"Cursor speed", {}, {{FieldType::Range, "", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        else if (rightMode == "MPH Manual" || rightMode == "MPH Auto")
        {
            page.sections.insert(page.sections.end() - 1, {"MPH camera speed", {}, {{FieldType::Range, "", ValueId::MphManualSpeed, PageId::Home, ActionId::None, 50, 500, 10}}});
            if (rightMode == "MPH Auto") page.sections.insert(page.sections.end() - 1, {"Stylus cooldown", {}, {{FieldType::Range, "", ValueId::MphAutoReleaseDelayMs, PageId::Home, ActionId::None, 100, 1000, 25}}});
        }
        return page;
    }
}
