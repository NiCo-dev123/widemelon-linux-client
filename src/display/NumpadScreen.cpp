#include "display/NumpadScreen.h"

#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/UiTheme.h"
#include "display/pages/NumpadPage.h"
#include "input/EvdevInput.h"

#include <array>
#include <string_view>

namespace widemelon::display
{
namespace
{
    constexpr int KeyCount = 13;
    constexpr int ColumnCount = 4;
    const std::array<const char *, KeyCount> Keys{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};

    void render(SDL_Renderer *renderer, int width, int height, const std::string &title,
                const std::string &value, int selectedKey)
    {
        const PageDefinition page = pages::numpadPage(title);
        const ThemeTextSettings &text = uiTextSettings();
        const UiTextures &textures = uiTextures();
        const int keyWidth = UiKeyboardKeyWidth;
        const int keyHeight = UiKeyboardKeyHeight;

        drawGradientBackground(renderer, width, height);
        SDL_SetRenderDrawColor(renderer, uiPalette().primary.r, uiPalette().primary.g, uiPalette().primary.b, 255);
        drawText(renderer, page.title, (width - textWidth(page.title, text.titleFontSize)) / 2, 65, text.titleFontSize);
        const std::string_view displayed = value.empty() ? "_" : value;
        drawText(renderer, displayed, (width - textWidth(displayed, text.keyboardValueFontSize)) / 2, 125, text.keyboardValueFontSize);
        for (std::size_t index = 0; index < Keys.size(); ++index)
        {
            const int row = static_cast<int>(index) / ColumnCount;
            const int column = static_cast<int>(index) % ColumnCount;
            const SDL_Rect key{UiKeyboardStartX + column * keyWidth, UiKeyboardStartY + row * keyHeight, keyWidth - 10, keyHeight - 8};
            drawPill(renderer, key, static_cast<int>(index) == selectedKey, true);
            const int fontSize = std::string_view(Keys[index]).size() > 1 ? text.keyboardActionFontSize : text.keyboardValueFontSize;
            drawText(renderer, Keys[index], key.x + (key.w - textWidth(Keys[index], fontSize)) / 2,
                     key.y + (key.h - fontSize) / 2, fontSize);
        }
        int hintX = width / 2 - 190;
        const int hintY = height - 64;
        hintX = drawControlHint(renderer, textures.hintA, "A", page.footer[0].text, hintX, hintY, text.hintFontSize);
        hintX = drawControlHint(renderer, textures.hintB, "B", page.footer[1].text, hintX, hintY, text.hintFontSize);
        hintX = drawControlHint(renderer, textures.hintX, "X", page.footer[2].text, hintX, hintY, text.hintFontSize);
        drawControlHint(renderer, textures.hintStart, "START", page.footer[3].text, hintX, hintY, text.hintFontSize);
        SDL_RenderPresent(renderer);
    }
}

bool NumpadScreen::edit(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                        const std::string &title, std::string &value, std::size_t maximumLength, bool allowDot)
{
    const std::string original = value;
    int selectedKey = 0;
    MenuNavigation navigation;
    while (true)
    {
        render(renderer, width, height, title, value, selectedKey);
        input.pollEvent();
        if (input.exitComboPressed()) return false;
        switch (navigation.nextAction(input.takeUiAction(), input.heldUiDirection()))
        {
        case UiAction::Up: selectedKey = (selectedKey + KeyCount - ColumnCount) % KeyCount; break;
        case UiAction::Down: selectedKey = (selectedKey + ColumnCount) % KeyCount; break;
        case UiAction::Left: selectedKey = (selectedKey + KeyCount - 1) % KeyCount; break;
        case UiAction::Right: selectedKey = (selectedKey + 1) % KeyCount; break;
        case UiAction::Delete: if (!value.empty()) value.pop_back(); break;
        case UiAction::Back: value = original; return false;
        case UiAction::Start: return true;
        case UiAction::Confirm:
        {
            const std::string_view key = Keys[static_cast<std::size_t>(selectedKey)];
            if (key == "OK") return true;
            if (key == "DEL") { if (!value.empty()) value.pop_back(); }
            else if (value.size() < maximumLength && (key != "." || allowDot)) value.append(key);
            break;
        }
        default: break;
        }
        SDL_Delay(10);
    }
}

}
