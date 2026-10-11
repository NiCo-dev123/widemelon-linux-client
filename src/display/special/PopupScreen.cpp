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
    const auto select = [&](std::string_view caption, const std::vector<std::string_view> &labels, int selected) -> int
    {
        MenuNavigation navigation;
        while (true)
        {
            const ThemeTextSettings &text = uiTextSettings();
            const UiTextures &textures = uiTextures();
            drawGradientBackground(renderer, width, height);
            const SDL_Rect popup{(width - PopupSize) / 2, (height - PopupSize) / 2, PopupSize, PopupSize};
            drawPopupBackground(renderer, popup);
            drawTextColored(renderer, title, (width - textWidth(title, text.titleFontSize)) / 2, popup.y + 96, text.titleFontSize, uiPalette().primary);
            drawTextColored(renderer, caption, (width - textWidth(caption, text.formFontSize)) / 2, popup.y + 190, text.formFontSize, uiPalette().primary);
            const SDL_Rect field{(width - 440) / 2, popup.y + 250, 440, UiFormFieldHeight};
            drawPill(renderer, field, true);
            const int textY = field.y + (field.h - text.formFontSize) / 2;
            drawTextColored(renderer, "<", field.x + 12, textY, text.formFontSize, uiPalette().primary);
            drawTextColored(renderer, ">", field.x + field.w - textWidth(">", text.formFontSize) - 12, textY, text.formFontSize, uiPalette().primary);
            const std::string_view label = labels[static_cast<std::size_t>(selected)];
            drawTextColored(renderer, label, field.x + (field.w - textWidth(label, text.formFontSize)) / 2, textY, text.formFontSize, uiPalette().primary);
            const int hintY = popup.y + popup.h - 72;
            const int total = controlHintWidth(textures.hintA, "A", "SELECT", text.hintFontSize) + controlHintWidth(textures.hintB, "B", "CANCEL", text.hintFontSize);
            int hintX = (width - total) / 2;
            hintX = drawControlHint(renderer, textures.hintA, "A", "SELECT", hintX, hintY, text.hintFontSize);
            drawControlHint(renderer, textures.hintB, "B", "CANCEL", hintX, hintY, text.hintFontSize);
            SDL_RenderPresent(renderer);
            input.pollEvent();
            if (input.exitComboPressed()) return -2;
            switch (navigation.nextAction(input.takeUiAction(), input.heldUiDirection()))
            {
            case UiAction::Left: selected = (selected + static_cast<int>(labels.size()) - 1) % static_cast<int>(labels.size()); break;
            case UiAction::Right: selected = (selected + 1) % static_cast<int>(labels.size()); break;
            case UiAction::Back:
            case UiAction::Delete: return -1;
            case UiAction::Confirm:
            case UiAction::Start: return selected;
            default: break;
            }
            SDL_Delay(10);
        }
    };

    const std::vector<std::string_view> outputLabels{"Disabled", "Key press", "Stylus press", "Stylus hold"};
    int outputSelected = current.output == MappingOutput::KeyPress ? 1 : current.output == MappingOutput::StylusPress ? 2 : current.output == MappingOutput::StylusHold ? 3 : 0;
    const int output = select("Output type", outputLabels, outputSelected);
    if (output == -2) return {false, true, current};
    if (output < 0) return {false, false, current};
    current.output = static_cast<MappingOutput>(output);
    if (current.output != MappingOutput::KeyPress) return {true, false, current};

    std::vector<std::string_view> buttonLabels;
    for (std::size_t index = 0; index <= static_cast<std::size_t>(DsButton::Start); ++index)
        buttonLabels.push_back(dsButtonName(static_cast<DsButton>(index)));
    const int button = select("DS button", buttonLabels, static_cast<int>(current.button));
    if (button == -2) return {false, true, current};
    if (button < 0) return {false, false, current};
    current.button = static_cast<DsButton>(button);
    return {true, false, current};
}
}
