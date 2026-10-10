#include "display/pages/InputPresetDeleteConfirmPage.h"

namespace widemelon::display::pages
{
    const PageDefinition &inputPresetDeleteConfirmPage()
    {
        static const PageDefinition page{
            PageId::InputPresetDeleteConfirm,
            "Delete preset",
            {{FooterItemType::Icon, FooterIcon::A, "SELECT"}, {FooterItemType::Icon, FooterIcon::B, "CANCEL"}},
            {{"", {"This cannot be undone."}, {
                {FieldType::ActionButton, "Cancel", ValueId::None, PageId::InputPresetEditor, ActionId::DiscardInputPreset},
                {FieldType::ActionButton, "Delete preset", ValueId::None, PageId::InputPresetManager, ActionId::ConfirmDeleteInputPreset},
            }}},
        };
        return page;
    }
}
