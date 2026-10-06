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
}
