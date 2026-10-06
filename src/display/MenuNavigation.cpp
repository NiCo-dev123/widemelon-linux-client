#include "display/MenuNavigation.h"

#include "display/ConfigScreen.h"

#include <algorithm>

namespace widemelon::display
{

    bool MenuNavigation::isDirectional(UiAction action)
    {
        return action == UiAction::Up || action == UiAction::Down
            || action == UiAction::Left || action == UiAction::Right;
    }

    UiAction MenuNavigation::nextAction(UiAction action, UiAction held)
    {
        const auto now = std::chrono::steady_clock::now();
        UiAction result = action;

        if (isDirectional(action))
        {
            repeatedAction = action;
            nextRepeatAt = now + UiNavigationInitialDelay;
        }
        else if (!isDirectional(held))
        {
            repeatedAction = UiAction::None;
        }
        else if (held != repeatedAction)
        {
            repeatedAction = held;
            nextRepeatAt = now + UiNavigationInitialDelay;
            result = held;
        }
        else if (now >= nextRepeatAt)
        {
            nextRepeatAt = now + UiNavigationRetriggerDelay;
            result = held;
        }
        else if (isDirectional(held))
        {
            result = UiAction::None;
        }

        if (!isDirectional(result)) return result;
        if (now < nextNavigationAt) return UiAction::None;
        nextNavigationAt = now + UiNavigationCooldown;
        return result;
    }

    int MenuNavigation::nextSelection(const std::vector<MenuNavigationItem> &items, int selected, int direction)
    {
        if (items.empty() || direction == 0) return selected;
        const int count = static_cast<int>(items.size());
        for (int attempts = 0; attempts < count; ++attempts)
        {
            selected = (selected + direction + count) % count;
            if (items[static_cast<std::size_t>(selected)].selectable) return selected;
        }
        return selected;
    }

    int MenuNavigation::scrollOffsetForSelection(const std::vector<MenuNavigationItem> &items, int selected,
                                                  int currentOffset, int viewportHeight)
    {
        if (items.empty() || selected < 0 || selected >= static_cast<int>(items.size()) || viewportHeight <= 0)
            return 0;

        const MenuNavigationItem &item = items[static_cast<std::size_t>(selected)];
        const auto firstSelectable = std::find_if(items.begin(), items.end(), [](const MenuNavigationItem &candidate)
        {
            return candidate.selectable;
        });
        if (firstSelectable != items.end() && &item == &*firstSelectable)
            currentOffset = 0;
        else if (item.top < currentOffset) currentOffset = item.top;
        else if (item.top + item.height > currentOffset + viewportHeight)
            currentOffset = item.top + item.height - viewportHeight;

        const MenuNavigationItem &last = items.back();
        const int contentHeight = last.top + last.height;
        return std::clamp(currentOffset, 0, std::max(0, contentHeight - viewportHeight));
    }

}
