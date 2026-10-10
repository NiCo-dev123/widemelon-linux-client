#include "display/pages/InputPresetManagerPage.h"

namespace widemelon::display::pages
{
    const PageDefinition &inputPresetManagerPage()
    {
        static const PageDefinition page{
            PageId::InputPresetManager,
            "Input presets",
            {{FooterItemType::Icon, FooterIcon::B, "BACK"}},
            {{"", {"Preset management will be added here."}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}}},
        };
        return page;
    }
}
