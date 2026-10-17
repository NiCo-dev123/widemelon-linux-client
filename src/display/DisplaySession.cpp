#include "display/ConfigScreen.h"
#include "display/DisplaySession.h"
#include "display/special/GameplayScreen.h"
#include "display/MenuNavigation.h"
#include "display/MenuScreen.h"
#include "display/special/NumpadScreen.h"
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
#include "input/TspsStickConfig.h"
#include "input/EvdevInput.h"
#include "input/InputPreset.h"
#include "input/Sm64ManualStickMod.h"
#include "input/Sm64StickMod.h"
#include "input/StickDpadMod.h"
#include "input/StylusIntent.h"
#include "input/StylusRouter.h"
#include "input/InputMapper.h"
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
    constexpr std::string_view UiConfigRightStickModeKey{"right-stick-mod"};
    constexpr std::string_view UiConfigMphManualSpeedKey{"mph-manual-speed"};
    constexpr std::string_view UiConfigSm64AutoFramesKey{"sm64-auto-center-hold-frames"};
    constexpr std::string_view UiConfigSm64AutoReleaseDelayKey{"sm64-auto-release-delay-ms"};
    constexpr std::string_view UiConfigCursorSpeedKey{"cursor-speed-limit"};
    constexpr std::string_view UiConfigSm64DpadDeadzoneKey{"sm64-dpad-deadzone-percent"};
    constexpr std::string_view UiConfigLeftStickScaleLeftKey{"left-stick-scale-left-percent"};
    constexpr std::string_view UiConfigLeftStickScaleRightKey{"left-stick-scale-right-percent"};
    constexpr std::string_view UiConfigLeftStickScaleUpKey{"left-stick-scale-up-percent"};
    constexpr std::string_view UiConfigLeftStickScaleDownKey{"left-stick-scale-down-percent"};

    // Values kept solely to migrate installations created before these dedicated
    // modes were removed from the preset editor.
    constexpr int LegacyMphManualSpeedDefault{100};
    constexpr int LegacyMphManualSpeedMinimum{50};
    constexpr int LegacyMphManualSpeedMaximum{500};
    constexpr int LegacySm64DpadDeadzoneDefaultPercent{75};
    constexpr int LegacySm64DpadDeadzoneMinimumPercent{25};
    constexpr int LegacySm64DpadDeadzoneMaximumPercent{95};

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
        widemelon::Sm64ManualStickMod sm64ManualStick;
        widemelon::CursorStickMod leftCursorStick;
        widemelon::CursorStickMod rightCursorStick;
        widemelon::StickDpadMod leftStickDpad;
        widemelon::StickDpadMod rightStickDpad;
        bool analogCenteredEnabled = false;
        bool cursorEnabled = false;
        bool rightCursorEnabled = false;
        bool leftStickDpadEnabled = false;
        bool rightStickDpadEnabled = false;
        widemelon::LeftStickCalibration leftStickCalibration;
        widemelon::StylusRouter stylusRouter;
        widemelon::InputMapper inputMapper;
        bool stylusButtonMappingEnabled = false;
        auto sm64ModeEnabled = [&] { return analogCenteredEnabled || cursorEnabled || rightCursorEnabled || stylusButtonMappingEnabled; };
        Sm64TouchState displayedStylusState;
        auto sm64TouchState = [&]() -> const Sm64TouchState&
        {
            const StylusState &state = stylusRouter.state();
            displayedStylusState = {state.pressed, state.x, state.y};
            return displayedStylusState;
        };
        auto reloadStickMods = [&]
        {
            std::array<char, 4096> executable{};
            const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
            if (length <= 0)
            {
                Logger::error("Cannot reload stick configuration: cannot resolve executable path");
                return;
            }

            executable[static_cast<std::size_t>(length)] = 0;
            const std::string directory = std::filesystem::path(executable.data()).parent_path().string();
            const widemelon::display::UiConfig uiConfig = uiConfigFor(directory);
            const std::optional<std::string> activePresetName = widemelon::InputPresetStore::activeName();
            std::optional<widemelon::InputPreset> activePreset = activePresetName
                ? widemelon::InputPresetStore::load(*activePresetName) : std::nullopt;
            if (!activePreset && !activePresetName)
            {
                widemelon::InputPreset migrated = widemelon::InputPresetStore::defaultPreset();
                migrated.name = "Config 1";
                migrated.leftStickMode = uiConfig.readValue(UiConfigLeftStickModeKey).value_or("D-pad");
                migrated.rightStickMode = uiConfig.readValue(UiConfigRightStickModeKey).value_or("Disabled");
                migrated.mphCameraSpeed = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigMphManualSpeedKey, LegacyMphManualSpeedDefault, LegacyMphManualSpeedMinimum, LegacyMphManualSpeedMaximum));
                migrated.sm64AutoCenterHoldFrames = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64AutoFramesKey, widemelon::Sm64TouchCenterHoldFramesDefault, widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum));
                migrated.sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigSm64AutoReleaseDelayKey, widemelon::Sm64TouchReleaseDelayDefaultMs, widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs));
                migrated.sm64DpadDeadzonePercent = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64DpadDeadzoneKey, LegacySm64DpadDeadzoneDefaultPercent, LegacySm64DpadDeadzoneMinimumPercent, LegacySm64DpadDeadzoneMaximumPercent));
                migrated.leftCursorSpeedLimit = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigCursorSpeedKey, widemelon::CursorSpeedDefault, widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum));
                migrated.rightCursorSpeedLimit = migrated.leftCursorSpeedLimit;
                std::string migrationError;
                if (widemelon::InputPresetStore::save(migrated, true, migrationError))
                {
                    Logger::info("Migrated legacy stick settings to input preset Config 1");
                    activePreset = migrated;
                }
                else
                    Logger::error("Input preset migration warning: " + migrationError);
            }
            else if (!activePreset)
            {
                activePreset = widemelon::InputPresetStore::defaultPreset();
                std::string fallbackError;
                if (widemelon::InputPresetStore::save(*activePreset, true, fallbackError))
                    Logger::info("Active input preset is unavailable; restored Default preset");
                else
                    Logger::error("Cannot restore Default input preset; continuing with built-in defaults: " + fallbackError);
            }
            if (!activePreset)
            {
                Logger::error("Cannot reload input preset; retaining the current stick configuration");
                return;
            }

            const std::string &leftStickMode = activePreset->leftStickMode;
            const std::string &rightStickMode = activePreset->rightStickMode;
            const widemelon::LeftStickCalibration legacyCalibration{
                static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleLeftKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
                static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleRightKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
                static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleUpKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
                static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleDownKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent))};
            std::optional<widemelon::LeftStickCalibration> previousCalibration =
                widemelon::InputPresetStore::legacyLeftStickCalibration(activePreset->name);
            if (!previousCalibration) previousCalibration = widemelon::InputPresetStore::legacyActiveStickCalibration();
            leftStickCalibration = widemelon::TspsStickConfig::loadOrMigrate(previousCalibration.value_or(legacyCalibration));
            analogCenteredEnabled = leftStickMode == "Analog centered" || leftStickMode == "SM64 Manual";
            cursorEnabled = leftStickMode == "Cursor" || leftStickMode == "Cursor warped";
            rightCursorEnabled = rightStickMode == "Cursor" || rightStickMode == "Cursor warped";
            leftStickDpadEnabled = leftStickMode == "D-pad";
            rightStickDpadEnabled = rightStickMode == "D-pad";
            sm64ManualStick.setEnabled(analogCenteredEnabled);
            leftCursorStick.setSource(StylusSource::LeftStick);
            leftCursorStick.setSpeedLimit(activePreset->leftCursorSpeedLimit);
            leftCursorStick.setDeadzonePercent(activePreset->leftCursorDeadzonePercent);
            leftCursorStick.setWarpEnabled(leftStickMode == "Cursor warped");
            leftCursorStick.setEnabled(cursorEnabled);
            rightCursorStick.setSource(StylusSource::RightStick);
            rightCursorStick.setSpeedLimit(activePreset->rightCursorSpeedLimit);
            rightCursorStick.setDeadzonePercent(activePreset->rightCursorDeadzonePercent);
            rightCursorStick.setWarpEnabled(rightStickMode == "Cursor warped");
            rightCursorStick.setEnabled(rightCursorEnabled);
            leftStickDpad.setEnabled(leftStickDpadEnabled);
            rightStickDpad.setEnabled(rightStickDpadEnabled);
            inputMapper.setMappings(activePreset->mappings);
            stylusButtonMappingEnabled = inputMapper.hasStylusMapping();
            exitInput.setLeftStickDpadEnabled(false);
            stylusRouter.reset();
            Logger::info("Stick configuration reloaded: left=" + leftStickMode + ", right=" + rightStickMode);
        };
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

        if (!inputTest) reloadStickMods();

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
        bool warpReleasePending = false;
        bool warpReleaseSent = false;
        std::uint16_t warpReleaseX = 0, warpReleaseY = 0;
        std::chrono::steady_clock::time_point warpReleaseUntil{};
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
            if (exitInput.exitComboPressed())
            {
                warpReleasePending = false;
                warpReleaseSent = false;
                Logger::info("Opening in-game settings with Start + L + R");
                if (widemelon::display::MenuScreen::editInGameSettings(renderer, width, height, config, exitInput))
                {
                    Logger::info("Exit requested from in-game settings; stopping network connection");
                    client->requestStop();
                    running = false;
                }
                else
                {
                    reloadStickMods();
                    widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                }
                continue;
            }
            const widemelon::LeftStickState calibratedStick = widemelon::applyLeftStickCalibration(exitInput.leftStickState(), leftStickCalibration);
            const bool sm64ManualTouchChanged = sm64ManualStick.update(calibratedStick);
            const widemelon::LeftStickState rightStick = exitInput.rightStickState();
            if (leftCursorStick.isStickActive(calibratedStick) && stylusRouter.owner() != StylusSource::LeftStick)
                leftCursorStick.setCursorPosition(stylusRouter.state().x, stylusRouter.state().y);
            if (rightCursorStick.isStickActive(rightStick) && stylusRouter.owner() != StylusSource::RightStick)
                rightCursorStick.setCursorPosition(stylusRouter.state().x, stylusRouter.state().y);
            const bool leftCursorTouchChanged = leftCursorStick.update(calibratedStick);
            const bool rightCursorTouchChanged = rightCursorStick.update(rightStick);
            const bool leftStickDpadChanged = leftStickDpad.update(calibratedStick);
            const bool rightStickDpadChanged = rightStickDpad.update(rightStick);
            const bool mappedInputChanged = inputMapper.update(exitInput);
            bool stylusChanged = stylusRouter.advanceScreenPress();
            if (sm64ManualTouchChanged) stylusChanged = stylusRouter.submit(sm64ManualStick.intent()) || stylusChanged;
            const auto submitCursor = [&](const CursorStickMod &cursor)
            {
                const StylusState before = stylusRouter.state();
                const bool changed = stylusRouter.submit(cursor.intent());
                const StylusState &after = stylusRouter.state();
                if (cursor.wrapped() && before.pressed && after.pressed && after.x == cursor.intent().x
                    && after.x != before.x && !warpReleasePending)
                {
                    warpReleasePending = true;
                    warpReleaseSent = false;
                    warpReleaseX = before.x;
                    warpReleaseY = before.y;
                }
                return changed;
            };
            if (leftCursorTouchChanged) stylusChanged = submitCursor(leftCursorStick) || stylusChanged;
            if (rightCursorTouchChanged) stylusChanged = submitCursor(rightCursorStick) || stylusChanged;
            if (mappedInputChanged)
            {
                stylusChanged = stylusRouter.setMappedPressed(inputMapper.touchPressed()) || stylusChanged;
                stylusChanged = stylusRouter.setScreenPress(inputMapper.screenPressActive(), inputMapper.screenPressX(),
                    inputMapper.screenPressY(), inputMapper.screenPressReleaseDelayFrames()) || stylusChanged;
            }
            inputDirty = exitInput.takeStateChanged() || stylusChanged || leftStickDpadChanged || rightStickDpadChanged || inputDirty;
            const Sm64TouchState *stylusCursor = sm64ModeEnabled() ? &sm64TouchState() : nullptr;
            if (widemelon::display::GameplayScreen::updateCursorVisibility(stylusCursor))
                widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, stylusCursor);
            if (stylusChanged && !inputTest)
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
                    warpReleasePending = false;
                    warpReleaseSent = false;
                    nextInputSnapshot = now;
                    widemelon::display::GameplayScreen::render(renderer, width, height, status, videoTexture, sm64ModeEnabled() ? &sm64TouchState() : nullptr);
                }
                if (warpReleasePending && warpReleaseSent && now >= warpReleaseUntil)
                {
                    warpReleasePending = false;
                    warpReleaseSent = false;
                    inputDirty = true;
                }
                if (sendReleasedSnapshot)
                {
                    const std::uint32_t sequence = ++inputSequence;
                    client->sendInputSnapshot(sequence, 0, false, sm64TouchState().x, sm64TouchState().y);
                    sendReleasedSnapshot = false;
                }
                else if (warpReleasePending && !warpReleaseSent)
                {
                    const std::uint32_t sequence = ++inputSequence;
                    const std::uint16_t buttons = static_cast<std::uint16_t>(inputMapper.buttonMask() | leftStickDpad.additionalButtons() | rightStickDpad.additionalButtons());
                    if (client->sendInputSnapshot(sequence, buttons, false, warpReleaseX, warpReleaseY))
                    {
                        warpReleaseSent = true;
                        warpReleaseUntil = std::chrono::steady_clock::now() + CursorWarpReleaseDuration;
                        inputDirty = true;
                    }
                }
                else if (inputDirty || now >= nextInputSnapshot)
                {
                    const std::uint32_t sequence = ++inputSequence;
                    const bool touchActive = sm64TouchState().active && !warpReleasePending;
                    client->sendInputSnapshot(sequence, static_cast<std::uint16_t>(inputMapper.buttonMask() | leftStickDpad.additionalButtons() | rightStickDpad.additionalButtons()),
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
