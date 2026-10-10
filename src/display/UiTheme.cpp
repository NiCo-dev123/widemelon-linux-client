#include "display/UiTheme.h"

#include "common/Logger.h"
#include "display/ConfigScreen.h"
#include "display/UiConfig.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>
#include <unordered_map>
#include <unistd.h>

#ifdef WIDEMELON_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
#ifdef WIDEMELON_HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

namespace widemelon::display
{
namespace
{
    Palette palette;
    ThemeTextSettings textSettings{
        UiTitleFontSize, UiGameFooterFontSize, UiGameplayStatusFontSize, UiHintFontSize,
        UiFormFontSize, UiKeyboardValueFontSize, UiKeyboardActionFontSize};
    UiTextures textures;

#ifdef WIDEMELON_HAVE_SDL_TTF
    std::string fontPath;
    std::map<int, TTF_Font *> fonts;

    TTF_Font *fontForSize(int size)
    {
        const auto existing = fonts.find(size);
        if (existing != fonts.end()) return existing->second;
        TTF_Font *font = TTF_OpenFont(fontPath.c_str(), size);
        // Cache failures too: retrying a damaged font every frame exhausts TSPS file descriptors.
        fonts.emplace(size, font);
        return font;
    }

    void closeFonts()
    {
        for (const auto &[size, font] : fonts)
        {
            (void)size;
            if (font) TTF_CloseFont(font);
        }
        fonts.clear();
    }
#endif

    bool parseColor(const std::string &value, SDL_Color &color)
    {
        if (value.size() != 7 || value.front() != '#') return false;
        unsigned int rgb = 0;
        const auto parsed = std::from_chars(value.data() + 1, value.data() + value.size(), rgb, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) return false;
        color = SDL_Color{static_cast<Uint8>((rgb >> 16) & 0xff), static_cast<Uint8>((rgb >> 8) & 0xff),
                          static_cast<Uint8>(rgb & 0xff), 255};
        return true;
    }

    bool parseInteger(const std::string &value, int &number)
    {
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
    }

    bool themeValue(const std::string &document, std::string_view key, std::string &value)
    {
        const std::string name = "\"" + std::string(key) + "\"";
        const std::size_t keyPosition = document.find(name);
        if (keyPosition == std::string::npos) return false;
        const std::size_t colon = document.find(':', keyPosition + name.size());
        const std::size_t start = colon == std::string::npos ? colon : document.find('"', colon + 1);
        const std::size_t end = start == std::string::npos ? start : document.find('"', start + 1);
        if (end == std::string::npos) return false;
        value = document.substr(start + 1, end - start - 1);
        return true;
    }

    bool themeInteger(const std::string &document, std::string_view key, int &value)
    {
        const std::string name = "\"" + std::string(key) + "\"";
        const std::size_t keyPosition = document.find(name);
        if (keyPosition == std::string::npos) return false;
        const std::size_t colon = document.find(':', keyPosition + name.size());
        if (colon == std::string::npos) return false;
        const std::size_t start = document.find_first_not_of(" \t\r\n", colon + 1);
        if (start == std::string::npos) return false;
        const std::size_t end = document.find_first_of(",} \t\r\n", start);
        return parseInteger(document.substr(start, (end == std::string::npos ? document.size() : end) - start), value);
    }

    void applyThemeConfig(const std::filesystem::path &directory, Palette &targetPalette,
                          std::string &font, ThemeTextSettings &targetText)
    {
        std::ifstream file(directory / "theme-config.json");
        if (!file) return;
        const std::string document((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const auto applyColor = [&](std::string_view key, SDL_Color &target)
        {
            std::string value;
            SDL_Color color{};
            if (themeValue(document, key, value) && parseColor(value, color)) target = color;
        };
        applyColor("background-dark", targetPalette.backgroundDark);
        applyColor("background-light", targetPalette.backgroundLight);
        applyColor("text-color", targetPalette.primary);
        applyColor("hint-color", targetPalette.hint);
        applyColor("button-fill", targetPalette.buttonFill);
        applyColor("button-outline", targetPalette.buttonOutline);
        const auto applySize = [&](std::string_view key, int &target)
        {
            int value = 0;
            if (themeInteger(document, key, value)) target = value;
        };
        applySize("title-font-size", targetText.titleFontSize);
        applySize("footer-font-size", targetText.footerFontSize);
        applySize("gameplay-status-font-size", targetText.gameplayStatusFontSize);
        applySize("hint-font-size", targetText.hintFontSize);
        applySize("form-font-size", targetText.formFontSize);
        applySize("keyboard-value-font-size", targetText.keyboardValueFontSize);
        applySize("keyboard-action-font-size", targetText.keyboardActionFontSize);
        std::string configuredFont;
        if (themeValue(document, "font", configuredFont) && !configuredFont.empty()) font = configuredFont;
    }

    void loadTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const std::string &path)
    {
#ifdef WIDEMELON_HAVE_SDL_IMAGE
        SDL_Surface *surface = IMG_Load(path.c_str());
        if (!surface)
        {
            Logger::error("Cannot load UI asset: " + path + "; " + IMG_GetError());
            return;
        }
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) Logger::error("Cannot create UI texture: " + path + "; " + SDL_GetError());
#else
        (void)renderer; (void)texture; (void)path;
#endif
    }

