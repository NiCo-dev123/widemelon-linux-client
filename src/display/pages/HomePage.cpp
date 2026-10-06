#include "display/pages/HomePage.h"

namespace widemelon::display::pages
{

    const PageDefinition &homePage()
    {
        static const PageDefinition page{
            PageId::Home,
            "WideMelon Client",
            {
                {FooterItemType::Icon, FooterIcon::A, "EDIT"},
                {FooterItemType::Icon, FooterIcon::Start, "CONNECT"},
            },
            {
                {"Server Address", {}, {{FieldType::TextInput, {}, ValueId::Host}}},
                {"Port", {}, {{FieldType::TextInput, {}, ValueId::Port}}},
                {"Session code", {}, {{FieldType::TextInput, {}, ValueId::PairingCode}}},
                {"Navigation", {}, {
                    {FieldType::ActionButton, "Connect", ValueId::None, PageId::Home, ActionId::Connect},
                    {FieldType::NavigationButton, "Settings", ValueId::None, PageId::Settings},
                }},
            },
        };
        return page;
    }

}
