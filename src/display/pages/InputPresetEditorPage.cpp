#include "display/pages/InputPresetEditorPage.h"

namespace widemelon::display::pages
{
    PageDefinition inputPresetEditorPage(std::string_view leftMode, std::string_view rightMode)
    {
        PageDefinition page{
            PageId::InputPresetEditor,
            "Edit preset",
            {{FooterItemType::Icon, FooterIcon::A, "SELECT"}, {FooterItemType::Icon, FooterIcon::B, "QUIT"}, {FooterItemType::Icon, FooterIcon::Start, "SAVE"}},
            {
                {"Left stick", {}, {{FieldType::Choice, "", ValueId::LeftStickMode}}},
                {"Right stick", {}, {{FieldType::Choice, "", ValueId::RightStickMode}}},
                {"File", {}, {
                    {FieldType::ActionButton, "Save and quit", ValueId::None, PageId::Home, ActionId::SaveInputPreset},
                    {FieldType::ActionButton, "Quit without saving", ValueId::None, PageId::Home, ActionId::DiscardInputPreset},
                    {FieldType::ActionButton, "Delete and quit", ValueId::None, PageId::Home, ActionId::RequestDeleteInputPreset},
                }},
            },
        };
        if (leftMode == "SM64 Auto") page.sections.insert(page.sections.begin() + 1, {"", {}, {{FieldType::Range, "Center hold", ValueId::Sm64AutoDelayFrames, PageId::Home, ActionId::None, 2, 20, 1}, {FieldType::Range, "Stylus cooldown", ValueId::Sm64AutoReleaseDelayMs, PageId::Home, ActionId::None, 0, 2000, 100}}});
        else if (leftMode == "SM64 D-pad") page.sections.insert(page.sections.begin() + 1, {"", {}, {{FieldType::Range, "D-pad deadzone", ValueId::Sm64DpadDeadzonePercent, PageId::Home, ActionId::None, 25, 95, 5}}});
        else if (leftMode == "Cursor") page.sections.insert(page.sections.begin() + 1, {"", {}, {{FieldType::Range, "Cursor speed", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        if (rightMode == "Cursor") page.sections.insert(page.sections.end() - 1, {"", {}, {{FieldType::Range, "Cursor speed", ValueId::CursorSpeedLimit, PageId::Home, ActionId::None, 100, 500, 25}}});
        else if (rightMode == "MPH Manual" || rightMode == "MPH Auto") { page.sections.insert(page.sections.end() - 1, {"", {}, {{FieldType::Range, "MPH camera speed", ValueId::MphManualSpeed, PageId::Home, ActionId::None, 50, 500, 10}}}); if (rightMode == "MPH Auto") page.sections.insert(page.sections.end() - 1, {"", {}, {{FieldType::Range, "Stylus cooldown", ValueId::MphAutoReleaseDelayMs, PageId::Home, ActionId::None, 100, 1000, 25}}}); }
        return page;
    }
}