    void closeTextures()
    {
        SDL_DestroyTexture(textures.background); SDL_DestroyTexture(textures.gameplayBackground);
        SDL_DestroyTexture(textures.fieldSelected); SDL_DestroyTexture(textures.fieldUnselected);
        SDL_DestroyTexture(textures.numpadSelected); SDL_DestroyTexture(textures.numpadUnselected);
        SDL_DestroyTexture(textures.hintA); SDL_DestroyTexture(textures.hintB); SDL_DestroyTexture(textures.hintX);
        SDL_DestroyTexture(textures.hintStart); SDL_DestroyTexture(textures.hintL); SDL_DestroyTexture(textures.hintR);
        SDL_DestroyTexture(textures.cursor);
        textures = {};
    }

    int fallbackScale(int fontSize) { return std::max(1, (fontSize + 3) / 7); }
    using Glyph = std::array<std::uint8_t, 7>;
    const Glyph &glyphFor(char character)
    {
        static const std::unordered_map<char, Glyph> glyphs{
            {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}}, {'C',{14,17,16,16,16,17,14}},
            {'D',{30,17,17,17,17,17,30}}, {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
            {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}}, {'I',{14,4,4,4,4,4,14}},
            {'J',{1,1,1,1,17,17,14}}, {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
            {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}}, {'O',{14,17,17,17,17,17,14}},
            {'P',{30,17,17,30,16,16,16}}, {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
            {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}}, {'U',{17,17,17,17,17,17,14}},
            {'V',{17,17,17,17,17,10,4}}, {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
            {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
            {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}}, {'2',{14,17,1,2,4,8,31}},
            {'3',{30,1,1,14,1,1,30}}, {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
            {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}}, {'8',{14,17,17,14,17,17,14}},
            {'9',{14,17,17,15,1,1,14}}, {':',{0,4,4,0,4,4,0}}, {'.',{0,0,0,0,0,6,6}}, {' ',{0,0,0,0,0,0,0}}};
        static const Glyph fallback{31,17,21,21,21,17,31};
        const auto it = glyphs.find(character);
        return it == glyphs.end() ? fallback : it->second;
    }
}

Palette &uiPalette() { return palette; }
ThemeTextSettings &uiTextSettings() { return textSettings; }
UiTextures &uiTextures() { return textures; }

void loadUiResources(SDL_Renderer *renderer)
{
    std::array<char, 4096> executable{};
    const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
    if (length <= 0) return;
    executable[static_cast<std::size_t>(length)] = '\0';
    const std::string directory = std::string(executable.data()).substr(0, std::string(executable.data()).find_last_of('/'));
    UiConfig config(std::filesystem::path(directory) / "widemelon-client-ui.conf");
    std::ifstream file(directory + "/widemelon-client-ui.conf");
#ifdef WIDEMELON_HAVE_SDL_TTF
    std::string textFont = "assets/themes/DukuSlice/comfortaa-latin-400-normal.ttf";
#endif
    std::string activeTheme = config.readValue("active-theme").value_or("DukuSlice");
    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = line.substr(0, separator), value = line.substr(separator + 1);
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (key == "text-font") { if (!value.empty()) textFont = value; continue; }
#endif
        if (key == "button-outline-width") { int width = 0; if (parseInteger(value, width)) palette.buttonOutlineWidth = std::clamp(width, 1, 32); continue; }
        SDL_Color color{};
        if (!parseColor(value, color)) continue;
        if (key == "background-dark") palette.backgroundDark = color;
        else if (key == "background-light") palette.backgroundLight = color;
        else if (key == "text-color") palette.primary = color;
        else if (key == "hint-color") palette.hint = color;
        else if (key == "button-fill") palette.buttonFill = color;
        else if (key == "button-outline") palette.buttonOutline = color;
    }
    const std::filesystem::path themes = std::filesystem::path(directory) / "assets/themes";
    const auto validName = [](const std::string &name) { return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string::npos; };
    std::filesystem::path theme = validName(activeTheme) ? themes / activeTheme : std::filesystem::path{};
    std::error_code error;
    if (!std::filesystem::is_directory(theme, error))
    {
        if (activeTheme != "DukuSlice") Logger::error("UI theme not found: " + activeTheme + "; using DukuSlice");
        theme = themes / "DukuSlice";
        error.clear();
    }
    const std::filesystem::path fallback = themes / "DukuSlice";
    if (!std::filesystem::is_directory(theme, error)) { Logger::error("Default UI theme not found; using square UI fallbacks"); return; }
    std::string fallbackFont;
    applyThemeConfig(fallback, palette, fallbackFont, textSettings);
    std::string selectedFont = fallbackFont;
    if (theme != fallback) applyThemeConfig(theme, palette, selectedFont, textSettings);
#ifdef WIDEMELON_HAVE_SDL_TTF
    const auto resolveFont = [](const std::filesystem::path &root, const std::string &name)
    {
        const std::filesystem::path configured(name);
        if (name.empty() || configured.is_absolute()) return std::filesystem::path{};
        const std::filesystem::path candidate = (root.lexically_normal() / configured).lexically_normal();
        const std::filesystem::path relative = candidate.lexically_relative(root.lexically_normal());
        for (const auto &part : relative) if (part == "..") return std::filesystem::path{};
        std::error_code fsError;
        return !relative.empty() && std::filesystem::is_regular_file(candidate, fsError) ? candidate : std::filesystem::path{};
    };
    std::filesystem::path selected = resolveFont(theme, selectedFont);
    if (selected.empty()) selected = resolveFont(fallback, fallbackFont);
    if (!selected.empty()) fontPath = selected.string();
    else { fontPath = !textFont.empty() && textFont.front() == '/' ? textFont : directory + "/" + textFont; Logger::error("Theme font not found; using configured font fallback"); }
#endif
    const std::string assets = theme.string() + "/";
    loadTexture(renderer, textures.background, assets + "backgrounds/background.png");
    loadTexture(renderer, textures.gameplayBackground, assets + "backgrounds/background-gameplay.png");
    loadTexture(renderer, textures.fieldSelected, assets + "icons/field-input-selected.png");
    loadTexture(renderer, textures.fieldUnselected, assets + "icons/field-input-unselected.png");
    loadTexture(renderer, textures.numpadSelected, assets + "icons/numpad-selected.png");
    loadTexture(renderer, textures.numpadUnselected, assets + "icons/numpad-unselected.png");
    loadTexture(renderer, textures.hintA, assets + "icons/hint-A.png"); loadTexture(renderer, textures.hintB, assets + "icons/hint-B.png");
    loadTexture(renderer, textures.hintX, assets + "icons/hint-X.png"); loadTexture(renderer, textures.hintStart, assets + "icons/hint-START.png");
    loadTexture(renderer, textures.hintL, assets + "icons/hint-L.png"); loadTexture(renderer, textures.hintR, assets + "icons/hint-R.png");
    loadTexture(renderer, textures.cursor, directory + "/assets/icons/pointer.png");
}

