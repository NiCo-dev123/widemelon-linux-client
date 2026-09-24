#include "display/ConfigScreen.h"

#include "common/Logger.h"
#include "input/EvdevInput.h"
#include "network/WebSocketClient.h"

#include <SDL.h>
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
#include <fstream>
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
        SDL_Color background{15, 18, 36, 255};
        SDL_Color primary{255, 255, 255, 255};
    };

    Palette palette;

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

    void loadUiResources()
    {
        std::array<char, 4096> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
        if (length <= 0)
            return;
        executable[static_cast<std::size_t>(length)] = '\0';
        const std::string directory = std::string(executable.data()).substr(0, std::string(executable.data()).find_last_of('/'));
        std::ifstream file(directory + "/widemelon-client-ui.conf");
        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos)
                continue;
            SDL_Color color{};
            if (!parseColor(line.substr(separator + 1), color))
                continue;
            if (line.substr(0, separator) == "background")
                palette.background = color;
            else if (line.substr(0, separator) == "primary")
                palette.primary = color;
        }
#ifdef WIDEMELON_HAVE_SDL_TTF
        fontPath = directory + "/assets/fonts/Roboto-Regular.ttf";
#endif
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

    int textWidth(std::string_view text, int scale)
    {
#ifdef WIDEMELON_HAVE_SDL_TTF
        int width = 0;
        if (TTF_Font *font = fontForSize(scale * 7))
        {
            TTF_SizeUTF8(font, std::string(text).c_str(), &width, nullptr);
            return width;
        }
#endif
        return static_cast<int>(text.size()) * 6 * scale - scale;
    }

    void drawText(SDL_Renderer *renderer, std::string_view text, int x, int y, int scale)
    {
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (TTF_Font *font = fontForSize(scale * 7))
        {
            const std::string rendered(text);
            SDL_Surface *surface = TTF_RenderUTF8_Blended(font, rendered.c_str(), palette.primary);
            if (surface)
            {
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                const SDL_Rect destination{x, y, surface->w, surface->h};
                if (texture)
                    SDL_RenderCopy(renderer, texture, nullptr, &destination);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
                return;
            }
        }
#endif
        for (char character : text)
        {
            const Glyph &glyph = glyphFor(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            for (int row = 0; row < 7; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    if ((glyph[row] & (1U << (4 - column))) == 0)
                        continue;
                    const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
            x += 6 * scale;
        }
    }

    void drawTextColored(SDL_Renderer *renderer, std::string_view text, int x, int y, int scale, SDL_Color color)
    {
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (TTF_Font *font = fontForSize(scale * 7))
        {
            const std::string rendered(text);
            SDL_Surface *surface = TTF_RenderUTF8_Blended(font, rendered.c_str(), color);
            if (surface)
            {
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                const SDL_Rect destination{x, y, surface->w, surface->h};
                if (texture)
                    SDL_RenderCopy(renderer, texture, nullptr, &destination);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
                return;
            }
        }
#endif
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        drawText(renderer, text, x, y, scale);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
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
                SDL_Texture *videoTexture)
    {
        (void)height;
        (void)config;
        SDL_SetRenderDrawColor(renderer, palette.background.r, palette.background.g, palette.background.b, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        const std::string title = "WideMelon Client";
        drawText(renderer, title, (width - textWidth(title, 4)) / 2, 20, 4);
        const SDL_Rect video{(width - 768) / 2, 70, 768, 576};
        if (videoTexture)
            SDL_RenderCopy(renderer, videoTexture, nullptr, &video);
        else
            SDL_RenderFillRect(renderer, &video);
        const std::string connection = "Connection status: " + status;
        const std::string exit = "Press START + R + L to quit";
        drawText(renderer, connection, video.x, video.y + video.h + 16, 2);
        drawText(renderer, exit, video.x, video.y + video.h + 38, 2);
        SDL_RenderPresent(renderer);
    }

    void renderSetup(SDL_Renderer *renderer, int width, int height, const widemelon::Config &config,
                     int selected, const std::string &status)
    {
        (void)height;
        SDL_SetRenderDrawColor(renderer, palette.background.r, palette.background.g, palette.background.b, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        const std::string title = "WideMelon Client";
        drawText(renderer, title, (width - textWidth(title, 4)) / 2, 48, 4);
        const int centerX = width / 2;
        const int fieldWidth = 320;
        const int fieldHeight = 34;
        const int fieldX = centerX - fieldWidth / 2;
        const std::array<std::string, 3> labels{"Server Address", "Port", "Session code"};
        const std::array<std::string, 3> values{config.host, std::to_string(config.port), config.pairingCode};
        const std::array<int, 3> positions{128, 214, 300};
        for (int index = 0; index < 3; ++index)
        {
            drawText(renderer, labels[index], centerX - textWidth(labels[index], 2) / 2, positions[index] - 31, 2);
            const SDL_Rect field{fieldX, positions[index], fieldWidth, fieldHeight};
            if (selected == index)
                SDL_RenderFillRect(renderer, &field);
            else
                SDL_RenderDrawRect(renderer, &field);
            if (selected == index)
            {
                drawTextColored(renderer, values[index], centerX - textWidth(values[index], 2) / 2,
                                positions[index] + 7, 2, palette.background);
            }
            else
                drawText(renderer, values[index], centerX - textWidth(values[index], 2) / 2, positions[index] + 7, 2);
        }
        const SDL_Rect connect{fieldX, 402, fieldWidth, fieldHeight};
        if (selected == 3)
            SDL_RenderFillRect(renderer, &connect);
        else
            SDL_RenderDrawRect(renderer, &connect);
        if (selected == 3)
        {
            const std::string connectLabel = "CONNECT";
            drawTextColored(renderer, connectLabel, centerX - textWidth(connectLabel, 2) / 2, 409, 2, palette.background);
        }
        else
        {
            const std::string connectLabel = "CONNECT";
            drawText(renderer, connectLabel, centerX - textWidth(connectLabel, 2) / 2, 409, 2);
        }
        drawText(renderer, status, centerX - textWidth(status, 2) / 2, 470, 2);
        const std::string hint = "A EDIT   START CONNECT";
        drawText(renderer, hint, centerX - textWidth(hint, 2) / 2, 510, 2);
        SDL_RenderPresent(renderer);
    }

    void renderKeyboard(SDL_Renderer *renderer, int width, int height, const std::string &title,
                        const std::string &value, int selectedKey)
    {
        static const std::array<const char *, 13> keys{
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};
        constexpr int columns = 4;
        constexpr int keyWidth = 170;
        constexpr int keyHeight = 62;
        constexpr int startX = 290;
        constexpr int startY = 205;

        SDL_SetRenderDrawColor(renderer, palette.background.r, palette.background.g, palette.background.b, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        drawText(renderer, title, (width - textWidth(title, 5)) / 2, 65, 5);
        drawText(renderer, value.empty() ? "_" : value, (width - textWidth(value.empty() ? "_" : value, 5)) / 2, 125, 5);
        for (std::size_t index = 0; index < keys.size(); ++index)
        {
            const int row = static_cast<int>(index) / columns;
            const int column = static_cast<int>(index) % columns;
            const SDL_Rect key{startX + column * keyWidth, startY + row * keyHeight, keyWidth - 10, keyHeight - 8};
            if (static_cast<int>(index) == selectedKey)
                SDL_RenderDrawRect(renderer, &key);
            const int scale = std::string_view(keys[index]).size() > 1 ? 4 : 5;
            drawText(renderer, keys[index], key.x + (key.w - textWidth(keys[index], scale)) / 2,
                     key.y + (key.h - 7 * scale) / 2, scale);
        }
        drawText(renderer, "A SELECT B DELETE X BACK START OK", (width - textWidth("A SELECT B DELETE X BACK START OK", 2)) / 2, height - 60, 2);
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

    bool editConfiguration(SDL_Renderer *renderer, int width, int height, widemelon::Config &config,
                           widemelon::EvdevInput &input)
    {
        int selected = 0;
        std::string status = "EDIT A FIELD THEN CONNECT";
        auto nextNavigationAt = std::chrono::steady_clock::time_point{};
        UiNavigationRepeater navigationRepeater;
        while (true)
        {
            renderSetup(renderer, width, height, config, selected, status);
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
                selected = (selected + 3) % 4;
            else if (acceptNavigation && action == widemelon::UiAction::Down)
                selected = (selected + 1) % 4;
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
                            status = "INVALID PORT";
                    }
                }
                else if (selected == 2)
                    editNumericField(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false);
                else
                {
                    const widemelon::ConfigLoadResult checked = widemelon::ConfigLoader::validate(config);
                    if (!checked.ok)
                    {
                        status = "INVALID CONFIG";
                        widemelon::Logger::error("Configuration form error: " + checked.error);
                    }
                    else
                    {
                        std::string error;
                        if (widemelon::ConfigLoader::saveNextToExecutable(config, error))
                        {
                            widemelon::Logger::info("Configuration saved from setup form");
                            return true;
                        }
                        // Persistence is optional: the values entered in this
                        // session remain valid and must not prevent connecting.
                        status = "NOT SAVED - CONTINUING";
                        widemelon::Logger::error("Configuration save warning: " + error);
                        return true;
                    }
                }
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

        loadUiResources();
#ifdef WIDEMELON_HAVE_SDL_TTF
        if (TTF_Init() != 0)
        {
            error = TTF_GetError();
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
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        SDL_Texture *videoTexture = nullptr;
        auto closeDisplay = [&]
        {
            SDL_DestroyTexture(videoTexture);
#ifdef WIDEMELON_HAVE_SDL_TTF
            closeFonts();
            TTF_Quit();
#endif
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        };

        std::unique_ptr<widemelon::WebSocketClient> client = std::make_unique<widemelon::WebSocketClient>();
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
                    renderSetup(renderer, width, height, config, 3, "CHECKING CONNECTION...");
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

        std::string status = inputTest ? "INPUT TEST ACTIVE" : "CONNECTION OK";
        if (inputTest)
        {
            connection = std::async(std::launch::async, []
                                    { return std::string{}; });
        }
        render(renderer, width, height, config, status, videoTexture);

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
                        render(renderer, width, height, config, status, videoTexture);
                    }
                    break;
                default:
                    break;
                }
            }
            const std::string inputEvent = exitInput.pollEvent();
            inputDirty = exitInput.takeStateChanged() || inputDirty;
            if (inputTest && !inputEvent.empty())
            {
                status = inputEvent;
                render(renderer, width, height, config, status, videoTexture);
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
                render(renderer, width, height, config, status, videoTexture);
                displayedConnectionState = currentConnectionState;
            }
            if (!inputTest && connection.valid() && connection.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                const std::string result = connection.get();
                if (result != "Cancelled")
                {
                    status = "CONNECTION ERROR";
                    render(renderer, width, height, config, status, videoTexture);
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
                    render(renderer, width, height, config, status, videoTexture);
                }
                if (sendReleasedSnapshot)
                {
                    client->sendInputSnapshot(++inputSequence, 0);
                    sendReleasedSnapshot = false;
                }
                else if (inputDirty || now >= nextInputSnapshot)
                {
                    client->sendInputSnapshot(++inputSequence, exitInput.buttonMask());
                    inputDirty = false;
                    nextInputSnapshot = now + std::chrono::milliseconds(200);
                }

                DecodedVideoFrame decodedFrame;
                if (client->latestDecodedVideoFrame(decodedFrame) && decodedFrame.sequence != displayedVideoSequence && updateVideoTexture(renderer, videoTexture, decodedFrame))
                {
                    displayedVideoSequence = decodedFrame.sequence;
                    render(renderer, width, height, config, status, videoTexture);
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
            client->sendInputSnapshot(++inputSequence, 0);
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
