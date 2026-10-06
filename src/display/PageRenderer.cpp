#include "display/PageRenderer.h"

namespace widemelon::display
{
    int PageRenderer::headerBottom(const PageRenderContext &context)
    {
        constexpr int headerTop = 36;
        return headerTop + context.titleFontSize;
    }

    int PageRenderer::firstItemY(const PageRenderContext &context)
    {
        return headerBottom(context) + context.itemGap;
    }

    int PageRenderer::footerTop(const PageRenderContext &context)
    {
        return context.height - 64;
    }

    void PageRenderer::renderMenu(const MenuRenderContext &context)
    {
        if (context.page.drawBackground) context.page.drawBackground();
        if (context.renderHeader) context.renderHeader();

        const SDL_Rect clip{0, context.listTop, context.page.width, context.listBottom - context.listTop};
        SDL_RenderSetClipRect(context.renderer, &clip);
        for (std::size_t index = 0; index < context.items.size(); ++index)
        {
            const MenuRenderItem &item = context.items[index];
            const int rowY = context.listTop + item.top - context.scrollOffset;
            if (rowY + item.height < context.listTop || rowY > context.listBottom) continue;
            if (context.renderItem) context.renderItem(index, rowY);
        }
        SDL_RenderSetClipRect(context.renderer, nullptr);

        if (context.renderFooter) context.renderFooter();
        if (context.present) context.present();
    }
}
