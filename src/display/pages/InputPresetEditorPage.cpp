#include "display/pages/InputPresetEditorPage.h"
#include "input/InputPreset.h"

#include <string>
#include <array>
#include <vector>

namespace widemelon::display::pages
{
    PageDefinition inputPresetEditorPage(std::string_view presetName, std::string_view leftMode, std::string_view rightMode)
    {
        static std::string displayedPresetName;
        displayedPresetName = presetName;
        std::vector<FieldDefinition> leftStickFields{{FieldType::Choice, "Left stick mode", ValueId::LeftStickMode}};
        std::vector<FieldDefinition> rightStickFields{{FieldType::Choice, "Right stick mode", ValueId::RightStickMode}};
        if (leftMode == "Cursor" || leftMode == "Cursor warped")
        {
            leftStickFields.push_back({FieldType::Range, "Cursor deadzone", ValueId::LeftCursorDeadzonePercent, PageId::Home, ActionId::None, CursorDeadzoneMinimumPercent, CursorDeadzoneMaximumPercent, CursorDeadzoneStepPercent});
            leftStickFields.push_back({FieldType::Range, "Cursor speed", ValueId::LeftCursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25});
        }
        if (rightMode == "Cursor" || rightMode == "Cursor warped")
        {
            rightStickFields.push_back({FieldType::Range, "Cursor deadzone", ValueId::RightCursorDeadzonePercent, PageId::Home, ActionId::None, CursorDeadzoneMinimumPercent, CursorDeadzoneMaximumPercent, CursorDeadzoneStepPercent});
            rightStickFields.push_back({FieldType::Range, "Cursor speed", ValueId::RightCursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25});
        }
        const std::array<ValueId, PhysicalInputCount> mappingValues{
            ValueId::MappingA, ValueId::MappingB, ValueId::MappingX, ValueId::MappingY, ValueId::MappingL, ValueId::MappingR,
            ValueId::MappingL2, ValueId::MappingR2, ValueId::MappingDpadLeft, ValueId::MappingDpadRight, ValueId::MappingDpadUp,
            ValueId::MappingDpadDown, ValueId::MappingSelect, ValueId::MappingStart, ValueId::MappingL3, ValueId::MappingR3};
        std::vector<FieldDefinition> mappingFields;
        for (std::size_t index = 0; index < mappingValues.size(); ++index)
            mappingFields.push_back({FieldType::ActionButton, "", mappingValues[index], PageId::Home, ActionId::ConfigureInputMapping});
        PageDefinition page{
            PageId::InputPresetEditor,
            "Edit preset",
            {{FooterItemType::Icon, FooterIcon::A, "SELECT"}, {FooterItemType::Icon, FooterIcon::B, "QUIT"}, {FooterItemType::Icon, FooterIcon::Start, "SAVE"}},
            {
                {"", {displayedPresetName}, {}},
                {"Left stick", {}, leftStickFields},
                {"Right stick", {}, rightStickFields},
                {"Key mapping", {}, mappingFields},
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
