#include "display/pages/LeftStickCalibrationPage.h"

#include "input/LeftStickCalibration.h"

namespace widemelon::display::pages
{

    const PageDefinition &leftStickCalibrationPage()
    {
        static const PageDefinition page{
            PageId::LeftStickCalibration,
            "Left stick calibration",
            {{FooterItemType::Icon, FooterIcon::B, "BACK"}},
            {
                {"", {"Adjust the range of the left stick\nto correct unsymmetrical inputs.\n(experimental feature)"}, {}},
                {"Top multiplier", {}, {{FieldType::Range, "", ValueId::LeftStickScaleUpPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent}}},
                {"Bottom multiplier", {}, {{FieldType::Range, "", ValueId::LeftStickScaleDownPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent}}},
                {"Left multiplier", {}, {{FieldType::Range, "", ValueId::LeftStickScaleLeftPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent}}},
                {"Vertical bottom multiplier", {}, {{FieldType::Range, "", ValueId::LeftStickScaleRightPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent}}},
                {"Navigation", {}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}},
            },
        };
        return page;
    }

}
