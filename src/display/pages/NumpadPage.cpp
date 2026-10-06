#include "display/pages/NumpadPage.h"

namespace widemelon::display::pages
{

    PageDefinition numpadPage(std::string_view title)
    {
        return {
            PageId::Numpad,
            title,
            {
                {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                {FooterItemType::Icon, FooterIcon::B, "DELETE"},
                {FooterItemType::Icon, FooterIcon::X, "BACK"},
                {FooterItemType::Icon, FooterIcon::Start, "OK"},
            },
            {},
        };
    }

}