void closeUiResources()
{
    closeTextures();
#ifdef WIDEMELON_HAVE_SDL_TTF
    closeFonts();
#endif
}

void reloadUiResources(SDL_Renderer *renderer)
{
    closeUiResources();
    palette = Palette{};
    textSettings = {UiTitleFontSize, UiGameFooterFontSize, UiGameplayStatusFontSize, UiHintFontSize,
                    UiFormFontSize, UiKeyboardValueFontSize, UiKeyboardActionFontSize};
    loadUiResources(renderer);
}

bool loadUiFont(int size)
{
#ifdef WIDEMELON_HAVE_SDL_TTF
    return fontForSize(size) != nullptr;
#else
    (void)size;
    return false;
#endif
}

const std::string &uiFontPath()
{
#ifdef WIDEMELON_HAVE_SDL_TTF
    return fontPath;
#else
    static const std::string empty;
    return empty;
#endif
}

void drawBackground(SDL_Renderer *renderer, int width, int height, bool gameplay)
{
    SDL_Texture *texture = gameplay ? textures.gameplayBackground : textures.background;
    if (texture) { SDL_Rect destination{0, 0, width, height}; SDL_RenderCopy(renderer, texture, nullptr, &destination); return; }
    for (int y = 0; y < height; ++y)
    {
        const int ratio = height > 1 ? y * 255 / (height - 1) : 0;
        const auto blend = [ratio](Uint8 light, Uint8 dark) { return static_cast<Uint8>((light * (255 - ratio) + dark * ratio) / 255); };
        SDL_SetRenderDrawColor(renderer, blend(palette.backgroundLight.r, palette.backgroundDark.r),
            blend(palette.backgroundLight.g, palette.backgroundDark.g), blend(palette.backgroundLight.b, palette.backgroundDark.b), 255);
        SDL_RenderDrawLine(renderer, 0, y, width, y);
    }
}

void drawGradientBackground(SDL_Renderer *renderer, int width, int height) { drawBackground(renderer, width, height, false); }

