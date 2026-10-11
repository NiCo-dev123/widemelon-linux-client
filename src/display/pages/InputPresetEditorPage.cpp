#include "display/pages/InputPresetEditorPage.h"

#include <string>
#include <vector>

namespace widemelon::display::pages
{
    PageDefinition inputPresetEditorPage(std::string_view presetName, std::string_view leftMode, std::string_view rightMode)
    {
        static std::string displayedPresetName;
        displayedPresetName = presetName;
        std::vector<FieldDefinition> leftStickFields{{FieldType::Choice, "Left stick mode", ValueId::LeftStickMode}};
        std::vector<FieldDefinition> rightStickFields{{FieldType::Choice, "Right stick mode", ValueId::RightStickMode}};
        if (leftMode == "Cursor")
            leftStickFields.push_back({FieldType::Range, "Cursor speed", ValueId::LeftCursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25});
        if (rightMode == "Cursor")
            rightStickFields.push_back({FieldType::Range, "Cursor speed", ValueId::RightCursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25});
        PageDefinition page{
            PageId::InputPresetEditor,
            "Edit preset",
            {{FooterItemType::Icon, FooterIcon::A, "SELECT"}, {FooterItemType::Icon, FooterIcon::B, "QUIT"}, {FooterItemType::Icon, FooterIcon::Start, "SAVE"}},
            {
                {"", {displayedPresetName}, {}},
                {"Left stick", {}, leftStickFields},
                {"Right stick", {}, rightStickFields},
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
        return page;
    }
}
