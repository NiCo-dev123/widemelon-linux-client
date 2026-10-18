#include "display/special/PopupScreen.h"

#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/UiTheme.h"
#include "input/EvdevInput.h"
#include "network/WebSocketClient.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

void PopupScreen::renderConnection(SDL_Renderer *renderer, int width, int height, const ConnectionProgress &progress)
{
    const ThemeTextSettings &text = uiTextSettings();
    const UiTextures &textures = uiTextures();
    const SDL_Rect popup{(width - PopupSize) / 2, (height - PopupSize) / 2, PopupSize, PopupSize};
    drawPopupBackground(renderer, popup);
    const auto centered = [&](std::string_view value, int y, int size, SDL_Color color)
    {
        drawTextColored(renderer, value, (width - textWidth(value, size)) / 2, y, size, color);
    };
    centered("Connection", popup.y + 70, text.titleFontSize, uiPalette().primary);
    centered(progress.status, popup.y + 150, text.formFontSize, uiPalette().primary);
    std::size_t start = 0;
    int line = 0;
    while (start < progress.error.size())
    {
        const std::size_t end = progress.error.find('\n', start);
        const std::string_view message(progress.error.data() + start,
            (end == std::string::npos ? progress.error.size() : end) - start);
        centered(message, popup.y + 230 + line * (text.formFontSize + 12), text.formFontSize, uiPalette().primary);
        ++line;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (!progress.serverCode.empty())
        centered("Server code: " + progress.serverCode, popup.y + 340, text.formFontSize, uiPalette().hint);
    const SDL_Rect button{(width - 360) / 2, popup.y + 475, 360, UiFormFieldHeight};
    drawPill(renderer, button, true);
    centered("OK", button.y + (button.h - text.formFontSize) / 2, text.formFontSize, uiPalette().primary);
    const int hintY = popup.y + popup.h - 72;
    const int total = controlHintWidth(textures.hintA, "A", "SELECT", text.hintFontSize)
        + controlHintWidth(textures.hintB, "B", "BACK", text.hintFontSize);
    int hintX = (width - total) / 2;
    hintX = drawControlHint(renderer, textures.hintA, "A", "SELECT", hintX, hintY, text.hintFontSize);
    drawControlHint(renderer, textures.hintB, "B", "BACK", hintX, hintY, text.hintFontSize);
    SDL_RenderPresent(renderer);
}

PopupScreen::MappingResult PopupScreen::editInputMapping(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                                          std::string_view inputName, InputMapping current)
{
    const std::string title = "Key mapping : " + std::string(inputName);
    const std::vector<std::string_view> outputLabels{"Disabled", "Key press", "Stylus press", "Stylus hold", "Screen press"};
    std::vector<std::string_view> buttonLabels;
    for (std::size_t index = 0; index <= static_cast<std::size_t>(DsButton::Start); ++index)
        buttonLabels.push_back(dsButtonName(static_cast<DsButton>(index)));
    int outputSelected = current.output == MappingOutput::KeyPress ? 1 : current.output == MappingOutput::StylusPress ? 2
        : current.output == MappingOutput::StylusHold ? 3 : current.output == MappingOutput::ScreenPress ? 4 : 0;
    int buttonSelected = static_cast<int>(current.button);
    int screenX = current.screenX;
    int screenY = current.screenY;
    int screenPressReleaseDelayFrames = current.screenPressReleaseDelayFrames;
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
        const SDL_Rect outputField{(width - 440) / 2, popup.y + 150, 440, UiFormFieldHeight};
        drawField(text, outputField, "Output type", outputLabels[static_cast<std::size_t>(outputSelected)], selectedField == 0);
        const bool keyPress = outputSelected == static_cast<int>(MappingOutput::KeyPress);
        const bool screenPress = outputSelected == static_cast<int>(MappingOutput::ScreenPress);
        if (keyPress)
        {
            const SDL_Rect buttonField{(width - 440) / 2, popup.y + 330, 440, UiFormFieldHeight};
            drawField(text, buttonField, "Select output", buttonLabels[static_cast<std::size_t>(buttonSelected)], selectedField == 1);
        }
        if (screenPress)
        {
            const SDL_Rect xField{(width - 440) / 2, popup.y + 290, 440, UiFormFieldHeight};
            const SDL_Rect yField{(width - 440) / 2, popup.y + 410, 440, UiFormFieldHeight};
            const SDL_Rect delayField{(width - 440) / 2, popup.y + 530, 440, UiFormFieldHeight};
            drawField(text, xField, "X coordinate", std::to_string(screenX), selectedField == 1);
            drawField(text, yField, "Y coordinate", std::to_string(screenY), selectedField == 2);
            drawField(text, delayField, "Release delay", std::to_string(screenPressReleaseDelayFrames) + " frames", selectedField == 3);
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
        const int fieldCount = keyPress ? 2 : screenPress ? 4 : 1;
        if (action == UiAction::Up) { selectedField = std::max(0, selectedField - 1); continue; }
        if (action == UiAction::Down) { selectedField = std::min(fieldCount - 1, selectedField + 1); continue; }
        const bool normalAdjustment = action == UiAction::Left || action == UiAction::Right;
        const bool fastRangeAdjustment = action == UiAction::L || action == UiAction::R;
        if (normalAdjustment || fastRangeAdjustment)
        {
            const int direction = (action == UiAction::Left || action == UiAction::L) ? -1 : 1;
            const int stepMultiplier = fastRangeAdjustment ? UiRangeFastStepMultiplier : 1;
            if (selectedField == 0 && normalAdjustment) outputSelected = (outputSelected + direction + static_cast<int>(outputLabels.size())) % static_cast<int>(outputLabels.size());
            else if (keyPress && normalAdjustment) buttonSelected = (buttonSelected + direction + static_cast<int>(buttonLabels.size())) % static_cast<int>(buttonLabels.size());
            else if (screenPress && selectedField == 1) screenX = std::clamp(screenX + direction * stepMultiplier, 0, 255);
            else if (screenPress && selectedField == 2) screenY = std::clamp(screenY + direction * stepMultiplier, 0, 191);
            else if (screenPress && selectedField == 3) screenPressReleaseDelayFrames = std::clamp(screenPressReleaseDelayFrames + direction * stepMultiplier,
                static_cast<int>(ScreenPressReleaseDelayFramesMinimum), static_cast<int>(ScreenPressReleaseDelayFramesMaximum));
            const int updatedFieldCount = outputSelected == static_cast<int>(MappingOutput::KeyPress) ? 2
                : outputSelected == static_cast<int>(MappingOutput::ScreenPress) ? 4 : 1;
            selectedField = std::min(selectedField, updatedFieldCount - 1);
            continue;
        }
        if (action == UiAction::Confirm || action == UiAction::Start)
        {
            current.output = static_cast<MappingOutput>(outputSelected);
            current.button = static_cast<DsButton>(buttonSelected);
            current.screenX = static_cast<std::uint16_t>(screenX);
            current.screenY = static_cast<std::uint16_t>(screenY);
            current.screenPressReleaseDelayFrames = static_cast<std::uint8_t>(screenPressReleaseDelayFrames);
            return {true, false, current};
        }
        SDL_Delay(10);
    }
}
}
