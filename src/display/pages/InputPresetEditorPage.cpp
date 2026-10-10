#include "display/pages/InputPresetEditorPage.h"

namespace widemelon::display::pages
{
    const PageDefinition &inputPresetEditorPage()
    {
        static const PageDefinition page{
            PageId::InputPresetEditor,
            "Edit preset",
            {{FooterItemType::Icon, FooterIcon::B, "BACK"}},
            {{"", {"Stick settings will be added here."}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::InputPresetManager}}}},
        };
        return page;
    }
}
