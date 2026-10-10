#include "display/pages/SettingsPage.h"

namespace widemelon::display::pages
{

    const PageDefinition &settingsPage()
    {
        static const PageDefinition page{
            PageId::Settings,
            "Settings",
            {
                {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                {FooterItemType::Icon, FooterIcon::B, "BACK"},
            },
            {
                {"Theme", {}, {{FieldType::Choice, {}, ValueId::ActiveTheme}}},
                {"Input mapping", {}, {{FieldType::NavigationButton, "Input presets", ValueId::None, PageId::InputPresetManager}}},
                {"Navigation", {}, {
                    {FieldType::NavigationButton, "Back", ValueId::None, PageId::Home},
                    {FieldType::ActionButton, "Quit", ValueId::None, PageId::Home, ActionId::Quit},
                }},
                {"More", {}, {{FieldType::NavigationButton, "About this app", ValueId::None, PageId::About}}},
            },
        };
        return page;
    }

}
