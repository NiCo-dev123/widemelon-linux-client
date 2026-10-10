#include "display/pages/InputPresetManagerPage.h"

namespace widemelon::display::pages
{
    const PageDefinition &inputPresetManagerPage()
    {
        static const PageDefinition page{
            PageId::InputPresetManager,
            "Input presets",
            {
                {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                {FooterItemType::Icon, FooterIcon::B, "BACK"},
            },
            {
                {"", {"Select a configuration file"}, {{FieldType::Choice, {}, ValueId::ActiveInputPreset}}},
                {"Options", {}, {
                    {FieldType::NavigationButton, "Edit", ValueId::None, PageId::InputPresetEditor},
                    {FieldType::ActionButton, "Rename", ValueId::None, PageId::Home, ActionId::RenameInputPreset},
                    {FieldType::ActionButton, "New", ValueId::None, PageId::Home, ActionId::NewInputPreset},
                }},
                {"Navigation", {}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}},
            },
        };
        return page;
    }
}
