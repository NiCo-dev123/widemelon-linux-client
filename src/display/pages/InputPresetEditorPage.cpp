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
                {"Key mapping", {}, {
                    {FieldType::ActionButton, "L2 : Stylus mapping", ValueId::L2StylusMapping, PageId::Home, ActionId::ConfigureL2StylusMapping},
                    {FieldType::ActionButton, "R2 : Stylus mapping", ValueId::R2StylusMapping, PageId::Home, ActionId::ConfigureR2StylusMapping},
                }},
                {"File", {}, {
                    {FieldType::ActionButton, "Save and quit", ValueId::None, PageId::Home, ActionId::SaveInputPreset},
                    {FieldType::ActionButton, "Quit without saving", ValueId::None, PageId::Home, ActionId::DiscardInputPreset},
                    {FieldType::ActionButton, "Delete and quit", ValueId::None, PageId::Home, ActionId::RequestDeleteInputPreset},
                }},
            },
        };
        if (leftMode == "Cursor") page.sections.insert(page.sections.begin() + 2, {"Cursor speed", {}, {{FieldType::Range, "", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        if (rightMode == "Cursor") page.sections.insert(page.sections.end() - 1, {"Cursor speed", {}, {{FieldType::Range, "", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        return page;
    }
}
