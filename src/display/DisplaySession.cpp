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
    using widemelon::display::loadUiResources;

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
