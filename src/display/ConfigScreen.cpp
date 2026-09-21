#include "display/ConfigScreen.h"

#include "common/Logger.h"
#include "input/EvdevInput.h"
#include "network/WebSocketClient.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <future>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

using Glyph = std::array<std::uint8_t, 7>;

const Glyph& glyphFor(char character)
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
    return static_cast<int>(text.size()) * 6 * scale - scale;
}

void drawText(SDL_Renderer* renderer, std::string_view text, int x, int y, int scale)
{
    for (char character : text)
    {
        const Glyph& glyph = glyphFor(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
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

void render(SDL_Renderer* renderer, int width, int height, const widemelon::Config& config, const std::string& status)
{
    const std::vector<std::string> lines{
        "WIDEMELON LINUX CLIENT",
        "CONFIGURATION LOADED",
        "",
        "HOST: " + config.host,
        "PORT: " + std::to_string(config.port),
        "PAIRING CODE: " + config.pairingCode,
        "",
        status,
        "",
        "HOLD START + L + R TO EXIT",
    };
    const int widest = static_cast<int>(std::max_element(lines.begin(), lines.end(),
        [](const std::string& left, const std::string& right) { return left.size() < right.size(); })->size());
    const int scale = std::max(1, std::min(width / (widest * 6 + 4), height / 80));
    const int lineHeight = 9 * scale;
    const int blockHeight = static_cast<int>(lines.size()) * lineHeight;

    SDL_SetRenderDrawColor(renderer, 11, 18, 32, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 115, 216, 164, 255);
    int y = (height - blockHeight) / 2;
    for (const std::string& line : lines)
    {
        drawText(renderer, line, (width - textWidth(line, scale)) / 2, y, scale);
        y += lineHeight;
    }
    SDL_RenderPresent(renderer);
}

void renderSetup(SDL_Renderer* renderer, int width, int height, const widemelon::Config& config,
    int selected, const std::string& status)
{
    const std::vector<std::string> lines{
        "WIDEMELON SETUP",
        "HOST: " + config.host,
        "PORT: " + std::to_string(config.port),
        "CODE: " + config.pairingCode,
        "CONNECT",
        "",
        status,
        "A EDIT   START CONNECT",
        "HOLD START L R TO EXIT",
    };
    const int widest = static_cast<int>(std::max_element(lines.begin(), lines.end(),
        [](const std::string& left, const std::string& right) { return left.size() < right.size(); })->size());
    const int scale = std::max(1, std::min(width / (widest * 6 + 4), height / 90));
    const int lineHeight = 9 * scale;
    const int blockHeight = static_cast<int>(lines.size()) * lineHeight;
    const int startY = (height - blockHeight) / 2;

    SDL_SetRenderDrawColor(renderer, 11, 18, 32, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 115, 216, 164, 255);
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const int y = startY + static_cast<int>(index) * lineHeight;
        const int textX = (width - textWidth(lines[index], scale)) / 2;
        if (static_cast<int>(index) == selected + 1)
        {
            const SDL_Rect border{textX - 7, y - 3, textWidth(lines[index], scale) + 14, lineHeight - 1};
            SDL_RenderDrawRect(renderer, &border);
        }
        drawText(renderer, lines[index], textX, y, scale);
    }
    SDL_RenderPresent(renderer);
}

void renderKeyboard(SDL_Renderer* renderer, int width, int height, const std::string& title,
    const std::string& value, int selectedKey)
{
    static const std::array<const char*, 13> keys{
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};
    constexpr int columns = 4;
    constexpr int keyWidth = 170;
    constexpr int keyHeight = 62;
    constexpr int startX = 290;
    constexpr int startY = 205;

    SDL_SetRenderDrawColor(renderer, 11, 18, 32, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 115, 216, 164, 255);
    drawText(renderer, title, (width - textWidth(title, 5)) / 2, 65, 5);
    drawText(renderer, value.empty() ? "_" : value, (width - textWidth(value.empty() ? "_" : value, 5)) / 2, 125, 5);
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const int row = static_cast<int>(index) / columns;
        const int column = static_cast<int>(index) % columns;
        const SDL_Rect key{startX + column * keyWidth, startY + row * keyHeight, keyWidth - 10, keyHeight - 8};
        if (static_cast<int>(index) == selectedKey) SDL_RenderDrawRect(renderer, &key);
        const int scale = std::string_view(keys[index]).size() > 1 ? 4 : 5;
        drawText(renderer, keys[index], key.x + (key.w - textWidth(keys[index], scale)) / 2,
            key.y + (key.h - 7 * scale) / 2, scale);
    }
    drawText(renderer, "A SELECT  B CANCEL", (width - textWidth("A SELECT  B CANCEL", 3)) / 2, height - 60, 3);
    SDL_RenderPresent(renderer);
}

bool editNumericField(SDL_Renderer* renderer, int width, int height, widemelon::EvdevInput& input,
    const std::string& title, std::string& value, std::size_t maximumLength, bool allowDot)
{
    static constexpr int keyCount = 13;
    static constexpr int columns = 4;
    static const std::array<const char*, keyCount> keys{
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", "DEL", "OK"};
    const std::string original = value;
    int selectedKey = 0;
    while (true)
    {
        renderKeyboard(renderer, width, height, title, value, selectedKey);
        input.pollEvent();
        if (input.exitComboPressed()) return false;
        switch (input.takeUiAction())
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
        case widemelon::UiAction::Back:
            value = original;
            return false;
        case widemelon::UiAction::Confirm:
        case widemelon::UiAction::Start:
        {
            const std::string_view key = keys[static_cast<std::size_t>(selectedKey)];
            if (key == "OK") return true;
            if (key == "DEL")
            {
                if (!value.empty()) value.pop_back();
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

bool editConfiguration(SDL_Renderer* renderer, int width, int height, widemelon::Config& config,
    widemelon::EvdevInput& input)
{
    int selected = 0;
    std::string status = "EDIT A FIELD THEN CONNECT";
    while (true)
    {
        renderSetup(renderer, width, height, config, selected, status);
        input.pollEvent();
        if (input.exitComboPressed()) return false;
        const widemelon::UiAction action = input.takeUiAction();
        if (action == widemelon::UiAction::Up) selected = (selected + 3) % 4;
        else if (action == widemelon::UiAction::Down) selected = (selected + 1) % 4;
        else if (action == widemelon::UiAction::Confirm || action == widemelon::UiAction::Start)
        {
            if (action == widemelon::UiAction::Start) selected = 3;
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
                    else status = "INVALID PORT";
                }
            }
            else if (selected == 2)
                editNumericField(renderer, width, height, input, "SESSION CODE", config.pairingCode, 10, false);
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
                    status = "SAVE ERROR";
                    widemelon::Logger::error("Configuration save error: " + error);
                }
            }
        }
        SDL_Delay(10);
    }
}

}

namespace widemelon
{

bool ConfigScreen::show(Config config, bool inputTest, std::string& error)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
    {
        error = SDL_GetError();
        return false;
    }

    SDL_Window* window = SDL_CreateWindow("WideMelon Linux Client", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window)
    {
        error = SDL_GetError();
        SDL_Quit();
        return false;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer)
    {
        error = SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    EvdevInput exitInput;
    std::string inputError;
    if (!exitInput.open("/dev/input/event4", inputError))
    {
        error = "Cannot open controller input: " + inputError;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    if (!inputTest && !editConfiguration(renderer, width, height, config, exitInput))
    {
        Logger::info("Configuration form cancelled");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return true;
    }

    std::string status = inputTest ? "INPUT TEST ACTIVE" : "SEARCHING FOR WIDEMELON";
    widemelon::WebSocketClient client;
    auto connection = std::async(std::launch::async, [&client, &config, inputTest]
    {
        if (inputTest) return std::string{};
        return client.connectAndAuthenticate(config, std::chrono::seconds(30));
    });
    render(renderer, width, height, config, status);

    bool running = true;
    ConnectionState displayedConnectionState = client.connectionState();
    bool inputDirty = true;
    std::uint32_t inputSequence = 0;
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
                if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    SDL_GetWindowSize(window, &width, &height);
                    render(renderer, width, height, config, status);
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
            render(renderer, width, height, config, status);
        }
        const ConnectionState currentConnectionState = client.connectionState();
        if (!inputTest && currentConnectionState != displayedConnectionState)
        {
            if (currentConnectionState == ConnectionState::Connected)
                status = "CONNECTION OK";
            else if (currentConnectionState == ConnectionState::Connecting)
                status = "RECONNECTING";
            else if (currentConnectionState == ConnectionState::Failed)
                status = "CONNECTION ERROR";
            render(renderer, width, height, config, status);
            displayedConnectionState = currentConnectionState;
        }
        if (!inputTest && connection.valid() && connection.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            const std::string result = connection.get();
            if (result != "Cancelled")
            {
                status = "CONNECTION ERROR";
                render(renderer, width, height, config, status);
            }
        }
        if (!inputTest && client.connectionState() == ConnectionState::Connected)
        {
            const auto now = std::chrono::steady_clock::now();
            const std::uint64_t generation = client.connectionGeneration();
            if (generation != inputConnectionGeneration)
            {
                inputConnectionGeneration = generation;
                inputSequence = 0;
                inputDirty = true;
                sendReleasedSnapshot = true;
                nextInputSnapshot = now;
            }
            if (sendReleasedSnapshot)
            {
                client.sendInputSnapshot(++inputSequence, 0);
                sendReleasedSnapshot = false;
            }
            else if (inputDirty || now >= nextInputSnapshot)
            {
                client.sendInputSnapshot(++inputSequence, exitInput.buttonMask());
                inputDirty = false;
                nextInputSnapshot = now + std::chrono::milliseconds(200);
            }
        }
        if (exitInput.exitComboPressed())
        {
            Logger::info("Exit requested by Start + L + R; stopping network connection");
            client.requestStop();
            running = false;
        }
        SDL_Delay(10);
    }

    if (!inputTest && client.connectionState() == ConnectionState::Connected)
        client.sendInputSnapshot(++inputSequence, 0);
    client.requestStop();
    if (connection.valid())
    {
        try
        {
            connection.get();
        }
        catch (const std::exception& exception)
        {
            Logger::error(std::string("Network worker stopped with exception: ") + exception.what());
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return true;
}

}
