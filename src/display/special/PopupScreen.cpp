#include "display/special/PopupScreen.h"

#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/UiTheme.h"
#include "input/EvdevInput.h"

#include <array>
#include <vector>

namespace widemelon::display
{
namespace
{
    constexpr int PopupSize = 680;
    struct Choice
    {
        std::string_view label;
        PopupScreen::Result result;
    };

    void render(SDL_Renderer *renderer, int width, int height, std::string_view message, const std::vector<Choice> &choices, int selected)
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
        for (std::size_t index = 0; index < choices.size(); ++index)
        {
            const SDL_Rect button{(width - buttonWidth) / 2, popup.y + buttonStartY + static_cast<int>(index) * buttonGap,
                                  buttonWidth, buttonHeight};
            drawPill(renderer, button, static_cast<int>(index) == selected);
            drawTextColored(renderer, choices[index].label, button.x + (button.w - textWidth(choices[index].label, text.formFontSize)) / 2,
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

    PopupScreen::Result show(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                             std::string_view message, const std::vector<Choice> &choices, int defaultChoice)
    {
        int selected = defaultChoice;
        MenuNavigation navigation;
        while (true)
        {
            render(renderer, width, height, message, choices, selected);
            input.pollEvent();
            if (input.exitComboPressed()) return PopupScreen::Result::Exit;
            switch (navigation.nextAction(input.takeUiAction(), input.heldUiDirection()))
            {
            case UiAction::Up:
            case UiAction::Left:
                selected = (selected + static_cast<int>(choices.size()) - 1) % static_cast<int>(choices.size());
                break;
            case UiAction::Down:
            case UiAction::Right:
                selected = (selected + 1) % static_cast<int>(choices.size());
                break;
            case UiAction::Delete:
            case UiAction::Back:
                return PopupScreen::Result::Cancel;
            case UiAction::Start:
            case UiAction::Confirm:
                return choices[static_cast<std::size_t>(selected)].result;
            default:
                break;
            }
            SDL_Delay(10);
        }
    }
}

PopupScreen::Result PopupScreen::saveChanges(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                             std::string_view message)
{
    return show(renderer, width, height, input, message,
                {{"Save", Result::Save}, {"Discard", Result::Discard}, {"Cancel", Result::Cancel}}, 0);
}

PopupScreen::Result PopupScreen::confirmDelete(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                               std::string_view message)
{
    return show(renderer, width, height, input, message,
                {{"Cancel", Result::Cancel}, {"Delete", Result::Delete}}, 0);
}

PopupScreen::Result PopupScreen::cannotDeleteLastPreset(SDL_Renderer *renderer, int width, int height, EvdevInput &input)
{
    return show(renderer, width, height, input, "Cannot delete the last preset.", {{"OK", Result::Acknowledge}}, 0);
}
}
