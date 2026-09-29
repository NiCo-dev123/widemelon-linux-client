#include "display/ConfigScreen.h"

#include "common/Logger.h"
#include "input/CursorStickMod.h"
#include "input/EvdevInput.h"
#include "input/Sm64ManualStickMod.h"
#include "input/Sm64StickMod.h"
#include "network/WebSocketClient.h"

#include <SDL.h>
#ifdef WIDEMELON_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
#ifdef WIDEMELON_HAVE_SDL_TTF
#include <SDL_ttf.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <future>
#include <iterator>
#include <fstream>
#include <filesystem>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace
{

    using Glyph = std::array<std::uint8_t, 7>;

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

    Palette palette;

    struct ThemeTextSettings
    {
        int titleFontSize{widemelon::UiTitleFontSize};
        int footerFontSize{widemelon::UiGameFooterFontSize};
        int gameplayStatusFontSize{widemelon::UiGameplayStatusFontSize};
        int hintFontSize{widemelon::UiHintFontSize};
        int formFontSize{widemelon::UiFormFontSize};
        int keyboardValueFontSize{widemelon::UiKeyboardValueFontSize};
        int keyboardActionFontSize{widemelon::UiKeyboardActionFontSize};
    };

    ThemeTextSettings themeText;

    struct UiTextures
    {
        SDL_Texture *background = nullptr;
        SDL_Texture *gameplayBackground = nullptr;
        SDL_Texture *fieldSelected = nullptr;
        SDL_Texture *fieldUnselected = nullptr;
        SDL_Texture *keyboardSelected = nullptr;
        SDL_Texture *keyboardUnselected = nullptr;
        SDL_Texture *hintA = nullptr;
        SDL_Texture *hintB = nullptr;
        SDL_Texture *hintX = nullptr;
        SDL_Texture *hintStart = nullptr;
        SDL_Texture *hintL = nullptr;
        SDL_Texture *hintR = nullptr;
        SDL_Texture *cursor = nullptr;
    };

    UiTextures uiTextures;

#ifdef WIDEMELON_HAVE_SDL_TTF
    std::string fontPath;
    std::map<int, TTF_Font *> fonts;

    TTF_Font *fontForSize(int size)
    {
        const auto existing = fonts.find(size);
        if (existing != fonts.end())
            return existing->second;
        TTF_Font *font = TTF_OpenFont(fontPath.c_str(), size);
        // Remember failures too. SDL_ttf may allocate a file descriptor while
        // attempting to open a damaged or unsupported font; retrying this once
        // per frame eventually exhausts the very small descriptor limit on TSPS.
        fonts.emplace(size, font);
        return font;
    }

    void closeFonts()
    {
        for (const auto &entry : fonts)
            if (entry.second)
                TTF_CloseFont(entry.second);
        fonts.clear();
    }
#endif

    bool parseColor(const std::string &value, SDL_Color &color)
    {
        if (value.size() != 7 || value.front() != '#')
            return false;
        unsigned int rgb = 0;
        const auto parsed = std::from_chars(value.data() + 1, value.data() + value.size(), rgb, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            return false;
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
        if (colon == std::string::npos) return false;
        const std::size_t start = document.find('"', colon + 1);
        if (start == std::string::npos) return false;
        const std::size_t end = document.find('"', start + 1);
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
        const std::string_view number(document.data() + start, (end == std::string::npos ? document.size() : end) - start);
        return parseInteger(std::string(number), value);
    }

    void applyThemeConfig(const std::filesystem::path &themeDirectory, Palette &themePalette, std::string &font, ThemeTextSettings &textSettings)
    {
        std::ifstream file(themeDirectory / "theme-config.json");
        if (!file) return;
        const std::string document((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const auto applyColor = [&document](std::string_view key, SDL_Color &target)
        {
            std::string value;
            SDL_Color color{};
            if (themeValue(document, key, value) && parseColor(value, color)) target = color;
        };
        applyColor("background-dark", themePalette.backgroundDark);
        applyColor("background-light", themePalette.backgroundLight);
        applyColor("text-color", themePalette.primary);
        applyColor("hint-color", themePalette.hint);
        applyColor("button-fill", themePalette.buttonFill);
        applyColor("button-outline", themePalette.buttonOutline);
        const auto applyFontSize = [&document](std::string_view key, int &target)
        {
            int value = 0;
            if (themeInteger(document, key, value)) target = value;
        };
        applyFontSize("title-font-size", textSettings.titleFontSize);
        applyFontSize("footer-font-size", textSettings.footerFontSize);
        applyFontSize("gameplay-status-font-size", textSettings.gameplayStatusFontSize);
        applyFontSize("hint-font-size", textSettings.hintFontSize);
        applyFontSize("form-font-size", textSettings.formFontSize);
        applyFontSize("keyboard-value-font-size", textSettings.keyboardValueFontSize);
        applyFontSize("keyboard-action-font-size", textSettings.keyboardActionFontSize);
        std::string configuredFont;
        if (themeValue(document, "font", configuredFont) && !configuredFont.empty()) font = configuredFont;
    }

    void loadUiTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const std::string &path)
    {
#ifdef WIDEMELON_HAVE_SDL_IMAGE
        SDL_Surface *surface = IMG_Load(path.c_str());
        if (!surface)
        {
            widemelon::Logger::error("Cannot load UI asset: " + path + "; " + IMG_GetError());
            return;
        }
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) widemelon::Logger::error("Cannot create UI texture: " + path + "; " + SDL_GetError());
#else
        (void)renderer;
        (void)texture;
        (void)path;
#endif
    }

    void closeUiTextures()
    {
        SDL_DestroyTexture(uiTextures.background);
        SDL_DestroyTexture(uiTextures.gameplayBackground);
        SDL_DestroyTexture(uiTextures.fieldSelected);
        SDL_DestroyTexture(uiTextures.fieldUnselected);
        SDL_DestroyTexture(uiTextures.keyboardSelected);
        SDL_DestroyTexture(uiTextures.keyboardUnselected);
        SDL_DestroyTexture(uiTextures.hintA);
        SDL_DestroyTexture(uiTextures.hintB);
        SDL_DestroyTexture(uiTextures.hintX);
        SDL_DestroyTexture(uiTextures.hintStart);
        SDL_DestroyTexture(uiTextures.hintL);
        SDL_DestroyTexture(uiTextures.hintR);
        SDL_DestroyTexture(uiTextures.cursor);
        uiTextures = {};
    }

    void loadUiResources(SDL_Renderer *renderer)
    {
        std::array<char, 4096> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
        if (length <= 0)
            return;
        executable[static_cast<std::size_t>(length)] = '\0';
        const std::string directory = std::string(executable.data()).substr(0, std::string(executable.data()).find_last_of('/'));
        std::ifstream file(directory + "/widemelon-client-ui.conf");
#ifdef WIDEMELON_HAVE_SDL_TTF
        std::string textFont = "assets/themes/WaterMelon/comfortaa-latin-400-normal.ttf";
#endif
        std::string activeTheme = "WaterMelon";
        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos)
                continue;
            const std::string key = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
            if (key == "active-theme")
            {
                if (!value.empty()) activeTheme = value;
                continue;
            }
#ifdef WIDEMELON_HAVE_SDL_TTF
            if (key == "text-font")
            {
                if (!value.empty()) textFont = value;
                continue;
            }
#endif
            if (key == "button-outline-width")
            {
                int width = 0;
                if (parseInteger(value, width)) palette.buttonOutlineWidth = std::clamp(width, 1, 32);
                continue;
            }
            SDL_Color color{};
            if (!parseColor(value, color)) continue;
            if (key == "background-dark") palette.backgroundDark = color;
            else if (key == "background-light") palette.backgroundLight = color;
            else if (key == "text-color") palette.primary = color;
            else if (key == "hint-color") palette.hint = color;
            else if (key == "button-fill") palette.buttonFill = color;
            else if (key == "button-outline") palette.buttonOutline = color;
        }
        const std::filesystem::path themesDirectory = std::filesystem::path(directory) / "assets" / "themes";
        const auto validThemeName = [](const std::string &name)
        {
            return !name.empty() && name != "." && name != ".."
                && name.find_first_of("/\\") == std::string::npos;
        };
        std::filesystem::path themeDirectory = validThemeName(activeTheme)
            ? themesDirectory / activeTheme
            : std::filesystem::path{};
        std::error_code themeError;
        if (!std::filesystem::is_directory(themeDirectory, themeError))
        {
            if (activeTheme != "WaterMelon")
                widemelon::Logger::error("UI theme not found: " + activeTheme + "; using WaterMelon");
            themeDirectory = themesDirectory / "WaterMelon";
            themeError.clear();
        }
        const std::filesystem::path defaultThemeDirectory = themesDirectory / "WaterMelon";
        if (!std::filesystem::is_directory(themeDirectory, themeError))
        {
            widemelon::Logger::error("Default UI theme not found; using square UI fallbacks");
            return;
        }

        std::string defaultThemeFont;
        applyThemeConfig(defaultThemeDirectory, palette, defaultThemeFont, themeText);
        std::string activeThemeFont = defaultThemeFont;
        if (themeDirectory != defaultThemeDirectory)
            applyThemeConfig(themeDirectory, palette, activeThemeFont, themeText);
#ifdef WIDEMELON_HAVE_SDL_TTF
        const auto resolveThemeFont = [](const std::filesystem::path &theme, const std::string &name)
        {
            const std::filesystem::path configured(name);
            if (name.empty() || configured.is_absolute()) return std::filesystem::path{};
            const std::filesystem::path root = theme.lexically_normal();
            const std::filesystem::path candidate = (root / configured).lexically_normal();
            const std::filesystem::path relative = candidate.lexically_relative(root);
            for (const auto &part : relative)
                if (part == "..") return std::filesystem::path{};
            std::error_code error;
            return !relative.empty() && std::filesystem::is_regular_file(candidate, error)
                ? candidate
                : std::filesystem::path{};
        };
        std::filesystem::path selectedFont = resolveThemeFont(themeDirectory, activeThemeFont);
        if (selectedFont.empty()) selectedFont = resolveThemeFont(defaultThemeDirectory, defaultThemeFont);
        if (!selectedFont.empty())
            fontPath = selectedFont.string();
        else
        {
            fontPath = !textFont.empty() && textFont[0] == 47 ? textFont : directory + "/" + textFont;
            widemelon::Logger::error("Theme font not found; using configured font fallback");
        }
#endif

        const std::string assets = themeDirectory.string() + "/";
        loadUiTexture(renderer, uiTextures.background, assets + "backgrounds/background.png");
        loadUiTexture(renderer, uiTextures.gameplayBackground, assets + "backgrounds/background-gameplay.png");
        loadUiTexture(renderer, uiTextures.fieldSelected, assets + "icons/field-input-selected.png");
        loadUiTexture(renderer, uiTextures.fieldUnselected, assets + "icons/field-input-unselected.png");
        loadUiTexture(renderer, uiTextures.keyboardSelected, assets + "icons/keyboard-selected.png");
        loadUiTexture(renderer, uiTextures.keyboardUnselected, assets + "icons/keyboard-unselected.png");
        loadUiTexture(renderer, uiTextures.hintA, assets + "icons/hint-A.png");
        loadUiTexture(renderer, uiTextures.hintB, assets + "icons/hint-B.png");
        loadUiTexture(renderer, uiTextures.hintX, assets + "icons/hint-X.png");
        loadUiTexture(renderer, uiTextures.hintStart, assets + "icons/hint-START.png");
        loadUiTexture(renderer, uiTextures.hintL, assets + "icons/hint-L.png");
        loadUiTexture(renderer, uiTextures.hintR, assets + "icons/hint-R.png");
        loadUiTexture(renderer, uiTextures.cursor, directory + "/assets/icons/pointer.png");
    }

    void reloadUiResources(SDL_Renderer *renderer)
    {
        closeUiTextures();
#ifdef WIDEMELON_HAVE_SDL_TTF
        closeFonts();
#endif
        palette = Palette{};
        themeText = ThemeTextSettings{};
        loadUiResources(renderer);
    }

    void drawBackground(SDL_Renderer *renderer, int width, int height, bool gameplay)
    {
        SDL_Texture *texture = gameplay ? uiTextures.gameplayBackground : uiTextures.background;
        if (texture)
        {
            const SDL_Rect destination{0, 0, width, height};
            SDL_RenderCopy(renderer, texture, nullptr, &destination);
            return;
        }

        for (int y = 0; y < height; ++y)
        {
            const int ratio = height > 1 ? y * 255 / (height - 1) : 0;
            const auto blend = [ratio](Uint8 light, Uint8 dark)
            {
                return static_cast<Uint8>((light * (255 - ratio) + dark * ratio) / 255);
            };
            SDL_SetRenderDrawColor(renderer, blend(palette.backgroundLight.r, palette.backgroundDark.r),
                blend(palette.backgroundLight.g, palette.backgroundDark.g),
                blend(palette.backgroundLight.b, palette.backgroundDark.b), 255);
            SDL_RenderDrawLine(renderer, 0, y, width, y);
        }
    }

    void drawGradientBackground(SDL_Renderer *renderer, int width, int height)
    {
        drawBackground(renderer, width, height, false);
    }

    void drawPill(SDL_Renderer *renderer, const SDL_Rect &rect, bool selected, bool keyboard = false)
    {
#ifdef WIDEMELON_HAVE_SDL_IMAGE
        SDL_Texture *texture = keyboard
            ? (selected ? uiTextures.keyboardSelected : uiTextures.keyboardUnselected)
            : (selected ? uiTextures.fieldSelected : uiTextures.fieldUnselected);
        if (texture)
        {
            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            return;
        }
#endif
        if (selected)
        {
            SDL_SetRenderDrawColor(renderer, palette.buttonFill.r, palette.buttonFill.g, palette.buttonFill.b, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_SetRenderDrawColor(renderer, palette.buttonOutline.r, palette.buttonOutline.g, palette.buttonOutline.b, 255);
        for (int thickness = 0; thickness < palette.buttonOutlineWidth; ++thickness)
        {
            const SDL_Rect outline{rect.x + thickness, rect.y + thickness, rect.w - thickness * 2, rect.h - thickness * 2};
            if (outline.w <= 0 || outline.h <= 0) break;
            SDL_RenderDrawRect(renderer, &outline);
        }
    }

    const Glyph &glyphFor(char character)
    {
        static const std::unordered_map<char, Glyph> glyphs{
            {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
            {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
            {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
            {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
            {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
            {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
            {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
            {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
            {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
            {'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
            {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
            {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
            {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
            {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
            {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
            {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
            {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
            {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
            {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
            {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
            {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
            {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
            {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
            {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
            {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
            {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
            {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
            {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
            {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
            {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
            {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
            {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
            {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
            {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
            {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
            {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
            {':', {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}},
            {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}},
            {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        };
        static const Glyph fallback{0x1F, 0x11, 0x15, 0x15, 0x15, 0x11, 0x1F};
        const auto it = glyphs.find(character);
        return it == glyphs.end() ? fallback : it->second;
    }

    int fallbackTextScale(int fontSize)
    {
        return std::max(1, (fontSize + 3) / 7);
    }

    int textWidth(std::string_view text, int fontSize)
    {
#ifdef WIDEMELON_HAVE_SDL_TTF
        int width = 0;
        if (TTF_Font *font = fontForSize(fontSize))
        {
            TTF_SizeUTF8(font, std::string(text).c_str(), &width, nullptr);
            return width;
        }
#endif
        const int scale = fallbackTextScale(fontSize);
        return static_cast<int>(text.size()) * 6 * scale - scale;
    }

    void drawText(SDL_Renderer *renderer, std::string_view text, int x, int y, int fontSize)
    {
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (TTF_Font *font = fontForSize(fontSize))
        {
            const std::string rendered(text);
            SDL_Surface *surface = TTF_RenderUTF8_Blended(font, rendered.c_str(), palette.primary);
            if (surface)
            {
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                const SDL_Rect destination{x, y, surface->w, surface->h};
                if (texture) SDL_RenderCopy(renderer, texture, nullptr, &destination);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
                return;
            }
        }
#endif
        const int scale = fallbackTextScale(fontSize);
        for (char character : text)
        {
            const Glyph &glyph = glyphFor(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            for (int row = 0; row < 7; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    if ((glyph[row] & (1U << (4 - column))) == 0) continue;
                    const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
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
            if (surface)
            {
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                const SDL_Rect destination{x, y, surface->w, surface->h};
                if (texture) SDL_RenderCopy(renderer, texture, nullptr, &destination);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
                return;
            }
        }
#endif
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        drawText(renderer, text, x, y, fontSize);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
    }

    int controlIconWidth(SDL_Texture *icon, std::string_view fallback, int fontSize)
    {
        constexpr int iconHeight = 48;
        if (!icon) return textWidth(fallback, fontSize);
        int sourceWidth = 0;
        int sourceHeight = 0;
        SDL_QueryTexture(icon, nullptr, nullptr, &sourceWidth, &sourceHeight);
        return sourceHeight > 0 ? sourceWidth * iconHeight / sourceHeight : iconHeight;
    }

    int controlHintWidth(SDL_Texture *icon, std::string_view fallback, std::string_view label, int fontSize)
    {
        return controlIconWidth(icon, fallback, fontSize) + widemelon::UiHintIconTextGap + textWidth(label, fontSize) + 20;
    }

    int drawControlHint(SDL_Renderer *renderer, SDL_Texture *icon, std::string_view fallback, std::string_view label, int x, int y, int fontSize)
    {
        constexpr int iconHeight = 48;
        int iconWidth = controlIconWidth(icon, fallback, fontSize);
        if (icon)
        {
            const SDL_Rect destination{x, y, iconWidth, iconHeight};
            SDL_RenderCopy(renderer, icon, nullptr, &destination);
        }
        else
        {
            iconWidth = textWidth(fallback, fontSize);
            drawTextColored(renderer, fallback, x, y + (iconHeight - fontSize) / 2, fontSize, palette.hint);
        }
        const int labelX = x + iconWidth + widemelon::UiHintIconTextGap;
        drawTextColored(renderer, label, labelX, y + (iconHeight - fontSize) / 2, fontSize, palette.hint);
        return labelX + textWidth(label, fontSize) + 20;
    }

    bool updateVideoTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const widemelon::DecodedVideoFrame &frame)
    {
        if (frame.width == 0 || frame.height == 0 || frame.rgb.size() != static_cast<std::size_t>(frame.width) * frame.height * 3)
            return false;
        if (!texture)
        {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                        frame.width, frame.height);
            if (!texture)
            {
                widemelon::Logger::error(std::string("Cannot create video texture: ") + SDL_GetError());
                return false;
            }
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        }
        if (SDL_UpdateTexture(texture, nullptr, frame.rgb.data(), frame.width * 3) != 0)
        {
            widemelon::Logger::error(std::string("Cannot update video texture: ") + SDL_GetError());
            return false;
        }
        return true;
    }

    void render(SDL_Renderer *renderer, int width, int height, const widemelon::Config &config, const std::string &status,
                SDL_Texture *videoTexture, const widemelon::Sm64TouchState *cursor = nullptr)
    {
        (void)config;
        drawBackground(renderer, width, height, true);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        const std::string title = "WideMelon Client";
        drawTextColored(renderer, title, (width - textWidth(title, themeText.titleFontSize)) / 2, 8, themeText.titleFontSize, palette.hint);
        const SDL_Rect video{(width - 768) / 2, 45, 768, 576};
        if (videoTexture)
            SDL_RenderCopy(renderer, videoTexture, nullptr, &video);
        else
            SDL_RenderFillRect(renderer, &video);
        if (cursor)
        {
            const SDL_Rect pointer{video.x + static_cast<int>(cursor->x) * video.w / 256,
                                   video.y + static_cast<int>(cursor->y) * video.h / 192, 48, 48};
            if (uiTextures.cursor)
            {
                SDL_SetTextureAlphaMod(uiTextures.cursor, cursor->active ? 255 : 128);
                SDL_RenderCopy(renderer, uiTextures.cursor, nullptr, &pointer);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, palette.hint.r, palette.hint.g, palette.hint.b, 255);
                const SDL_Rect fallback{pointer.x, pointer.y, 8, 8};
                SDL_RenderFillRect(renderer, &fallback);
            }
        }
        const std::string connection = "Connection status: " + status;
        drawTextColored(renderer, connection, video.x, video.y + video.h + 6, themeText.gameplayStatusFontSize, palette.hint);
        int exitX = video.x;
        const int exitY = video.y + video.h + 50;
        exitX = drawControlHint(renderer, uiTextures.hintStart, "START", "+", exitX, exitY, themeText.hintFontSize);
        exitX = drawControlHint(renderer, uiTextures.hintR, "R", "+", exitX, exitY, themeText.hintFontSize);
        drawControlHint(renderer, uiTextures.hintL, "L", ": QUIT", exitX, exitY, themeText.hintFontSize);
        SDL_RenderPresent(renderer);
    }

    void renderSetup(SDL_Renderer *renderer, int width, int height, const widemelon::Config &config,
                     int selected)
    {
        drawGradientBackground(renderer, width, height);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        const int formFontSize = themeText.formFontSize;
        const int titleFontSize = themeText.titleFontSize;
        const std::string title = "WideMelon Client";
        drawTextColored(renderer, title, (width - textWidth(title, titleFontSize)) / 2, 48, titleFontSize, palette.primary);
        const int centerX = width / 2;
        const int fieldWidth = widemelon::UiFormFieldWidth;
        const int fieldHeight = widemelon::UiFormFieldHeight;
        const int fieldX = centerX - fieldWidth / 2;
        const std::array<std::string, 3> labels{"Server Address", "Port", "Session code"};
        const std::array<std::string, 3> values{config.host, std::to_string(config.port), config.pairingCode};
        const std::array<int, 3> positions{144, 247, 350};
        for (int index = 0; index < 3; ++index)
        {
            drawTextColored(renderer, labels[index], centerX - textWidth(labels[index], formFontSize) / 2, positions[index] - 38, formFontSize, palette.primary);
            const SDL_Rect field{fieldX, positions[index], fieldWidth, fieldHeight};
            drawPill(renderer, field, selected == index);
            if (selected == index)
            {
                drawTextColored(renderer, values[index], centerX - textWidth(values[index], formFontSize) / 2,
                                   positions[index] + 10, formFontSize, palette.primary);
            }
            else
                drawTextColored(renderer, values[index], centerX - textWidth(values[index], formFontSize) / 2, positions[index] + 10, formFontSize, palette.primary);
        }
        const SDL_Rect connect{fieldX, 473, fieldWidth, fieldHeight};
        drawPill(renderer, connect, selected == 3);
        const std::string connectLabel = "CONNECT";
        drawTextColored(renderer, connectLabel, centerX - textWidth(connectLabel, formFontSize) / 2, 483, formFontSize, palette.primary);
        const SDL_Rect settings{fieldX, 545, fieldWidth, fieldHeight};
        drawPill(renderer, settings, selected == 4);
        const std::string settingsLabel = "SETTINGS";
        drawTextColored(renderer, settingsLabel, centerX - textWidth(settingsLabel, formFontSize) / 2, 555, formFontSize, palette.primary);
        const int formHintFontSize = themeText.hintFontSize;
        const int formHintWidth = controlHintWidth(uiTextures.hintA, "A", "EDIT", formHintFontSize)
            + controlHintWidth(uiTextures.hintStart, "START", "CONNECT", formHintFontSize);
        int hintX = (width - formHintWidth) / 2;
        const int hintY = height - 64;
        hintX = drawControlHint(renderer, uiTextures.hintA, "A", "EDIT", hintX, hintY, formHintFontSize);
        drawControlHint(renderer, uiTextures.hintStart, "START", "CONNECT", hintX, hintY, formHintFontSize);
        const std::string version = "v" WIDEMELON_VERSION;
        drawText(renderer, version, width - textWidth(version, themeText.footerFontSize) - 20, height - 34, themeText.footerFontSize);
        SDL_RenderPresent(renderer);
    }

    void renderKeyboard(SDL_Renderer *renderer, int width, int height, const std::string &title,
                        const std::string &value, int selectedKey)
    {
        static const std::array<const char *, 13> keys{
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};
        const int columns = widemelon::UiKeyboardColumns;
        const int keyWidth = widemelon::UiKeyboardKeyWidth;
        const int keyHeight = widemelon::UiKeyboardKeyHeight;
        const int startX = widemelon::UiKeyboardStartX;
        const int startY = widemelon::UiKeyboardStartY;

        drawGradientBackground(renderer, width, height);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        drawText(renderer, title, (width - textWidth(title, themeText.titleFontSize)) / 2, 65, themeText.titleFontSize);
        drawText(renderer, value.empty() ? "_" : value, (width - textWidth(value.empty() ? "_" : value, themeText.keyboardValueFontSize)) / 2, 125, themeText.keyboardValueFontSize);
        for (std::size_t index = 0; index < keys.size(); ++index)
        {
            const int row = static_cast<int>(index) / columns;
            const int column = static_cast<int>(index) % columns;
            const SDL_Rect key{startX + column * keyWidth, startY + row * keyHeight, keyWidth - 10, keyHeight - 8};
            drawPill(renderer, key, static_cast<int>(index) == selectedKey, true);
            const int fontSize = std::string_view(keys[index]).size() > 1 ? themeText.keyboardActionFontSize : themeText.keyboardValueFontSize;
            drawText(renderer, keys[index], key.x + (key.w - textWidth(keys[index], fontSize)) / 2,
                     key.y + (key.h - fontSize) / 2, fontSize);
        }
        int hintX = width / 2 - 190;
        const int hintY = height - 64;
        hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
        hintX = drawControlHint(renderer, uiTextures.hintB, "B", "DELETE", hintX, hintY, themeText.hintFontSize);
        hintX = drawControlHint(renderer, uiTextures.hintX, "X", "BACK", hintX, hintY, themeText.hintFontSize);
        drawControlHint(renderer, uiTextures.hintStart, "START", "OK", hintX, hintY, themeText.hintFontSize);
        SDL_RenderPresent(renderer);
    }

    bool isNavigation(widemelon::UiAction action)
    {
        return action == widemelon::UiAction::Up || action == widemelon::UiAction::Down
            || action == widemelon::UiAction::Left || action == widemelon::UiAction::Right;
    }

    class UiNavigationRepeater
    {
    public:
        widemelon::UiAction next(widemelon::UiAction action, widemelon::UiAction held)
        {
            const auto now = std::chrono::steady_clock::now();
            if (isNavigation(action))
            {
                repeatedAction = action;
                nextRepeatAt = now + widemelon::UiNavigationInitialDelay;
                return action;
            }
            if (!isNavigation(held))
            {
                repeatedAction = widemelon::UiAction::None;
                return action;
            }
            if (held != repeatedAction)
            {
                repeatedAction = held;
                nextRepeatAt = now + widemelon::UiNavigationInitialDelay;
                return held;
            }
            if (now < nextRepeatAt) return action;
            nextRepeatAt = now + widemelon::UiNavigationRetriggerDelay;
            return held;
        }

    private:
        widemelon::UiAction repeatedAction = widemelon::UiAction::None;
        std::chrono::steady_clock::time_point nextRepeatAt{};
    };

    bool editNumericField(SDL_Renderer *renderer, int width, int height, widemelon::EvdevInput &input,
                          const std::string &title, std::string &value, std::size_t maximumLength, bool allowDot)
    {
        static constexpr int keyCount = 13;
        static constexpr int columns = 4;
        static const std::array<const char *, keyCount> keys{
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};
        const std::string original = value;
        int selectedKey = 0;
        auto nextNavigationAt = std::chrono::steady_clock::time_point{};
        UiNavigationRepeater navigationRepeater;
        while (true)
        {
            renderKeyboard(renderer, width, height, title, value, selectedKey);
            input.pollEvent();
            if (input.exitComboPressed())
                return false;
            const widemelon::UiAction action = navigationRepeater.next(input.takeUiAction(), input.heldUiDirection());
            const bool navigation = action == widemelon::UiAction::Up || action == widemelon::UiAction::Down || action == widemelon::UiAction::Left || action == widemelon::UiAction::Right;
            const auto now = std::chrono::steady_clock::now();
            if (navigation && now < nextNavigationAt)
            {
                SDL_Delay(10);
                continue;
            }
            if (navigation)
                nextNavigationAt = now + widemelon::UiNavigationCooldown;
            switch (action)
            {
            case widemelon::UiAction::Up:
                selectedKey = (selectedKey + keyCount - columns) % keyCount;
                break;
            case widemelon::UiAction::Down:
                selectedKey = (selectedKey + columns) % keyCount;
                break;
            case widemelon::UiAction::Left:
                selectedKey = (selectedKey + keyCount - 1) % keyCount;
                break;
            case widemelon::UiAction::Right:
                selectedKey = (selectedKey + 1) % keyCount;
                break;
            case widemelon::UiAction::Delete:
                if (!value.empty())
                    value.pop_back();
                break;
            case widemelon::UiAction::Back:
                value = original;
                return false;
            case widemelon::UiAction::Start:
                return true;
            case widemelon::UiAction::Confirm:
            {
                const std::string_view key = keys[static_cast<std::size_t>(selectedKey)];
                if (key == "OK")
                    return true;
                if (key == "DEL")
                {
                    if (!value.empty())
                        value.pop_back();
                }
                else if (value.size() < maximumLength && (key != "." || allowDot))
                    value.append(key);
                break;
            }
            default:
                break;
            }
            SDL_Delay(10);
        }
    }

    enum class SettingsResult { Home, Exit };

    std::vector<std::string> availableThemes(const std::string &directory)
    {
        std::vector<std::string> themes;
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(directory) / "assets" / "themes", error))
            if (entry.is_directory(error)) themes.push_back(entry.path().filename().string());
        std::sort(themes.begin(), themes.end());
        return themes;
    }


    std::string configuredTheme(const std::string &directory)
    {
        std::ifstream file(std::filesystem::path(directory) / "widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            constexpr std::string_view key{"active-theme="};
            if (line.compare(0, key.size(), key) == 0 && line.size() > key.size())
                return line.substr(key.size());
        }
        return "WaterMelon";
    }

    bool saveConfiguredTheme(const std::string &directory, const std::string &theme)
    {
        const std::filesystem::path path = std::filesystem::path(directory) / "widemelon-client-ui.conf";
        std::ifstream input(path);
        if (!input)
        {
            widemelon::Logger::error("Cannot open UI configuration for theme selection: " + path.string());
            return false;
        }

        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, 13, "active-theme=") == 0)
            {
                lines.push_back("active-theme=" + theme);
                replaced = true;
            }
            else
                lines.push_back(line);
        }
        if (!replaced) lines.push_back("active-theme=" + theme);

        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                widemelon::Logger::error("Cannot save UI theme selection: " + temporary.string());
                return false;
            }
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output)
            {
                widemelon::Logger::error("Cannot write UI theme selection: " + temporary.string());
                return false;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            widemelon::Logger::error("Cannot activate UI theme: " + error.message());
            return false;
        }
        widemelon::Logger::info("UI theme selected: " + theme);
        return true;
    }

    std::string configuredLeftStickMode(const std::string &directory)
    {
        std::ifstream file(std::filesystem::path(directory) / "widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            constexpr std::string_view key{"left-stick-mod="};
            if (line.compare(0, key.size(), key) == 0 && line.size() > key.size())
                return line.substr(key.size());
        }
        return "D-pad";
    }

    bool saveConfiguredLeftStickMode(const std::string &directory, const std::string &mode)
    {
        const std::filesystem::path path = std::filesystem::path(directory) / "widemelon-client-ui.conf";
        std::ifstream input(path);
        if (!input)
        {
            widemelon::Logger::error("Cannot open UI configuration for left stick mode: " + path.string());
            return false;
        }

        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, 15, "left-stick-mod=") == 0)
            {
                lines.push_back("left-stick-mod=" + mode);
                replaced = true;
            }
            else
                lines.push_back(line);
        }
        if (!replaced) lines.push_back("left-stick-mod=" + mode);

        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                widemelon::Logger::error("Cannot save left stick mode: " + temporary.string());
                return false;
            }
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output)
            {
                widemelon::Logger::error("Cannot write left stick mode: " + temporary.string());
                return false;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            widemelon::Logger::error("Cannot activate left stick mode: " + error.message());
            return false;
        }
        widemelon::Logger::info("Left stick mode selected: " + mode);
        return true;
    }

    std::uint8_t configuredSm64AutoCenterHoldFrames(const std::string &directory)
    {
        std::ifstream file(std::filesystem::path(directory) / "widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            constexpr std::string_view key{"sm64-auto-center-hold-frames="};
            if (line.compare(0, key.size(), key) == 0)
            {
                unsigned int value = 0;
                const std::string raw = line.substr(key.size());
                const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
                if (result.ec == std::errc{} && result.ptr == raw.data() + raw.size())
                    return static_cast<std::uint8_t>(std::clamp(value, static_cast<unsigned int>(widemelon::Sm64TouchCenterHoldFramesMinimum), static_cast<unsigned int>(widemelon::Sm64TouchCenterHoldFramesMaximum)));
            }
        }
        return widemelon::Sm64TouchCenterHoldFramesDefault;
    }

    bool saveConfiguredSm64AutoCenterHoldFrames(const std::string &directory, std::uint8_t frames)
    {
        const std::filesystem::path path = std::filesystem::path(directory) / "widemelon-client-ui.conf";
        std::ifstream input(path);
        if (!input) return false;

        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, 29, "sm64-auto-center-hold-frames=") == 0)
            {
                lines.push_back("sm64-auto-center-hold-frames=" + std::to_string(frames));
                replaced = true;
            }
            else
                lines.push_back(line);
        }
        if (!replaced) lines.push_back("sm64-auto-center-hold-frames=" + std::to_string(frames));

        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) return false;
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output) return false;
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    std::uint16_t configuredSm64AutoReleaseDelayMs(const std::string &directory)
    {
        std::ifstream file(std::filesystem::path(directory) / "widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            constexpr std::string_view key{"sm64-auto-release-delay-ms="};
            if (line.compare(0, key.size(), key) == 0)
            {
                unsigned int value = 0;
                const std::string raw = line.substr(key.size());
                const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
                if (result.ec == std::errc{} && result.ptr == raw.data() + raw.size())
                    return static_cast<std::uint16_t>(std::clamp(value, static_cast<unsigned int>(widemelon::Sm64TouchReleaseDelayMinimumMs), static_cast<unsigned int>(widemelon::Sm64TouchReleaseDelayMaximumMs)));
            }
        }
        return widemelon::Sm64TouchReleaseDelayDefaultMs;
    }

    bool saveConfiguredSm64AutoReleaseDelayMs(const std::string &directory, std::uint16_t milliseconds)
    {
        const std::filesystem::path path = std::filesystem::path(directory) / "widemelon-client-ui.conf";
        std::ifstream input(path);
        if (!input) return false;

        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, 27, "sm64-auto-release-delay-ms=") == 0)
            {
                lines.push_back("sm64-auto-release-delay-ms=" + std::to_string(milliseconds));
                replaced = true;
            }
            else
                lines.push_back(line);
        }
        if (!replaced) lines.push_back("sm64-auto-release-delay-ms=" + std::to_string(milliseconds));

        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) return false;
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output) return false;
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    std::uint16_t configuredCursorSpeedLimit(const std::string &directory)
    {
        std::ifstream file(std::filesystem::path(directory) / "widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            constexpr std::string_view key{"cursor-speed-limit="};
            if (line.compare(0, key.size(), key) == 0)
            {
                unsigned int value = 0;
                const std::string raw = line.substr(key.size());
                const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
                if (result.ec == std::errc{} && result.ptr == raw.data() + raw.size())
                    return static_cast<std::uint16_t>(std::clamp(value, static_cast<unsigned int>(widemelon::CursorSpeedMinimum), static_cast<unsigned int>(widemelon::CursorSpeedMaximum)));
            }
        }
        return widemelon::CursorSpeedDefault;
    }

    bool saveConfiguredCursorSpeedLimit(const std::string &directory, std::uint16_t pixelsPerSecond)
    {
        const std::filesystem::path path = std::filesystem::path(directory) / "widemelon-client-ui.conf";
        std::ifstream input(path);
        if (!input) return false;
        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, 19, "cursor-speed-limit=") == 0)
            {
                lines.push_back("cursor-speed-limit=" + std::to_string(pixelsPerSecond));
                replaced = true;
            }
            else lines.push_back(line);
        }
        if (!replaced) lines.push_back("cursor-speed-limit=" + std::to_string(pixelsPerSecond));
        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) return false;
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output) return false;
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    enum class SettingsItem { Theme, LeftStickMode, Sm64AutoFrames, Sm64AutoReleaseDelay, CursorSpeed, Back, Quit };

    std::vector<SettingsItem> settingsItems(const std::string &leftStickMode)
    {
        std::vector<SettingsItem> items{SettingsItem::Theme, SettingsItem::LeftStickMode};
        if (leftStickMode == "SM64 Auto")
        {
            items.push_back(SettingsItem::Sm64AutoFrames);
            items.push_back(SettingsItem::Sm64AutoReleaseDelay);
        }
        else if (leftStickMode == "Cursor")
            items.push_back(SettingsItem::CursorSpeed);
        items.push_back(SettingsItem::Back);
        items.push_back(SettingsItem::Quit);
        return items;
    }

    void renderSettings(SDL_Renderer *renderer, int width, int height, const std::string &theme, const std::string &leftStickMode,
                        std::uint8_t sm64AutoFrames, std::uint16_t sm64AutoReleaseDelayMs, std::uint16_t cursorSpeedLimit,
                        const std::vector<SettingsItem> &items, int selected, int scrollOffset)
    {
        drawGradientBackground(renderer, width, height);
        const int centerX = width / 2;
        const int fieldWidth = widemelon::UiFormFieldWidth;
        const int fieldHeight = widemelon::UiFormFieldHeight;
        const int fieldX = centerX - fieldWidth / 2;
        const std::string title = "Settings";
        drawTextColored(renderer, title, centerX - textWidth(title, themeText.titleFontSize) / 2, 36, themeText.titleFontSize, palette.primary);

        constexpr int listTop = 112;
        constexpr int listBottomPadding = 80;
        constexpr int rowHeight = 82;
        const int listBottom = height - listBottomPadding;
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            const int rowY = listTop + static_cast<int>(index) * rowHeight - scrollOffset;
            if (rowY + rowHeight < listTop || rowY > listBottom) continue;

            std::string label;
            std::string value;
            switch (items[index])
            {
            case SettingsItem::Theme: label = "Theme"; value = "< " + theme + " >"; break;
            case SettingsItem::LeftStickMode: label = "Left stick mod"; value = "< " + leftStickMode + " >"; break;
            case SettingsItem::Sm64AutoFrames: label = "SM64 Auto delay"; value = "< " + std::to_string(sm64AutoFrames) + " frames >"; break;
            case SettingsItem::Sm64AutoReleaseDelay: label = "SM64 Auto release delay"; value = "< " + std::to_string(sm64AutoReleaseDelayMs) + " ms >"; break;
            case SettingsItem::CursorSpeed: label = "Cursor speed limit"; value = "< " + std::to_string(cursorSpeedLimit) + " px/s >"; break;
            case SettingsItem::Back: value = "BACK"; break;
            case SettingsItem::Quit: value = "QUIT"; break;
            }

            if (!label.empty())
                drawTextColored(renderer, label, centerX - textWidth(label, themeText.formFontSize) / 2, rowY, themeText.formFontSize, palette.primary);
            const SDL_Rect field{fieldX, rowY + (label.empty() ? 0 : 30), fieldWidth, fieldHeight};
            drawPill(renderer, field, static_cast<int>(index) == selected);
            drawTextColored(renderer, value, centerX - textWidth(value, themeText.formFontSize) / 2, field.y + 10, themeText.formFontSize, palette.primary);
        }

        int hintX = width / 2 - 150;
        const int hintY = height - 64;
        hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
        drawControlHint(renderer, uiTextures.hintB, "B", "BACK", hintX, hintY, themeText.hintFontSize);
        SDL_RenderPresent(renderer);
    }

    SettingsResult editSettings(SDL_Renderer *renderer, int width, int height, widemelon::EvdevInput &input)
    {
        std::array<char, 4096> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
        if (length <= 0) return SettingsResult::Home;
        executable[static_cast<std::size_t>(length)] = 0;
        const std::string directory = std::filesystem::path(executable.data()).parent_path().string();
        std::vector<std::string> themes = availableThemes(directory);
        if (themes.empty()) return SettingsResult::Home;
        std::size_t themeIndex = 0;
        const std::string currentTheme = configuredTheme(directory);
        const auto current = std::find(themes.begin(), themes.end(), currentTheme);
        if (current != themes.end()) themeIndex = static_cast<std::size_t>(std::distance(themes.begin(), current));

        const std::array<std::string, 5> leftStickModes{"Disabled", "D-pad", "SM64 Auto", "SM64 Manual", "Cursor"};
        std::size_t leftStickModeIndex = 0;
        const std::string currentLeftStickMode = configuredLeftStickMode(directory);
        const auto currentLeftStick = std::find(leftStickModes.begin(), leftStickModes.end(), currentLeftStickMode);
        if (currentLeftStick != leftStickModes.end())
            leftStickModeIndex = static_cast<std::size_t>(std::distance(leftStickModes.begin(), currentLeftStick));
        else if (currentLeftStickMode == "SM64")
            leftStickModeIndex = 2;

        std::uint8_t sm64AutoFrames = configuredSm64AutoCenterHoldFrames(directory);
        std::uint16_t sm64AutoReleaseDelayMs = configuredSm64AutoReleaseDelayMs(directory);
        std::uint16_t cursorSpeedLimit = configuredCursorSpeedLimit(directory);
        int selected = 0;
        int scrollOffset = 0;
        UiNavigationRepeater repeater;
        auto nextNavigationAt = std::chrono::steady_clock::time_point{};
        while (true)
        {
            const std::string &leftStickMode = leftStickModes[leftStickModeIndex];
            const std::vector<SettingsItem> items = settingsItems(leftStickMode);
            selected = std::min(selected, static_cast<int>(items.size()) - 1);
            constexpr int listTop = 112;
            constexpr int listBottomPadding = 80;
            constexpr int rowHeight = 82;
            const int viewportHeight = height - listBottomPadding - listTop;
            const int selectedTop = selected * rowHeight;
            if (selectedTop < scrollOffset) scrollOffset = selectedTop;
            else if (selectedTop + rowHeight > scrollOffset + viewportHeight) scrollOffset = selectedTop + rowHeight - viewportHeight;
            scrollOffset = std::clamp(scrollOffset, 0, std::max(0, static_cast<int>(items.size()) * rowHeight - viewportHeight));
            renderSettings(renderer, width, height, themes[themeIndex], leftStickMode, sm64AutoFrames, sm64AutoReleaseDelayMs, cursorSpeedLimit, items, selected, scrollOffset);

            input.pollEvent();
            if (input.exitComboPressed()) return SettingsResult::Exit;
            const auto action = repeater.next(input.takeUiAction(), input.heldUiDirection());
            if (action == widemelon::UiAction::Back || action == widemelon::UiAction::Delete) return SettingsResult::Home;
            const bool navigation = isNavigation(action);
            const auto now = std::chrono::steady_clock::now();
            if (navigation && now < nextNavigationAt)
            {
                SDL_Delay(10);
                continue;
            }
            if (navigation) nextNavigationAt = now + widemelon::UiNavigationCooldown;
            if (action == widemelon::UiAction::Up) selected = (selected + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
            else if (action == widemelon::UiAction::Down) selected = (selected + 1) % static_cast<int>(items.size());
            else if (action == widemelon::UiAction::Left || action == widemelon::UiAction::Right)
            {
                const int direction = action == widemelon::UiAction::Left ? -1 : 1;
                switch (items[static_cast<std::size_t>(selected)])
                {
                case SettingsItem::Theme:
                {
                    const std::size_t nextTheme = static_cast<std::size_t>((static_cast<int>(themeIndex) + direction + static_cast<int>(themes.size())) % static_cast<int>(themes.size()));
                    if (saveConfiguredTheme(directory, themes[nextTheme]))
                    {
                        themeIndex = nextTheme;
                        reloadUiResources(renderer);
                    }
                    break;
                }
                case SettingsItem::LeftStickMode:
                {
                    const std::size_t nextMode = static_cast<std::size_t>((static_cast<int>(leftStickModeIndex) + direction + static_cast<int>(leftStickModes.size())) % static_cast<int>(leftStickModes.size()));
                    if (saveConfiguredLeftStickMode(directory, leftStickModes[nextMode])) leftStickModeIndex = nextMode;
                    break;
                }
                case SettingsItem::Sm64AutoFrames:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoFrames) + direction, static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMinimum), static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMaximum));
                    if (saveConfiguredSm64AutoCenterHoldFrames(directory, static_cast<std::uint8_t>(next))) sm64AutoFrames = static_cast<std::uint8_t>(next);
                    break;
                }
                case SettingsItem::Sm64AutoReleaseDelay:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoReleaseDelayMs) + direction * static_cast<int>(widemelon::Sm64TouchReleaseDelayStepMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMinimumMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMaximumMs));
                    if (saveConfiguredSm64AutoReleaseDelayMs(directory, static_cast<std::uint16_t>(next))) sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(next);
                    break;
                }
                case SettingsItem::CursorSpeed:
                {
                    const int next = std::clamp(static_cast<int>(cursorSpeedLimit) + direction * static_cast<int>(widemelon::CursorSpeedStep), static_cast<int>(widemelon::CursorSpeedMinimum), static_cast<int>(widemelon::CursorSpeedMaximum));
                    if (saveConfiguredCursorSpeedLimit(directory, static_cast<std::uint16_t>(next))) cursorSpeedLimit = static_cast<std::uint16_t>(next);
                    break;
                }
                default: break;
                }
            }
            else if (action == widemelon::UiAction::Confirm && items[static_cast<std::size_t>(selected)] == SettingsItem::Back) return SettingsResult::Home;
            else if (action == widemelon::UiAction::Confirm && items[static_cast<std::size_t>(selected)] == SettingsItem::Quit) return SettingsResult::Exit;
            SDL_Delay(10);
        }
    }

    bool editConfiguration(SDL_Renderer *renderer, int width, int height, widemelon::Config &config,
                           widemelon::EvdevInput &input)
    {
        int selected = 0;
        auto nextNavigationAt = std::chrono::steady_clock::time_point{};
        UiNavigationRepeater navigationRepeater;
        while (true)
        {
            renderSetup(renderer, width, height, config, selected);
            input.pollEvent();
            if (input.exitComboPressed())
                return false;
            const widemelon::UiAction action = navigationRepeater.next(input.takeUiAction(), input.heldUiDirection());
            const bool navigation = action == widemelon::UiAction::Up || action == widemelon::UiAction::Down || action == widemelon::UiAction::Left || action == widemelon::UiAction::Right;
            const auto now = std::chrono::steady_clock::now();
            const bool acceptNavigation = !navigation || now >= nextNavigationAt;
            if (navigation && acceptNavigation)
                nextNavigationAt = now + widemelon::UiNavigationCooldown;
            if (acceptNavigation && action == widemelon::UiAction::Up)
                selected = (selected + 4) % 5;
            else if (acceptNavigation && action == widemelon::UiAction::Down)
                selected = (selected + 1) % 5;
            else if (action == widemelon::UiAction::Confirm || action == widemelon::UiAction::Start)
            {
                if (action == widemelon::UiAction::Start)
                    selected = 3;
                if (selected == 0)
                    editNumericField(renderer, width, height, input, "HOST ADDRESS", config.host, 15, true);
                else if (selected == 1)
                {
                    std::string port = std::to_string(config.port);
                    if (editNumericField(renderer, width, height, input, "PORT", port, 5, false))
                    {
                        unsigned int parsed = 0;
                        const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed);
                        if (result.ec == std::errc{} && result.ptr == port.data() + port.size() && parsed <= 65535)
                            config.port = static_cast<std::uint16_t>(parsed);
                        else
                            widemelon::Logger::error("Configuration form error: invalid port");
                    }
                }
                else if (selected == 2)
                    editNumericField(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false);
                else if (selected == 3)
                {
                    const widemelon::ConfigLoadResult checked = widemelon::ConfigLoader::validate(config);
                    if (!checked.ok)
                    {
                        widemelon::Logger::error("Configuration form error: " + checked.error);
                    }
                    else
                    {
                        std::string error;
                        if (widemelon::ConfigLoader::saveConfiguration(config, error))
                        {
                            widemelon::Logger::info("Configuration saved from setup form");
                            return true;
                        }
                        // Persistence is optional: the values entered in this
                        // session remain valid and must not prevent connecting.
                        widemelon::Logger::error("Configuration save warning: " + error);
                        return true;
                    }
                }
                else if (editSettings(renderer, width, height, input) == SettingsResult::Exit)
                    return false;
            }
            SDL_Delay(10);
        }
    }

}

namespace widemelon
{

    bool ConfigScreen::show(Config config, bool inputTest, std::string &error)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
        {
            error = SDL_GetError();
            return false;
        }

        SDL_Window *window = SDL_CreateWindow("WideMelon Linux Client", SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (!window)
        {
            error = SDL_GetError();
            SDL_Quit();
            return false;
        }
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer)
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer)
        {
            error = SDL_GetError();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

#ifdef WIDEMELON_HAVE_SDL_IMAGE
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
            Logger::error(std::string("Cannot initialize PNG support: ") + IMG_GetError());
#endif
        loadUiResources(renderer);
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (TTF_Init() != 0)
        {
            error = TTF_GetError();
            closeUiTextures();
#ifdef WIDEMELON_HAVE_SDL_IMAGE
            IMG_Quit();
#endif
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        if (!fontForSize(14))
            Logger::error("Cannot load UI font: " + fontPath + "; " + TTF_GetError());
        else
            Logger::info("Loaded UI font: " + fontPath);
#endif

        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);

        EvdevInput exitInput;
        std::string inputError;
        if (!exitInput.open("/dev/input/event4", inputError))
        {
            error = "Cannot open controller input: " + inputError;
#ifdef WIDEMELON_HAVE_SDL_TTF
            closeFonts();
            TTF_Quit();
#endif
            closeUiTextures();
#ifdef WIDEMELON_HAVE_SDL_IMAGE
            IMG_Quit();
#endif
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        SDL_Texture *videoTexture = nullptr;
        auto closeDisplay = [&]
        {
            SDL_DestroyTexture(videoTexture);
            closeUiTextures();
#ifdef WIDEMELON_HAVE_SDL_IMAGE
            IMG_Quit();
#endif
#ifdef WIDEMELON_HAVE_SDL_TTF
            closeFonts();
            TTF_Quit();
#endif
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        };

        std::unique_ptr<widemelon::WebSocketClient> client = std::make_unique<widemelon::WebSocketClient>();
        widemelon::Sm64StickMod sm64Stick;
        widemelon::Sm64ManualStickMod sm64ManualStick;
        widemelon::CursorStickMod cursorStick;
        bool sm64Enabled = false;
        bool sm64ManualEnabled = false;
        bool cursorEnabled = false;
        auto sm64ModeEnabled = [&] { return sm64Enabled || sm64ManualEnabled || cursorEnabled; };
        auto sm64TouchState = [&]() -> const Sm64TouchState&
        { return cursorEnabled ? cursorStick.touchState() : (sm64ManualEnabled ? sm64ManualStick.touchState() : sm64Stick.touchState()); };
        std::future<std::string> connection;
        if (!inputTest)
        {
            while (true)
            {
                if (!editConfiguration(renderer, width, height, config, exitInput))
                {
                    Logger::info("Configuration form cancelled");
                    closeDisplay();
                    return true;
                }

                client = std::make_unique<widemelon::WebSocketClient>();
                connection = std::async(std::launch::async, [&client, &config]
                                        { return client->connectAndAuthenticate(config, std::chrono::seconds(5)); });

                while (connection.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready && client->connectionState() != widemelon::ConnectionState::Connected)
                {
                    renderSetup(renderer, width, height, config, 3);
                    exitInput.pollEvent();
                    if (exitInput.exitComboPressed())
                    {
                        client->requestStop();
                        connection.get();
                        closeDisplay();
                        return true;
                    }
                    SDL_Delay(10);
                }

                if (client->connectionState() == widemelon::ConnectionState::Connected)
                    break;
                const std::string connectionError = connection.get();
                client->requestStop();
                if (connectionError != "Cancelled")
                    Logger::error("Configuration connection test failed: " + connectionError);
                continue;
            }
        }

        if (!inputTest)
        {
            std::array<char, 4096> executable{};
            const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
            if (length > 0)
            {
                executable[static_cast<std::size_t>(length)] = 0;
                const std::string directory = std::filesystem::path(executable.data()).parent_path().string();
                const std::string leftStickMode = configuredLeftStickMode(directory);
                sm64Enabled = leftStickMode == "SM64 Auto" || leftStickMode == "SM64";
                sm64ManualEnabled = leftStickMode == "SM64 Manual";
                cursorEnabled = leftStickMode == "Cursor";
                sm64Stick.setCenterHoldFrames(configuredSm64AutoCenterHoldFrames(directory));
                sm64Stick.setReleaseDelayMs(configuredSm64AutoReleaseDelayMs(directory));
                sm64Stick.setEnabled(sm64Enabled);
                sm64ManualStick.setEnabled(sm64ManualEnabled);
                cursorStick.setSpeedLimit(configuredCursorSpeedLimit(directory));
                cursorStick.setEnabled(cursorEnabled);
                exitInput.setLeftStickDpadEnabled(leftStickMode == "D-pad");
                if (sm64Enabled) Logger::info("SM64 Auto stick mod enabled");
                if (sm64ManualEnabled) Logger::info("SM64 Manual stick mod enabled");
                if (cursorEnabled) Logger::info("Cursor stick mod enabled");
                if (leftStickMode == "Disabled") Logger::info("Left stick disabled");
            }
        }

        std::string status = inputTest ? "INPUT TEST ACTIVE" : "CONNECTION OK";
        if (inputTest)
        {
            connection = std::async(std::launch::async, []
                                    { return std::string{}; });
        }
        render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);

        bool running = true;
        ConnectionState displayedConnectionState = client->connectionState();
        bool inputDirty = true;
        std::uint32_t inputSequence = 0;
        std::uint32_t displayedVideoSequence = 0;
        std::uint64_t inputConnectionGeneration = 0;
        bool sendReleasedSnapshot = false;
        auto nextInputSnapshot = std::chrono::steady_clock::now();
        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        SDL_GetWindowSize(window, &width, &height);
                        render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                    }
                    break;
                default:
                    break;
                }
            }
            const std::string inputEvent = exitInput.pollEvent();
            const bool sm64TouchChanged = sm64Stick.update(exitInput.leftStickState());
            const bool sm64ManualTouchChanged = sm64ManualStick.update(exitInput.leftStickState(), exitInput.r2Pressed());
            const bool cursorTouchChanged = cursorStick.update(exitInput.leftStickState(), exitInput.r2Pressed());
            inputDirty = exitInput.takeStateChanged() || sm64TouchChanged || sm64ManualTouchChanged || cursorTouchChanged || inputDirty;
            if ((sm64TouchChanged || sm64ManualTouchChanged || cursorTouchChanged) && !inputTest)
                render(renderer, width, height, config, status, videoTexture, &sm64TouchState());
            if (inputTest && !inputEvent.empty())
            {
                status = inputEvent;
                render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
            }
            const ConnectionState currentConnectionState = client->connectionState();
            if (!inputTest && currentConnectionState != displayedConnectionState)
            {
                if (currentConnectionState == ConnectionState::Connected)
                    status = "CONNECTION OK";
                else if (currentConnectionState == ConnectionState::Connecting)
                    status = "RECONNECTING";
                else if (currentConnectionState == ConnectionState::Failed)
                    status = "CONNECTION ERROR";
                render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                displayedConnectionState = currentConnectionState;
            }
            if (!inputTest && connection.valid() && connection.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                const std::string result = connection.get();
                if (result != "Cancelled")
                {
                    status = "CONNECTION ERROR";
                    render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                }
            }
            if (!inputTest && client->connectionState() == ConnectionState::Connected)
            {
                const auto now = std::chrono::steady_clock::now();
                const std::uint64_t generation = client->connectionGeneration();
                if (generation != inputConnectionGeneration)
                {
                    inputConnectionGeneration = generation;
                    inputSequence = 0;
                    displayedVideoSequence = 0;
                    SDL_DestroyTexture(videoTexture);
                    videoTexture = nullptr;
                    inputDirty = true;
                    sendReleasedSnapshot = true;
                    nextInputSnapshot = now;
                    render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                }
                if (sendReleasedSnapshot)
                {
                    const std::uint32_t sequence = ++inputSequence;
                    client->sendInputSnapshot(sequence, 0, false, sm64TouchState().x, sm64TouchState().y);
                    sendReleasedSnapshot = false;
                }
                else if (inputDirty || now >= nextInputSnapshot)
                {
                    const std::uint32_t sequence = ++inputSequence;
                    const bool touchActive = sm64ModeEnabled() && sm64TouchState().active;
                    client->sendInputSnapshot(sequence, exitInput.buttonMask(),
                        touchActive, sm64TouchState().x, sm64TouchState().y);
                    inputDirty = false;
                    nextInputSnapshot = now + std::chrono::milliseconds(200);
                }

                DecodedVideoFrame decodedFrame;
                if (client->latestDecodedVideoFrame(decodedFrame) && decodedFrame.sequence != displayedVideoSequence && updateVideoTexture(renderer, videoTexture, decodedFrame))
                {
                    displayedVideoSequence = decodedFrame.sequence;
                    render(renderer, width, height, config, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                }
            }
            if (exitInput.exitComboPressed())
            {
                Logger::info("Exit requested by Start + L + R; stopping network connection");
                client->requestStop();
                running = false;
            }
            SDL_Delay(10);
        }

        if (!inputTest && client->connectionState() == ConnectionState::Connected)
            client->sendInputSnapshot(++inputSequence, 0, false, sm64TouchState().x, sm64TouchState().y);
        client->requestStop();
        if (connection.valid())
        {
            try
            {
                connection.get();
            }
            catch (const std::exception &exception)
            {
                Logger::error(std::string("Network worker stopped with exception: ") + exception.what());
            }
        }
        closeDisplay();
        return true;
    }

}
