#pragma once

#include <chrono>
#include <string>

#include "config/Config.h"

namespace widemelon
{

    inline constexpr std::chrono::milliseconds UiNavigationCooldown{100};
    inline constexpr std::chrono::milliseconds UiNavigationInitialDelay{250};
    inline constexpr std::chrono::milliseconds UiNavigationRetriggerDelay{150};

    inline constexpr int UiGameTitleFontSize{28};
    inline constexpr int UiGameFooterFontSize{12};
    inline constexpr int UiGameConnectionFontSize{25};
    inline constexpr int UiHintIconTextGap{10};

    inline constexpr int UiFormFontSize{20};
    inline constexpr int UiFormTitleFontSize{34};
    inline constexpr int UiFormFieldWidth{300};
    inline constexpr int UiFormFieldHeight{48};

    inline constexpr int UiKeyboardTitleFontSize{35};
    inline constexpr int UiKeyboardValueFontSize{35};
    inline constexpr int UiKeyboardActionFontSize{28};
    inline constexpr int UiKeyboardHintFontSize{35};
    inline constexpr int UiKeyboardKeyWidth{170};
    inline constexpr int UiKeyboardKeyHeight{85};
    inline constexpr int UiKeyboardColumns{4};
    inline constexpr int UiKeyboardStartX{290};
    inline constexpr int UiKeyboardStartY{205};

    class ConfigScreen
    {
    public:
        static bool show(Config config, bool inputTest, std::string &error);
    };

}
