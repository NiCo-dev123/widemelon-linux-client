#pragma once

#include "display/PageDefinition.h"

#include <SDL.h>

#include <functional>
#include <string>

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

    class PageRenderer
    {
    public:
        static int headerBottom(const PageRenderContext &context);
        static int firstItemY(const PageRenderContext &context);
        static int footerTop(const PageRenderContext &context);
    };
}
