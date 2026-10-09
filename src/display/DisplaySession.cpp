#include "display/ConfigScreen.h"
#include "display/DisplaySession.h"
#include "display/MenuNavigation.h"
#include "display/NumpadScreen.h"
#include "display/PageRenderer.h"
#include "display/UiConfig.h"
#include "display/UiTheme.h"
#include "display/pages/HomePage.h"
#include "display/pages/LeftStickCalibrationPage.h"
#include "display/pages/LeftStickModPage.h"
#include "display/pages/NumpadPage.h"
#include "display/pages/SettingsPage.h"

#include "common/Logger.h"
#include "input/CursorStickMod.h"
#include "input/LeftStickCalibration.h"
#include "input/Sm64DpadMod.h"
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

    auto &palette = widemelon::display::uiPalette();
    auto &themeText = widemelon::display::uiTextSettings();
    auto &uiTextures = widemelon::display::uiTextures();
    using widemelon::display::controlHintWidth;
    using widemelon::display::drawBackground;
    using widemelon::display::drawControlHint;
    using widemelon::display::drawGradientBackground;
    using widemelon::display::drawPill;
    using widemelon::display::drawText;
    using widemelon::display::drawTextColored;
    using widemelon::display::loadUiResources;
    using widemelon::display::reloadUiResources;
    using widemelon::display::textWidth;

    constexpr std::string_view UiConfigThemeKey{"active-theme"};
    constexpr std::string_view UiConfigLeftStickModeKey{"left-stick-mod"};
    constexpr std::string_view UiConfigSm64AutoFramesKey{"sm64-auto-center-hold-frames"};
    constexpr std::string_view UiConfigSm64AutoReleaseDelayKey{"sm64-auto-release-delay-ms"};
    constexpr std::string_view UiConfigCursorSpeedKey{"cursor-speed-limit"};
    constexpr std::string_view UiConfigSm64DpadDeadzoneKey{"sm64-dpad-deadzone-percent"};
    constexpr std::string_view UiConfigLeftStickScaleLeftKey{"left-stick-scale-left-percent"};
    constexpr std::string_view UiConfigLeftStickScaleRightKey{"left-stick-scale-right-percent"};
    constexpr std::string_view UiConfigLeftStickScaleUpKey{"left-stick-scale-up-percent"};
    constexpr std::string_view UiConfigLeftStickScaleDownKey{"left-stick-scale-down-percent"};

    widemelon::display::UiConfig uiConfigFor(const std::string &directory)
    {
        return widemelon::display::UiConfig(std::filesystem::path(directory) / "widemelon-client-ui.conf");
    }

// Replaced by display/UiTheme.cpp. Keep the old implementation out of the
// build temporarily until the remaining display modules have been extracted.
#if 0
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
        const widemelon::display::UiConfig uiConfig = uiConfigFor(directory);
        std::ifstream file(directory + "/widemelon-client-ui.conf");
#ifdef WIDEMELON_HAVE_SDL_TTF
        std::string textFont = "assets/themes/DukuSlice/comfortaa-latin-400-normal.ttf";
