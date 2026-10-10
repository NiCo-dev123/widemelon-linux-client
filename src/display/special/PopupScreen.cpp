#include "display/special/PopupScreen.h"

#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/UiTheme.h"
#include "input/EvdevInput.h"

#include <array>

namespace widemelon::display
{
namespace
{
    constexpr int PopupSize = 680;
    constexpr std::array<std::string_view, 3> Choices{"Save", "Discard", "Cancel"};

    void render(SDL_Renderer *renderer, int width, int height, std::string_view message, int selected)
    {
        const ThemeTextSettings &text = uiTextSettings();
        const UiTextures &textures = uiTextures();
        drawGradientBackground(renderer, width, height);
        const SDL_Rect popup{(width - PopupSize) / 2, (height - PopupSize) / 2, PopupSize, PopupSize};
        drawPopupBackground(renderer, popup);
        drawTextColored(renderer, message, (width - textWidth(message, text.titleFontSize)) / 2, popup.y + 96,
                        text.titleFontSize, uiPalette().primary);
        constexpr int buttonWidth = 360;
        constexpr int buttonHeight = UiFormFieldHeight;
        constexpr int buttonStartY = 240;
        constexpr int buttonGap = 80;
        for (std::size_t index = 0; index < Choices.size(); ++index)
        {
            const SDL_Rect button{(width - buttonWidth) / 2, popup.y + buttonStartY + static_cast<int>(index) * buttonGap,
                                  buttonWidth, buttonHeight};
            drawPill(renderer, button, static_cast<int>(index) == selected);
            drawTextColored(renderer, Choices[index], button.x + (button.w - textWidth(Choices[index], text.formFontSize)) / 2,
                            button.y + (button.h - text.formFontSize) / 2, text.formFontSize, uiPalette().primary);
        }
        const int hintY = popup.y + popup.h - 72;
        const int total = controlHintWidth(textures.hintA, "A", "SELECT", text.hintFontSize)
            + controlHintWidth(textures.hintB, "B", "CANCEL", text.hintFontSize);
        int hintX = (width - total) / 2;
        hintX = drawControlHint(renderer, textures.hintA, "A", "SELECT", hintX, hintY, text.hintFontSize);
        drawControlHint(renderer, textures.hintB, "B", "CANCEL", hintX, hintY, text.hintFontSize);
        SDL_RenderPresent(renderer);
    }
}

PopupScreen::Result PopupScreen::saveChanges(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                             std::string_view message)
{
    int selected = 0;
    MenuNavigation navigation;
    while (true)
    {
        render(renderer, width, height, message, selected);
        input.pollEvent();
        if (input.exitComboPressed()) return Result::Exit;
        switch (navigation.nextAction(input.takeUiAction(), input.heldUiDirection()))
        {
        case UiAction::Up:
        case UiAction::Left:
            selected = (selected + static_cast<int>(Choices.size()) - 1) % static_cast<int>(Choices.size());
            break;
        case UiAction::Down:
        case UiAction::Right:
            selected = (selected + 1) % static_cast<int>(Choices.size());
            break;
        case UiAction::Delete:
        case UiAction::Back:
            return Result::Cancel;
        case UiAction::Start:
        case UiAction::Confirm:
            return selected == 0 ? Result::Save : selected == 1 ? Result::Discard : Result::Cancel;
        default:
            break;
        }
        SDL_Delay(10);
    }
}
}
