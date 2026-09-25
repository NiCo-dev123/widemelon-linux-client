#pragma once

#include <chrono>
#include <string>

#include "config/Config.h"

namespace widemelon
{

    inline constexpr std::chrono::milliseconds UiNavigationCooldown{100};
    inline constexpr std::chrono::milliseconds UiNavigationInitialDelay{250};
    inline constexpr std::chrono::milliseconds UiNavigationRetriggerDelay{150};

    inline constexpr int UiGameTitleTextScale{4};
    inline constexpr int UiGameFooterTextScale{2};

    inline constexpr int UiFormTextSize{20};
    inline constexpr int UiFormTitleTextSize{34};
    inline constexpr int UiFormFieldWidth{300};
    inline constexpr int UiFormFieldHeight{48};

    inline constexpr int UiKeyboardTitleTextScale{5};
    inline constexpr int UiKeyboardValueTextScale{5};
    inline constexpr int UiKeyboardActionTextScale{4};
    inline constexpr int UiKeyboardHintTextScale{6};
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