#endif
        std::string activeTheme = uiConfig.readValue(UiConfigThemeKey).value_or("DukuSlice");
        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos)
                continue;
            const std::string key = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
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
            if (activeTheme != "DukuSlice")
                widemelon::Logger::error("UI theme not found: " + activeTheme + "; using DukuSlice");
            themeDirectory = themesDirectory / "DukuSlice";
            themeError.clear();
        }
        const std::filesystem::path defaultThemeDirectory = themesDirectory / "DukuSlice";
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
#endif

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

    enum class MenuPage { Home, Settings, LeftStickMod, LeftStickCalibration };
    enum class MenuItem {
        HomeHostLabel, HomeHost, HomePortLabel, HomePort, HomeSessionCodeLabel, HomeSessionCode,
        SectionConnect, HomeConnect, SectionSettings, HomeSettings,
        SectionTheme, Theme, SectionAnalogSticks, LeftStickMod, LeftStickCalibration,
        LeftStickModeHint, LeftStickMode, LeftStickDescription,
        Sm64AutoFramesLabel, Sm64AutoFrames, Sm64AutoReleaseDelayLabel, Sm64AutoReleaseDelay,
        Sm64DpadDeadzoneLabel, Sm64DpadDeadzone, CursorSpeedLabel, CursorSpeed,
        LeftStickCalibrationHint, LeftStickScaleUpLabel, LeftStickScaleUp,
        LeftStickScaleDownLabel, LeftStickScaleDown, LeftStickScaleLeftLabel, LeftStickScaleLeft,
        LeftStickScaleRightLabel, LeftStickScaleRight,
        SectionNavigation, Back, Quit
    };

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


    int menuItemHeight(MenuItem item, std::string_view leftStickMode = {})
    {
        switch (item)
        {
        case MenuItem::SectionConnect:
        case MenuItem::SectionSettings:
        case MenuItem::SectionTheme:
        case MenuItem::SectionAnalogSticks:
        case MenuItem::SectionNavigation:
        case MenuItem::HomeHostLabel:
        case MenuItem::HomePortLabel:
        case MenuItem::HomeSessionCodeLabel:
        case MenuItem::Sm64AutoFramesLabel:
        case MenuItem::Sm64AutoReleaseDelayLabel:
        case MenuItem::Sm64DpadDeadzoneLabel:
        case MenuItem::CursorSpeedLabel:
        case MenuItem::LeftStickScaleUpLabel:
        case MenuItem::LeftStickScaleDownLabel:
        case MenuItem::LeftStickScaleLeftLabel:
        case MenuItem::LeftStickScaleRightLabel:
            return themeText.formFontSize;
        case MenuItem::LeftStickModeHint:
            return themeText.gameplayStatusFontSize * 2 + 8;
        case MenuItem::LeftStickCalibrationHint:
            return themeText.gameplayStatusFontSize * 3 + 16;
        case MenuItem::LeftStickDescription:
        {
            const std::string_view description = widemelon::display::pages::leftStickModeDescription(leftStickMode);
            const int lineCount = 1 + static_cast<int>(std::count(description.begin(), description.end(), '\n'));
            return lineCount * themeText.gameplayStatusFontSize + std::max(0, lineCount - 1) * 8;
        }
        default:
            return widemelon::UiFormFieldHeight;
        }
    }

    int menuItemTop(const std::vector<MenuItem> &items, std::size_t index, std::string_view leftStickMode = {})
    {
        int top = 0;
        for (std::size_t itemIndex = 0; itemIndex < index; ++itemIndex)
            top += menuItemHeight(items[itemIndex], leftStickMode) + widemelon::UiMenuItemGap;
        return top;
    }

    int menuListTop()
    {
        widemelon::display::PageRenderContext context;
        context.itemGap = widemelon::UiMenuItemGap;
        context.titleFontSize = themeText.titleFontSize;
        return widemelon::display::PageRenderer::firstItemY(context);
    }

    int menuFooterTop(int height)
    {
        widemelon::display::PageRenderContext context;
        context.height = height;
        return widemelon::display::PageRenderer::footerTop(context);
    }

    int menuListBottom(int height)
    {
        return menuFooterTop(height) - widemelon::UiMenuItemGap;
    }

    bool menuItemSelectable(MenuItem item)
    {
        switch (item)
        {
        case MenuItem::HomeHostLabel:
        case MenuItem::HomePortLabel:
        case MenuItem::HomeSessionCodeLabel:
        case MenuItem::SectionConnect:
        case MenuItem::SectionSettings:
        case MenuItem::SectionTheme:
        case MenuItem::SectionAnalogSticks:
        case MenuItem::LeftStickModeHint:
        case MenuItem::LeftStickDescription:
        case MenuItem::LeftStickCalibrationHint:
        case MenuItem::Sm64AutoFramesLabel:
        case MenuItem::Sm64AutoReleaseDelayLabel:
        case MenuItem::Sm64DpadDeadzoneLabel:
        case MenuItem::CursorSpeedLabel:
        case MenuItem::LeftStickScaleUpLabel:
        case MenuItem::LeftStickScaleDownLabel:
        case MenuItem::LeftStickScaleLeftLabel:
        case MenuItem::LeftStickScaleRightLabel:
        case MenuItem::SectionNavigation:
            return false;
        default:
            return true;
        }
    }

    std::vector<widemelon::display::MenuNavigationItem> menuNavigationItems(const std::vector<MenuItem> &items,
                                                                              std::string_view leftStickMode = {})
    {
        std::vector<widemelon::display::MenuNavigationItem> navigationItems;
        navigationItems.reserve(items.size());
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            navigationItems.push_back({menuItemSelectable(items[index]), menuItemTop(items, index, leftStickMode),
                                       menuItemHeight(items[index], leftStickMode)});
        }
        return navigationItems;
    }

    int menuScrollOffset(const std::vector<MenuItem> &items, std::string_view leftStickMode, int selected,
                         int currentOffset, int height)
    {
        const int viewportHeight = menuListBottom(height) - menuListTop();
        return widemelon::display::MenuNavigation::scrollOffsetForSelection(
            menuNavigationItems(items, leftStickMode), selected, currentOffset, viewportHeight);
    }

    std::vector<MenuItem> menuItems(MenuPage page, const std::string &leftStickMode)
    {
        if (page == MenuPage::Home)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::homePage().sections)
            {
                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    switch (field.value)
                    {
                    case widemelon::display::ValueId::Host:
                        items.push_back(MenuItem::HomeHostLabel);
                        items.push_back(MenuItem::HomeHost);
                        break;
                    case widemelon::display::ValueId::Port:
                        items.push_back(MenuItem::HomePortLabel);
                        items.push_back(MenuItem::HomePort);
                        break;
                    case widemelon::display::ValueId::PairingCode:
                        items.push_back(MenuItem::HomeSessionCodeLabel);
                        items.push_back(MenuItem::HomeSessionCode);
                        break;
                    default:
                        if (field.action == widemelon::display::ActionId::Connect)
                        {
                            items.push_back(MenuItem::SectionConnect);
                            items.push_back(MenuItem::HomeConnect);
                        }
                        else if (field.destination == widemelon::display::PageId::Settings)
                        {
                            items.push_back(MenuItem::SectionSettings);
                            items.push_back(MenuItem::HomeSettings);
                        }
                        break;
                    }
                }
            }
            return items;
        }
        if (page == MenuPage::Settings)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::settingsPage().sections)
            {
                if (section.title == "Theme") items.push_back(MenuItem::SectionTheme);
                else if (section.title == "Analog sticks") items.push_back(MenuItem::SectionAnalogSticks);
                else if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);

                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    if (field.value == widemelon::display::ValueId::ActiveTheme)
                        items.push_back(MenuItem::Theme);
                    else if (field.destination == widemelon::display::PageId::LeftStickMod)
                        items.push_back(MenuItem::LeftStickMod);
                    else if (field.destination == widemelon::display::PageId::LeftStickCalibration)
                        items.push_back(MenuItem::LeftStickCalibration);
                    else if (field.action == widemelon::display::ActionId::Quit)
                        items.push_back(MenuItem::Quit);
                    else if (field.destination == widemelon::display::PageId::Home)
                        items.push_back(MenuItem::Back);
                }
            }
            return items;
        }
        if (page == MenuPage::LeftStickCalibration)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::leftStickCalibrationPage().sections)
            {
                if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);
                if (!section.hintLines.empty()) items.push_back(MenuItem::LeftStickCalibrationHint);
                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    switch (field.value)
                    {
                    case widemelon::display::ValueId::LeftStickScaleUpPercent:
                        items.push_back(MenuItem::LeftStickScaleUpLabel);
                        items.push_back(MenuItem::LeftStickScaleUp);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleDownPercent:
                        items.push_back(MenuItem::LeftStickScaleDownLabel);
                        items.push_back(MenuItem::LeftStickScaleDown);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleLeftPercent:
                        items.push_back(MenuItem::LeftStickScaleLeftLabel);
                        items.push_back(MenuItem::LeftStickScaleLeft);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleRightPercent:
                        items.push_back(MenuItem::LeftStickScaleRightLabel);
                        items.push_back(MenuItem::LeftStickScaleRight);
                        break;
                    default:
                        if (field.destination == widemelon::display::PageId::Settings) items.push_back(MenuItem::Back);
                        break;
                    }
                }
            }
            return items;
        }

        const widemelon::display::PageDefinition definition = widemelon::display::pages::leftStickModPage(leftStickMode);
        std::vector<MenuItem> items;
        for (const widemelon::display::SectionDefinition &section : definition.sections)
        {
            if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);
            if (!section.hintLines.empty())
            {
                items.push_back(section.title == "Mode hint" ? MenuItem::LeftStickModeHint : MenuItem::LeftStickDescription);
            }
            for (const widemelon::display::FieldDefinition &field : section.fields)
            {
                switch (field.value)
                {
                case widemelon::display::ValueId::LeftStickMode: items.push_back(MenuItem::LeftStickMode); break;
                case widemelon::display::ValueId::Sm64AutoDelayFrames:
                    items.push_back(MenuItem::Sm64AutoFramesLabel);
                    items.push_back(MenuItem::Sm64AutoFrames);
                    break;
                case widemelon::display::ValueId::Sm64AutoReleaseDelayMs:
                    items.push_back(MenuItem::Sm64AutoReleaseDelayLabel);
                    items.push_back(MenuItem::Sm64AutoReleaseDelay);
                    break;
                case widemelon::display::ValueId::Sm64DpadDeadzonePercent:
                    items.push_back(MenuItem::Sm64DpadDeadzoneLabel);
                    items.push_back(MenuItem::Sm64DpadDeadzone);
                    break;
                case widemelon::display::ValueId::CursorSpeedLimit:
                    items.push_back(MenuItem::CursorSpeedLabel);
                    items.push_back(MenuItem::CursorSpeed);
                    break;
                default:
                    if (field.destination == widemelon::display::PageId::Settings) items.push_back(MenuItem::Back);
                    break;
                }
            }
        }
        return items;
    }

    const char *menuTitle(MenuPage page)
    {
        switch (page)
        {
        case MenuPage::Home: return widemelon::display::pages::homePage().title.data();
        case MenuPage::Settings: return widemelon::display::pages::settingsPage().title.data();
        case MenuPage::LeftStickMod: return widemelon::display::pages::leftStickModPage({}).title.data();
        case MenuPage::LeftStickCalibration: return widemelon::display::pages::leftStickCalibrationPage().title.data();
        }
        return "Settings";
    }

    void drawAdjustableValue(SDL_Renderer *renderer, const SDL_Rect &field, const std::string &value, bool selected)
    {
        drawPill(renderer, field, selected);
        const int textY = field.y + (field.h - themeText.formFontSize) / 2;
        drawTextColored(renderer, "<", field.x + 12, textY, themeText.formFontSize, palette.primary);
        drawTextColored(renderer, value, field.x + (field.w - textWidth(value, themeText.formFontSize)) / 2,
                        textY, themeText.formFontSize, palette.primary);
        drawTextColored(renderer, ">", field.x + field.w - textWidth(">", themeText.formFontSize) - 12,
                        textY, themeText.formFontSize, palette.primary);
    }

    void drawMenuText(SDL_Renderer *renderer, int centerX, int rowY, std::string_view text, SDL_Color color, int fontSize)
    {
        drawTextColored(renderer, text, centerX - textWidth(text, fontSize) / 2, rowY, fontSize, color);
    }

    void drawMenuMultilineText(SDL_Renderer *renderer, int centerX, int rowY, std::string_view text,
                               SDL_Color color, int fontSize)
    {
        constexpr int lineGap = 8;
        std::size_t start = 0;
        int line = 0;
        while (start <= text.size())
        {
            const std::size_t end = text.find('\n', start);
            const std::string_view current = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
            if (!current.empty()) drawMenuText(renderer, centerX, rowY + line * (fontSize + lineGap), current, color, fontSize);
            if (end == std::string_view::npos) break;
            start = end + 1;
            ++line;
        }
    }

    void renderMenu(SDL_Renderer *renderer, int width, int height, MenuPage page, const widemelon::Config &config, const std::string &theme, const std::string &leftStickMode,
                    std::uint8_t sm64AutoFrames, std::uint16_t sm64AutoReleaseDelayMs, std::uint8_t sm64DpadDeadzone, std::uint16_t cursorSpeedLimit,
                    const widemelon::LeftStickCalibration &leftStickCalibration,
                    const std::vector<MenuItem> &items, int selected, int scrollOffset)
    {
        const int centerX = width / 2;
        const int fieldWidth = widemelon::UiFormFieldWidth;
        const int fieldHeight = widemelon::UiFormFieldHeight;
        const int fieldX = centerX - fieldWidth / 2;
        const std::string title = menuTitle(page);
        const int listTop = menuListTop();
        const int listBottom = menuListBottom(height);
        const std::string_view description = widemelon::display::pages::leftStickModeDescription(leftStickMode);
        widemelon::display::MenuRenderContext renderContext;
        renderContext.page.width = width;
        renderContext.page.height = height;
        renderContext.page.drawBackground = [=]() { drawGradientBackground(renderer, width, height); };
        renderContext.renderer = renderer;
        renderContext.listTop = listTop;
        renderContext.listBottom = listBottom;
        renderContext.scrollOffset = scrollOffset;
        for (std::size_t index = 0; index < items.size(); ++index)
            renderContext.items.push_back({menuItemTop(items, index, leftStickMode), menuItemHeight(items[index], leftStickMode)});
        renderContext.renderHeader = [&]()
        {
            drawMenuText(renderer, centerX, 36, title, palette.primary, themeText.titleFontSize);
        };
        renderContext.renderItem = [&](std::size_t index, int rowY)
        {

            switch (items[index])
            {
            case MenuItem::SectionConnect: drawMenuText(renderer, centerX, rowY, "Connect", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionSettings: drawMenuText(renderer, centerX, rowY, "Settings", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionTheme: drawMenuText(renderer, centerX, rowY, "Theme", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionAnalogSticks: drawMenuText(renderer, centerX, rowY, "Analog sticks", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionNavigation: drawMenuText(renderer, centerX, rowY, "Navigation", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomeHostLabel: drawMenuText(renderer, centerX, rowY, "Server Address", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomePortLabel: drawMenuText(renderer, centerX, rowY, "Port", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomeSessionCodeLabel: drawMenuText(renderer, centerX, rowY, "Session code", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickModeHint:
                drawMenuMultilineText(renderer, centerX, rowY, "Select the desired behaviour\nfor the left stick while in-game.", palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::LeftStickDescription:
                drawMenuMultilineText(renderer, centerX, rowY, description, palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::Sm64AutoFramesLabel: drawMenuText(renderer, centerX, rowY, "SM64 Auto delay", palette.primary, themeText.formFontSize); return;
            case MenuItem::Sm64AutoReleaseDelayLabel: drawMenuText(renderer, centerX, rowY, "SM64 Auto release delay", palette.primary, themeText.formFontSize); return;
            case MenuItem::Sm64DpadDeadzoneLabel: drawMenuText(renderer, centerX, rowY, "SM64 D-pad deadzone", palette.primary, themeText.formFontSize); return;
            case MenuItem::CursorSpeedLabel: drawMenuText(renderer, centerX, rowY, "Cursor speed limit", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickCalibrationHint:
                drawMenuMultilineText(renderer, centerX, rowY, "Adjust the range of the left stick\nto correct asymmetrical inputs.\n(experimental feature)", palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::LeftStickScaleUpLabel: drawMenuText(renderer, centerX, rowY, "Top multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleDownLabel: drawMenuText(renderer, centerX, rowY, "Bottom multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleLeftLabel: drawMenuText(renderer, centerX, rowY, "Left multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleRightLabel: drawMenuText(renderer, centerX, rowY, "Right multiplier", palette.primary, themeText.formFontSize); return;
            default: break;
            }

            std::string value;
            bool adjustable = false;
            switch (items[index])
            {
            case MenuItem::HomeHost: value = config.host; break;
            case MenuItem::HomePort: value = std::to_string(config.port); break;
            case MenuItem::HomeSessionCode: value = config.pairingCode; break;
            case MenuItem::HomeConnect: value = "Connect"; break;
            case MenuItem::HomeSettings: value = "Settings"; break;
            case MenuItem::Theme: value = theme; adjustable = true; break;
            case MenuItem::LeftStickMod: value = "Left stick mod"; break;
            case MenuItem::LeftStickCalibration: value = "Left stick calibration"; break;
            case MenuItem::LeftStickMode: value = leftStickMode; adjustable = true; break;
            case MenuItem::Sm64AutoFrames: value = std::to_string(sm64AutoFrames) + " frames"; adjustable = true; break;
            case MenuItem::Sm64AutoReleaseDelay: value = std::to_string(sm64AutoReleaseDelayMs) + " ms"; adjustable = true; break;
            case MenuItem::Sm64DpadDeadzone: value = std::to_string(sm64DpadDeadzone) + " %"; adjustable = true; break;
            case MenuItem::CursorSpeed: value = std::to_string(cursorSpeedLimit) + " px/s"; adjustable = true; break;
            case MenuItem::LeftStickScaleUp: value = std::to_string(leftStickCalibration.up) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleDown: value = std::to_string(leftStickCalibration.down) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleLeft: value = std::to_string(leftStickCalibration.left) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleRight: value = std::to_string(leftStickCalibration.right) + " %"; adjustable = true; break;
            case MenuItem::Back: value = "Back"; break;
            case MenuItem::Quit: value = "Quit"; break;
            default: break;
            }

            const SDL_Rect field{fieldX, rowY, fieldWidth, fieldHeight};
            if (adjustable)
                drawAdjustableValue(renderer, field, value, static_cast<int>(index) == selected);
            else
            {
                drawPill(renderer, field, static_cast<int>(index) == selected);
                drawMenuText(renderer, centerX, field.y + (field.h - themeText.formFontSize) / 2, value, palette.primary, themeText.formFontSize);
            }
        };

        renderContext.renderFooter = [&]()
        {
            const int hintY = menuFooterTop(height);
            if (page == MenuPage::Home)
            {
                const int formHintWidth = controlHintWidth(uiTextures.hintA, "A", "EDIT", themeText.hintFontSize)
                    + controlHintWidth(uiTextures.hintStart, "START", "CONNECT", themeText.hintFontSize);
                int hintX = width - formHintWidth - 20;
                hintX = drawControlHint(renderer, uiTextures.hintA, "A", "EDIT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintStart, "START", "CONNECT", hintX, hintY, themeText.hintFontSize);
            }
            else if (page == MenuPage::Settings)
            {
                const int footerWidth = controlHintWidth(uiTextures.hintA, "A", "SELECT", themeText.hintFontSize)
                    + controlHintWidth(uiTextures.hintB, "B", "BACK", themeText.hintFontSize);
                int hintX = width - footerWidth - 20;
                hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintB, "B", "BACK", hintX, hintY, themeText.hintFontSize);
            }
            else
            {
                const int footerWidth = (page == MenuPage::LeftStickMod ? controlHintWidth(uiTextures.hintA, "A", "SELECT", themeText.hintFontSize) : 0)
                    + controlHintWidth(uiTextures.hintB, "B", "BACK", themeText.hintFontSize);
                int hintX = width - footerWidth - 20;
                if (page == MenuPage::LeftStickMod)
                    hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintB, "B", "BACK", hintX, hintY, themeText.hintFontSize);
            }
        };
        renderContext.present = [=]() { SDL_RenderPresent(renderer); };
        widemelon::display::PageRenderer::renderMenu(renderContext);
    }

    SettingsResult editSettings(SDL_Renderer *renderer, int width, int height, const widemelon::Config &config, widemelon::EvdevInput &input)
    {
        std::array<char, 4096> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
        if (length <= 0) return SettingsResult::Home;
        executable[static_cast<std::size_t>(length)] = 0;
        const std::string directory = std::filesystem::path(executable.data()).parent_path().string();
        const widemelon::display::UiConfig uiConfig = uiConfigFor(directory);
        std::vector<std::string> themes = availableThemes(directory);
        if (themes.empty()) return SettingsResult::Home;
        std::size_t themeIndex = 0;
        const std::string currentTheme = uiConfig.readValue(UiConfigThemeKey).value_or("DukuSlice");
        const auto current = std::find(themes.begin(), themes.end(), currentTheme);
        if (current != themes.end()) themeIndex = static_cast<std::size_t>(std::distance(themes.begin(), current));

        const std::array<std::string, 6> leftStickModes{"Disabled", "D-pad", "SM64 Auto", "SM64 Manual", "SM64 D-pad", "Cursor"};
        std::size_t leftStickModeIndex = 0;
        const std::string currentLeftStickMode = uiConfig.readValue(UiConfigLeftStickModeKey).value_or("D-pad");
        const auto currentLeftStick = std::find(leftStickModes.begin(), leftStickModes.end(), currentLeftStickMode);
        if (currentLeftStick != leftStickModes.end())
            leftStickModeIndex = static_cast<std::size_t>(std::distance(leftStickModes.begin(), currentLeftStick));
        else if (currentLeftStickMode == "SM64")
            leftStickModeIndex = 2;

        std::uint8_t sm64AutoFrames = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64AutoFramesKey, widemelon::Sm64TouchCenterHoldFramesDefault,
                                                                                  widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum));
        std::uint16_t sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigSm64AutoReleaseDelayKey, widemelon::Sm64TouchReleaseDelayDefaultMs,
                                                                                            widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs));
        std::uint16_t cursorSpeedLimit = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigCursorSpeedKey, widemelon::CursorSpeedDefault,
                                                                                      widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum));
        std::uint8_t sm64DpadDeadzone = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64DpadDeadzoneKey, widemelon::Sm64DpadDeadzoneDefaultPercent,
                                                                                    widemelon::Sm64DpadDeadzoneMinimumPercent, widemelon::Sm64DpadDeadzoneMaximumPercent));
        widemelon::LeftStickCalibration leftStickCalibration{
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleLeftKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleRightKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleUpKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleDownKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
        };
        MenuPage page = MenuPage::Settings;
        int selected = 1;
        int scrollOffset = 0;
        widemelon::display::MenuNavigation navigation;
        while (true)
        {
            const std::string &leftStickMode = leftStickModes[leftStickModeIndex];
            const std::vector<MenuItem> items = menuItems(page, leftStickMode);
            if (!items.empty())
            {
                selected = std::min(selected, static_cast<int>(items.size()) - 1);
                scrollOffset = menuScrollOffset(items, leftStickMode, selected, scrollOffset, height);
            }
            renderMenu(renderer, width, height, page, config, themes[themeIndex], leftStickMode, sm64AutoFrames, sm64AutoReleaseDelayMs,
                           sm64DpadDeadzone, cursorSpeedLimit, leftStickCalibration, items, selected, scrollOffset);

            input.pollEvent();
            if (input.exitComboPressed()) return SettingsResult::Exit;
            const auto action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
            if (action == widemelon::UiAction::Back || action == widemelon::UiAction::Delete)
            {
                if (page == MenuPage::Settings) return SettingsResult::Home;
                page = MenuPage::Settings;
                selected = 1;
                scrollOffset = 0;
                continue;
            }
            if (items.empty())
            {
                SDL_Delay(10);
                continue;
            }

            const std::vector<widemelon::display::MenuNavigationItem> navigationItems = menuNavigationItems(items, leftStickMode);
            if (action == widemelon::UiAction::Up) selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, -1);
            else if (action == widemelon::UiAction::Down) selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, 1);
            else if (action == widemelon::UiAction::Left || action == widemelon::UiAction::Right)
            {
                const int direction = action == widemelon::UiAction::Left ? -1 : 1;
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::Theme:
                {
                    const std::size_t nextTheme = static_cast<std::size_t>((static_cast<int>(themeIndex) + direction + static_cast<int>(themes.size())) % static_cast<int>(themes.size()));
                    if (uiConfig.writeValue(UiConfigThemeKey, themes[nextTheme]))
                    {
                        themeIndex = nextTheme;
                        reloadUiResources(renderer);
                    }
                    break;
                }
                case MenuItem::LeftStickMode:
                {
                    const std::size_t nextMode = static_cast<std::size_t>((static_cast<int>(leftStickModeIndex) + direction + static_cast<int>(leftStickModes.size())) % static_cast<int>(leftStickModes.size()));
                    if (uiConfig.writeValue(UiConfigLeftStickModeKey, leftStickModes[nextMode])) leftStickModeIndex = nextMode;
                    break;
                }
                case MenuItem::Sm64AutoFrames:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoFrames) + direction, static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMinimum), static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMaximum));
                    if (uiConfig.writeInt(UiConfigSm64AutoFramesKey, next, widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum)) sm64AutoFrames = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::Sm64AutoReleaseDelay:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoReleaseDelayMs) + direction * static_cast<int>(widemelon::Sm64TouchReleaseDelayStepMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMinimumMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMaximumMs));
                    if (uiConfig.writeInt(UiConfigSm64AutoReleaseDelayKey, next, widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs)) sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(next);
                    break;
                }
                case MenuItem::Sm64DpadDeadzone:
                {
                    const int next = std::clamp(static_cast<int>(sm64DpadDeadzone) + direction, static_cast<int>(widemelon::Sm64DpadDeadzoneMinimumPercent), static_cast<int>(widemelon::Sm64DpadDeadzoneMaximumPercent));
                    if (uiConfig.writeInt(UiConfigSm64DpadDeadzoneKey, next, widemelon::Sm64DpadDeadzoneMinimumPercent, widemelon::Sm64DpadDeadzoneMaximumPercent)) sm64DpadDeadzone = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::CursorSpeed:
                {
                    const int next = std::clamp(static_cast<int>(cursorSpeedLimit) + direction * static_cast<int>(widemelon::CursorSpeedStep), static_cast<int>(widemelon::CursorSpeedMinimum), static_cast<int>(widemelon::CursorSpeedMaximum));
                    if (uiConfig.writeInt(UiConfigCursorSpeedKey, next, widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum)) cursorSpeedLimit = static_cast<std::uint16_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleUp:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.up) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleUpKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.up = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleDown:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.down) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleDownKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.down = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleLeft:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.left) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleLeftKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.left = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleRight:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.right) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleRightKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.right = static_cast<std::uint8_t>(next);
                    break;
                }
                default: break;
                }
            }
            else if (action == widemelon::UiAction::Confirm)
            {
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::LeftStickMod:
                    page = MenuPage::LeftStickMod;
                    selected = 2;
                    scrollOffset = 0;
                    break;
                case MenuItem::LeftStickCalibration:
                    page = MenuPage::LeftStickCalibration;
                    selected = 2;
                    scrollOffset = 0;
                    break;
                case MenuItem::Back:
                    if (page == MenuPage::Settings) return SettingsResult::Home;
                    page = MenuPage::Settings;
                    selected = 1;
                    scrollOffset = 0;
                    break;
                case MenuItem::Quit: return SettingsResult::Exit;
                default: break;
                }
            }
            SDL_Delay(10);
        }
    }

    bool editConfiguration(SDL_Renderer *renderer, int width, int height, widemelon::Config &config,
                           widemelon::EvdevInput &input)
    {
        const std::vector<MenuItem> items = menuItems(MenuPage::Home, "");
        int selected = 0;
        int scrollOffset = 0;
        widemelon::display::MenuNavigation navigation;
        while (true)
        {
            const std::vector<widemelon::display::MenuNavigationItem> navigationItems = menuNavigationItems(items);
            scrollOffset = menuScrollOffset(items, {}, selected, scrollOffset, height);
            renderMenu(renderer, width, height, MenuPage::Home, config, "", "", 0, 0, 0, 0, widemelon::LeftStickCalibration{}, items, selected, scrollOffset);

            input.pollEvent();
            if (input.exitComboPressed()) return false;
            const widemelon::UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
            if (action == widemelon::UiAction::Up)
                selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, -1);
            else if (action == widemelon::UiAction::Down)
                selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, 1);
            else if (action == widemelon::UiAction::Confirm || action == widemelon::UiAction::Start)
            {
                if (action == widemelon::UiAction::Start)
                    selected = 7;
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::HomeHost:
                    widemelon::display::NumpadScreen::edit(renderer, width, height, input, "HOST ADDRESS", config.host, 15, true);
                    break;
                case MenuItem::HomePort:
                {
                    std::string port = std::to_string(config.port);
                    if (widemelon::display::NumpadScreen::edit(renderer, width, height, input, "PORT", port, 5, false))
                    {
                        unsigned int parsed = 0;
                        const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed);
                        if (result.ec == std::errc{} && result.ptr == port.data() + port.size() && parsed <= 65535)
                            config.port = static_cast<std::uint16_t>(parsed);
                        else
                            widemelon::Logger::error("Configuration form error: invalid port");
                    }
                    break;
                }
                case MenuItem::HomeSessionCode:
                    widemelon::display::NumpadScreen::edit(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false);
                    break;
                case MenuItem::HomeConnect:
                {
                    const widemelon::ConfigLoadResult checked = widemelon::ConfigLoader::validate(config);
                    if (!checked.ok)
                    {
                        widemelon::Logger::error("Configuration form error: " + checked.error);
                        break;
                    }
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
                case MenuItem::HomeSettings:
                    if (editSettings(renderer, width, height, config, input) == SettingsResult::Exit)
                        return false;
                    break;
                default:
                    break;
                }
            }
            SDL_Delay(10);
        }
    }


}

