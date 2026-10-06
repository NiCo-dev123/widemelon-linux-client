#pragma once

#include "input/EvdevInput.h"

#include <chrono>
#include <vector>

namespace widemelon::display
{

    struct MenuNavigationItem
    {
        bool selectable{false};
        int top{0};
        int height{0};
    };

    class MenuNavigation
    {
    public:
        UiAction nextAction(UiAction action, UiAction held);

        static int nextSelection(const std::vector<MenuNavigationItem> &items, int selected, int direction);
        static int scrollOffsetForSelection(const std::vector<MenuNavigationItem> &items, int selected,
                                            int currentOffset, int viewportHeight);

    private:
        static bool isDirectional(UiAction action);

        UiAction repeatedAction{UiAction::None};
        std::chrono::steady_clock::time_point nextRepeatAt{};
        std::chrono::steady_clock::time_point nextNavigationAt{};
    };

}
