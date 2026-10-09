#include "display/ConfigScreen.h"
#include "display/DisplaySession.h"
#include "display/GameplayScreen.h"
#include "display/MenuNavigation.h"
#include "display/MenuScreen.h"
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
                if (!display::MenuScreen::editConfiguration(renderer, width, height, config, exitInput))
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
                    display::MenuScreen::renderConnecting(renderer, width, height, config);
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
        widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);

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
                        widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
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
                widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, &sm64TouchState());
            if (inputTest && !inputEvent.empty())
            {
                status = inputEvent;
                widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
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
                widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                displayedConnectionState = currentConnectionState;
            }
            if (!inputTest && connection.valid() && connection.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                const std::string result = connection.get();
                if (result != "Cancelled")
                {
                    status = "CONNECTION ERROR";
                    widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
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
                    widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
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
                if (client->latestDecodedVideoFrame(decodedFrame) && decodedFrame.sequence != displayedVideoSequence && widemelon::display::GameplayScreen::updateVideoTexture(renderer, videoTexture, decodedFrame))
                {
                    displayedVideoSequence = decodedFrame.sequence;
                    widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
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