namespace widemelon
{

    bool runDisplaySession(Config config, bool inputTest, std::string &error)
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
            widemelon::display::closeUiResources();
#ifdef WIDEMELON_HAVE_SDL_IMAGE
            IMG_Quit();
#endif
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        if (!widemelon::display::loadUiFont(14))
            Logger::error("Cannot load UI font: " + widemelon::display::uiFontPath() + "; " + TTF_GetError());
        else
            Logger::info("Loaded UI font: " + widemelon::display::uiFontPath());
#endif

        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);

        EvdevInput exitInput;
        std::string inputError;
        if (!exitInput.open("/dev/input/event4", inputError))
        {
            error = "Cannot open controller input: " + inputError;
            widemelon::display::closeUiResources();
#ifdef WIDEMELON_HAVE_SDL_TTF
            TTF_Quit();
#endif
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
            widemelon::display::closeUiResources();
#ifdef WIDEMELON_HAVE_SDL_IMAGE
            IMG_Quit();
#endif
#ifdef WIDEMELON_HAVE_SDL_TTF
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
        widemelon::Sm64DpadMod sm64Dpad;
        bool sm64Enabled = false;
        bool sm64ManualEnabled = false;
        bool cursorEnabled = false;
        bool sm64DpadEnabled = false;
        widemelon::LeftStickCalibration leftStickCalibration;
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
                    const std::vector<MenuItem> homeItems = menuItems(MenuPage::Home, "");
                    renderMenu(renderer, width, height, MenuPage::Home, config, "", "", 0, 0, 0, 0, widemelon::LeftStickCalibration{}, homeItems, 4, 0);
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
                const widemelon::display::UiConfig uiConfig = uiConfigFor(directory);
                const std::string leftStickMode = uiConfig.readValue(UiConfigLeftStickModeKey).value_or("D-pad");
                leftStickCalibration.left = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleLeftKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent));
                leftStickCalibration.right = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleRightKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent));
                leftStickCalibration.up = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleUpKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent));
                leftStickCalibration.down = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleDownKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent));
                sm64Enabled = leftStickMode == "SM64 Auto" || leftStickMode == "SM64";
                sm64ManualEnabled = leftStickMode == "SM64 Manual";
                cursorEnabled = leftStickMode == "Cursor";
                sm64DpadEnabled = leftStickMode == "SM64 D-pad";
                sm64Stick.setCenterHoldFrames(static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64AutoFramesKey, widemelon::Sm64TouchCenterHoldFramesDefault,
                                                                                          widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum)));
                sm64Stick.setReleaseDelayMs(static_cast<std::uint16_t>(uiConfig.readInt(UiConfigSm64AutoReleaseDelayKey, widemelon::Sm64TouchReleaseDelayDefaultMs,
                                                                                         widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs)));
                sm64Stick.setEnabled(sm64Enabled);
                sm64ManualStick.setEnabled(sm64ManualEnabled);
                cursorStick.setSpeedLimit(static_cast<std::uint16_t>(uiConfig.readInt(UiConfigCursorSpeedKey, widemelon::CursorSpeedDefault,
                                                                                       widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum)));
                cursorStick.setEnabled(cursorEnabled);
                sm64Dpad.setDeadzonePercent(static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64DpadDeadzoneKey, widemelon::Sm64DpadDeadzoneDefaultPercent,
                                                                                         widemelon::Sm64DpadDeadzoneMinimumPercent, widemelon::Sm64DpadDeadzoneMaximumPercent)));
                sm64Dpad.setEnabled(sm64DpadEnabled);
                exitInput.setLeftStickDpadEnabled(leftStickMode == "D-pad" || sm64DpadEnabled);
                if (sm64DpadEnabled) exitInput.setLeftStickDpadThresholdFraction(widemelon::Sm64DpadDirectionalThreshold);
                if (sm64Enabled) Logger::info("SM64 Auto stick mod enabled");
                if (sm64ManualEnabled) Logger::info("SM64 Manual stick mod enabled");
                if (cursorEnabled) Logger::info("Cursor stick mod enabled");
                if (sm64DpadEnabled) Logger::info("SM64 D-pad stick mod enabled");
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
            const widemelon::LeftStickState calibratedStick = widemelon::applyLeftStickCalibration(exitInput.leftStickState(), leftStickCalibration);
            const bool sm64TouchChanged = sm64Stick.update(calibratedStick);
            const bool sm64ManualTouchChanged = sm64ManualStick.update(calibratedStick, exitInput.r2Pressed());
            const bool cursorTouchChanged = cursorStick.update(calibratedStick, exitInput.r2Pressed());
            const bool sm64DpadChanged = sm64Dpad.update(calibratedStick);
            inputDirty = exitInput.takeStateChanged() || sm64TouchChanged || sm64ManualTouchChanged || cursorTouchChanged || sm64DpadChanged || inputDirty;
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
                    client->sendInputSnapshot(sequence, static_cast<std::uint16_t>(exitInput.buttonMask() | sm64Dpad.additionalButtons()),
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
