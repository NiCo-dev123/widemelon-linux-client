#pragma once

#include "display/PageDefinition.h"

#include <string_view>

namespace widemelon::display
{

    enum class MenuEntryType : unsigned char
    {
        SectionTitle,
        Hint,
        Field,
    };

    struct MenuEntry
    {
        MenuEntryType type{MenuEntryType::Field};
        const SectionDefinition *section{nullptr};
        const FieldDefinition *field{nullptr};
        std::string_view text;

        bool selectable() const { return type == MenuEntryType::Field && field != nullptr; }
    };

}
