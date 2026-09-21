#include "display/ConfigScreen.h"

#include "common/Logger.h"
#include "input/EvdevInput.h"
#include "network/WebSocketClient.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
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

}

namespace widemelon
{

bool ConfigScreen::show(const Config& config, bool inputTest, std::string& error)
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
    std::string status = inputTest ? "INPUT TEST ACTIVE" : "SEARCHING FOR WIDEMELON";
    widemelon::WebSocketClient client;
    auto connection = std::async(std::launch::async, [&client, &config, inputTest]
    {
        if (inputTest) return std::string{};
        return client.connectAndAuthenticate(config, std::chrono::seconds(30));
    });
    render(renderer, width, height, config, status);

    EvdevInput exitInput;
    std::string inputError;
    exitInput.open("/dev/input/event4", inputError);

    bool running = true;
    bool connectionReported = false;
    bool inputDirty = true;
    std::uint32_t inputSequence = 0;
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
        if (!inputTest && !connectionReported && client.connectionState() == ConnectionState::Connected)
        {
            status = "CONNECTION OK";
            render(renderer, width, height, config, status);
            connectionReported = true;
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
            if (inputDirty || now >= nextInputSnapshot)
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
