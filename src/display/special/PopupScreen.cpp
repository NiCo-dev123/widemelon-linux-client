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

PopupScreen::Result PopupScreen::acknowledge(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                             std::string_view message)
{
    return show(renderer, width, height, input, message, {{"OK", Result::Acknowledge}}, 0);
}

PopupScreen::Result PopupScreen::cannotDeleteLastPreset(SDL_Renderer *renderer, int width, int height, EvdevInput &input)
{
    return acknowledge(renderer, width, height, input, "Cannot delete the last preset.");
}

PopupScreen::MappingResult PopupScreen::editInputMapping(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                                          std::string_view inputName, InputMapping current)
{
    const std::string title = "Key mapping : " + std::string(inputName);
    const std::vector<std::string_view> outputLabels{"Disabled", "Key press", "Stylus press", "Stylus hold"};
    std::vector<std::string_view> buttonLabels;
    for (std::size_t index = 0; index <= static_cast<std::size_t>(DsButton::Start); ++index)
        buttonLabels.push_back(dsButtonName(static_cast<DsButton>(index)));
    int outputSelected = current.output == MappingOutput::KeyPress ? 1 : current.output == MappingOutput::StylusPress ? 2 : current.output == MappingOutput::StylusHold ? 3 : 0;
    int buttonSelected = static_cast<int>(current.button);
    int selectedField = 0;
    MenuNavigation navigation;
    const auto drawField = [&](const ThemeTextSettings &text, const SDL_Rect &field, std::string_view label,
                               std::string_view value, bool selected)
    {
        drawTextColored(renderer, label, (width - textWidth(label, text.formFontSize)) / 2, field.y - text.formFontSize - 12,
                        text.formFontSize, uiPalette().primary);
        drawPill(renderer, field, selected);
        const int textY = field.y + (field.h - text.formFontSize) / 2;
        drawTextColored(renderer, "<", field.x + 12, textY, text.formFontSize, uiPalette().primary);
        drawTextColored(renderer, ">", field.x + field.w - textWidth(">", text.formFontSize) - 12, textY, text.formFontSize, uiPalette().primary);
        drawTextColored(renderer, value, field.x + (field.w - textWidth(value, text.formFontSize)) / 2, textY, text.formFontSize, uiPalette().primary);
    };
    while (true)
    {
        const ThemeTextSettings &text = uiTextSettings();
        const UiTextures &textures = uiTextures();
        drawGradientBackground(renderer, width, height);
        const SDL_Rect popup{(width - PopupSize) / 2, (height - PopupSize) / 2, PopupSize, PopupSize};
        drawPopupBackground(renderer, popup);
        drawTextColored(renderer, title, (width - textWidth(title, text.titleFontSize)) / 2, popup.y + 72, text.titleFontSize, uiPalette().primary);
        const SDL_Rect outputField{(width - 440) / 2, popup.y + 190, 440, UiFormFieldHeight};
        drawField(text, outputField, "Output type", outputLabels[static_cast<std::size_t>(outputSelected)], selectedField == 0);
        const bool keyPress = outputSelected == static_cast<int>(MappingOutput::KeyPress);
        if (keyPress)
        {
            const SDL_Rect buttonField{(width - 440) / 2, popup.y + 360, 440, UiFormFieldHeight};
            drawField(text, buttonField, "Select output", buttonLabels[static_cast<std::size_t>(buttonSelected)], selectedField == 1);
        }
        const int hintY = popup.y + popup.h - 72;
        const int total = controlHintWidth(textures.hintA, "A", "SELECT", text.hintFontSize) + controlHintWidth(textures.hintB, "B", "CANCEL", text.hintFontSize);
        int hintX = (width - total) / 2;
        hintX = drawControlHint(renderer, textures.hintA, "A", "SELECT", hintX, hintY, text.hintFontSize);
        drawControlHint(renderer, textures.hintB, "B", "CANCEL", hintX, hintY, text.hintFontSize);
        SDL_RenderPresent(renderer);

        input.pollEvent();
        if (input.exitComboPressed()) return {false, true, current};
        const UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
        if (action == UiAction::Back || action == UiAction::Delete) return {false, false, current};
        if (action == UiAction::Up && keyPress) { selectedField = 0; continue; }
        if (action == UiAction::Down && keyPress) { selectedField = 1; continue; }
        if (action == UiAction::Left || action == UiAction::Right)
        {
            const int direction = action == UiAction::Left ? -1 : 1;
            if (selectedField == 0) outputSelected = (outputSelected + direction + static_cast<int>(outputLabels.size())) % static_cast<int>(outputLabels.size());
            else buttonSelected = (buttonSelected + direction + static_cast<int>(buttonLabels.size())) % static_cast<int>(buttonLabels.size());
            if (outputSelected != static_cast<int>(MappingOutput::KeyPress)) selectedField = 0;
            continue;
        }
        if (action == UiAction::Confirm || action == UiAction::Start)
        {
            current.output = static_cast<MappingOutput>(outputSelected);
            current.button = static_cast<DsButton>(buttonSelected);
            return {true, false, current};
        }
        SDL_Delay(10);
    }
}
}
