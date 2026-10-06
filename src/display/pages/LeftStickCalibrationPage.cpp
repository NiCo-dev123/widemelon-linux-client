#include "display/pages/LeftStickCalibrationPage.h"

namespace widemelon::display::pages
{

    const PageDefinition &leftStickCalibrationPage()
    {
        static const PageDefinition page{
            PageId::LeftStickCalibration,
            "Left stick calibration",
            {{FooterItemType::Icon, FooterIcon::B, "BACK"}},
            {},
        };
        return page;
    }

}
