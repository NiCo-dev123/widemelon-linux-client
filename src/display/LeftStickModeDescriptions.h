#pragma once

#include <array>
#include <string_view>

namespace widemelon
{
    struct LeftStickModeDescription
    {
        std::array<std::string_view, 3> lines;
    };

    inline constexpr LeftStickModeDescription leftStickModeDescription(std::string_view mode)
    {
        if (mode == "D-pad")
            return {{{"The left stick mirrors", "D-pad inputs.", ""}}};
        if (mode == "SM64 D-pad")
            return {{{"The left stick mirrors D-pad inputs.", "Y is held automatically for running.", ""}}};
        if (mode == "SM64 Auto")
            return {{{"Emulates the stylus in SM64DS.", "Touch control is automatic.", "Adds input delay."}}};
        if (mode == "SM64 Manual")
            return {{{"Emulates the stylus for true analog", "movement in SM64DS.", "Press R2 to toggle touch."}}};
        if (mode == "Cursor")
            return {{{"Moves the stylus freely.", "Hold R2 to touch the screen.", ""}}};
        return {{{"The left stick is disabled.", "", ""}}};
    }
}
