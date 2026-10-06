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
                {"Calibration hint", {"Adjust the range of the left stick\nto correct asymmetrical inputs.\n(experimental feature)"}, {}},
                {"", {}, {
                    {FieldType::Range, "Top multiplier", ValueId::LeftStickScaleUpPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent},
                    {FieldType::Range, "Bottom multiplier", ValueId::LeftStickScaleDownPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent},
                    {FieldType::Range, "Left multiplier", ValueId::LeftStickScaleLeftPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent},
                    {FieldType::Range, "Right multiplier", ValueId::LeftStickScaleRightPercent, PageId::Home, ActionId::None,
                     LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent},
                }},
                {"Navigation", {}, {{FieldType::NavigationButton, "Back", ValueId::None, PageId::Settings}}},
            },
        };
        return page;
    }

}
