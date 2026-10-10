#pragma once

#include <SDL.h>

#include <string>
#include <string_view>

namespace widemelon::display
{

    struct Palette
    {
        SDL_Color backgroundDark{204, 51, 51, 255};
        SDL_Color backgroundLight{255, 80, 80, 255};
        SDL_Color primary{51, 12, 0, 255};
        SDL_Color hint{255, 114, 114, 255};
        SDL_Color buttonFill{255, 114, 114, 255};
        SDL_Color buttonOutline{98, 213, 93, 255};
        int buttonOutlineWidth{6};
    };

    struct ThemeTextSettings
    {
        int titleFontSize;
        int footerFontSize;
        int gameplayStatusFontSize;
        int hintFontSize;
        int formFontSize;
        int keyboardValueFontSize;
        int keyboardActionFontSize;
    };

    struct UiTextures
    {
        SDL_Texture *background = nullptr;
        SDL_Texture *gameplayBackground = nullptr;
        SDL_Texture *fieldSelected = nullptr;
        SDL_Texture *fieldUnselected = nullptr;
        SDL_Texture *numpadSelected = nullptr;
        SDL_Texture *numpadUnselected = nullptr;
        SDL_Texture *keyboardSelected = nullptr;
        SDL_Texture *keyboardUnselected = nullptr;
        SDL_Texture *hintA = nullptr;
        SDL_Texture *hintB = nullptr;
        SDL_Texture *hintX = nullptr;
        SDL_Texture *hintY = nullptr;
        SDL_Texture *hintStart = nullptr;
        SDL_Texture *hintL = nullptr;
        SDL_Texture *hintR = nullptr;
        SDL_Texture *cursor = nullptr;
    };

    Palette &uiPalette();
    ThemeTextSettings &uiTextSettings();
    UiTextures &uiTextures();

    void loadUiResources(SDL_Renderer *renderer);
    void reloadUiResources(SDL_Renderer *renderer);
    void closeUiResources();
    bool loadUiFont(int size);
    const std::string &uiFontPath();

    void drawBackground(SDL_Renderer *renderer, int width, int height, bool gameplay);
    void drawGradientBackground(SDL_Renderer *renderer, int width, int height);
    void drawPill(SDL_Renderer *renderer, const SDL_Rect &rect, bool selected, bool numpad = false);
    void drawKeyboardKey(SDL_Renderer *renderer, const SDL_Rect &rect, bool selected);
    int textWidth(std::string_view text, int fontSize);
    int symbolWidth(std::string_view symbol, int fontSize);
    void drawText(SDL_Renderer *renderer, std::string_view text, int x, int y, int fontSize);
    void drawTextColored(SDL_Renderer *renderer, std::string_view text, int x, int y, int fontSize, SDL_Color color);
    void drawSymbol(SDL_Renderer *renderer, std::string_view symbol, int x, int y, int fontSize, SDL_Color color);
    int controlHintWidth(SDL_Texture *icon, std::string_view fallback, std::string_view label, int fontSize);
    int drawControlHint(SDL_Renderer *renderer, SDL_Texture *icon, std::string_view fallback,
                        std::string_view label, int x, int y, int fontSize);

}
