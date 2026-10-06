#pragma once

#include "display/PageDefinition.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace widemelon::display
{
    struct PageRenderContext
    {
        int width{0};
        int height{0};
        int itemGap{0};
        int titleFontSize{0};
        int textFontSize{0};
        int hintFontSize{0};

        std::function<void()> drawBackground;
        std::function<void(std::string_view, int, int, int, SDL_Color)> drawText;
        std::function<int(std::string_view, int)> textWidth;
    };

    struct MenuRenderItem
    {
        int top{0};
        int height{0};
    };

    struct MenuRenderContext
    {
        PageRenderContext page;
        SDL_Renderer *renderer{nullptr};
        int listTop{0};
        int listBottom{0};
        int scrollOffset{0};
        std::vector<MenuRenderItem> items;

        std::function<void()> renderHeader;
        std::function<void(std::size_t, int)> renderItem;
        std::function<void()> renderFooter;
        std::function<void()> present;
    };

    class PageRenderer
    {
    public:
        static int headerBottom(const PageRenderContext &context);
        static int firstItemY(const PageRenderContext &context);
        static int footerTop(const PageRenderContext &context);
        static void renderMenu(const MenuRenderContext &context);
    };
}