void drawPill(SDL_Renderer *renderer, const SDL_Rect &rect, bool selected, bool numpad)
{
#ifdef WIDEMELON_HAVE_SDL_IMAGE
    SDL_Texture *texture = numpad ? (selected ? textures.numpadSelected : textures.numpadUnselected)
                                  : (selected ? textures.fieldSelected : textures.fieldUnselected);
    if (texture) { SDL_RenderCopy(renderer, texture, nullptr, &rect); return; }
#endif
    if (selected) { SDL_SetRenderDrawColor(renderer, palette.buttonFill.r, palette.buttonFill.g, palette.buttonFill.b, 255); SDL_RenderFillRect(renderer, &rect); }
    SDL_SetRenderDrawColor(renderer, palette.buttonOutline.r, palette.buttonOutline.g, palette.buttonOutline.b, 255);
    for (int thickness = 0; thickness < palette.buttonOutlineWidth; ++thickness)
    {
        SDL_Rect outline{rect.x + thickness, rect.y + thickness, rect.w - thickness * 2, rect.h - thickness * 2};
        if (outline.w <= 0 || outline.h <= 0) break;
        SDL_RenderDrawRect(renderer, &outline);
    }
}

int textWidth(std::string_view text, int fontSize)
{
#ifdef WIDEMELON_HAVE_SDL_TTF
    int width = 0;
    if (TTF_Font *font = fontForSize(fontSize)) { TTF_SizeUTF8(font, std::string(text).c_str(), &width, nullptr); return width; }
#endif
    const int scale = fallbackScale(fontSize);
    return static_cast<int>(text.size()) * 6 * scale - scale;
}

void drawText(SDL_Renderer *renderer, std::string_view text, int x, int y, int fontSize)
{
#ifdef WIDEMELON_HAVE_SDL_TTF
    if (TTF_Font *font = fontForSize(fontSize))
    {
        const std::string rendered(text);
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, rendered.c_str(), palette.primary);
        if (surface) { SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface); SDL_Rect destination{x, y, surface->w, surface->h}; if (texture) SDL_RenderCopy(renderer, texture, nullptr, &destination); SDL_DestroyTexture(texture); SDL_FreeSurface(surface); return; }
    }
#endif
    const int scale = fallbackScale(fontSize);
    for (char character : text)
    {
        const Glyph &glyph = glyphFor(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
        for (int row = 0; row < 7; ++row) for (int column = 0; column < 5; ++column)
            if (glyph[row] & (1U << (4 - column))) { SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale}; SDL_RenderFillRect(renderer, &pixel); }
        x += 6 * scale;
    }
}

void drawTextColored(SDL_Renderer *renderer, std::string_view text, int x, int y, int fontSize, SDL_Color color)
{
#ifdef WIDEMELON_HAVE_SDL_TTF
    if (TTF_Font *font = fontForSize(fontSize))
    {
        const std::string rendered(text);
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, rendered.c_str(), color);
        if (surface) { SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface); SDL_Rect destination{x, y, surface->w, surface->h}; if (texture) SDL_RenderCopy(renderer, texture, nullptr, &destination); SDL_DestroyTexture(texture); SDL_FreeSurface(surface); return; }
    }
#endif
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    drawText(renderer, text, x, y, fontSize);
    SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
}

int controlHintWidth(SDL_Texture *icon, std::string_view fallback, std::string_view label, int fontSize)
{
    constexpr int iconHeight = 48;
    int iconWidth = textWidth(fallback, fontSize);
    if (icon) { int sourceWidth = 0, sourceHeight = 0; SDL_QueryTexture(icon, nullptr, nullptr, &sourceWidth, &sourceHeight); iconWidth = sourceHeight > 0 ? sourceWidth * iconHeight / sourceHeight : iconHeight; }
    return iconWidth + UiHintIconTextGap + textWidth(label, fontSize) + 20;
}

int drawControlHint(SDL_Renderer *renderer, SDL_Texture *icon, std::string_view fallback, std::string_view label, int x, int y, int fontSize)
{
    constexpr int iconHeight = 48;
    int iconWidth = textWidth(fallback, fontSize);
    if (icon) { int sourceWidth = 0, sourceHeight = 0; SDL_QueryTexture(icon, nullptr, nullptr, &sourceWidth, &sourceHeight); iconWidth = sourceHeight > 0 ? sourceWidth * iconHeight / sourceHeight : iconHeight; SDL_Rect destination{x, y, iconWidth, iconHeight}; SDL_RenderCopy(renderer, icon, nullptr, &destination); }
    else drawTextColored(renderer, fallback, x, y + (iconHeight - fontSize) / 2, fontSize, palette.hint);
    const int labelX = x + iconWidth + UiHintIconTextGap;
    drawTextColored(renderer, label, labelX, y + (iconHeight - fontSize) / 2, fontSize, palette.hint);
    return labelX + textWidth(label, fontSize) + 20;
}

}
