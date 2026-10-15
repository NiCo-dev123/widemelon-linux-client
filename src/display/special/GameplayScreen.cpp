#include "display/special/GameplayScreen.h"

#include "common/Logger.h"
#include "display/UiTheme.h"
#include "input/Sm64StickMod.h"
#include "network/WebSocketClient.h"

#include <chrono>

namespace widemelon::display
{
    namespace
    {
        struct CursorVisibilityState
        {
            bool initialized{false};
            bool wasPressed{false};
            bool visible{false};
            std::uint16_t x{0};
            std::uint16_t y{0};
            std::chrono::steady_clock::time_point lastActivity{};
        };

        CursorVisibilityState &cursorVisibilityState()
        {
            static CursorVisibilityState state;
            return state;
        }
    }


    bool GameplayScreen::updateVideoTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const DecodedVideoFrame &frame)
    {
        if (frame.width == 0 || frame.height == 0 || frame.rgb.size() != static_cast<std::size_t>(frame.width) * frame.height * 3)
            return false;
        if (!texture)
        {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);
            if (!texture)
            {
                Logger::error(std::string("Cannot create video texture: ") + SDL_GetError());
                return false;
            }
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        }
        if (SDL_UpdateTexture(texture, nullptr, frame.rgb.data(), frame.width * 3) != 0)
        {
            Logger::error(std::string("Cannot update video texture: ") + SDL_GetError());
            return false;
        }
        return true;
    }

    bool GameplayScreen::updateCursorVisibility(const Sm64TouchState *cursor)
    {
        CursorVisibilityState &state = cursorVisibilityState();
        if (!cursor)
        {
            const bool changed = state.visible;
            state = {};
            return changed;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool moved = !state.initialized || cursor->x != state.x || cursor->y != state.y;
        const bool pressChanged = state.initialized && cursor->active != state.wasPressed;
        if (moved || pressChanged || cursor->active) state.lastActivity = now;

        state.initialized = true;
        state.x = cursor->x;
        state.y = cursor->y;
        state.wasPressed = cursor->active;
        const bool visible = cursor->active || now - state.lastActivity < StylusSpriteInactivityHideDelay;
        const bool changed = visible != state.visible;
        state.visible = visible;
        return changed;
    }

    void GameplayScreen::render(SDL_Renderer *renderer, int width, int height, const std::string &status,
                                SDL_Texture *videoTexture, const Sm64TouchState *cursor)
    {
        const Palette &palette = uiPalette();
        const ThemeTextSettings &text = uiTextSettings();
        const UiTextures &textures = uiTextures();
        drawBackground(renderer, width, height, true);
        SDL_SetRenderDrawColor(renderer, palette.primary.r, palette.primary.g, palette.primary.b, 255);
        constexpr std::string_view title{"WideMelon Client"};
        drawTextColored(renderer, title, (width - textWidth(title, text.titleFontSize)) / 2, 8, text.titleFontSize, palette.hint);
        const SDL_Rect video{(width - 768) / 2, 45, 768, 576};
        if (videoTexture)
            SDL_RenderCopy(renderer, videoTexture, nullptr, &video);
        else
            SDL_RenderFillRect(renderer, &video);
        updateCursorVisibility(cursor);
        if (cursor && cursorVisibilityState().visible)
        {
            const SDL_Rect pointer{video.x + static_cast<int>(cursor->x) * video.w / 256,
                                   video.y + static_cast<int>(cursor->y) * video.h / 192, 48, 48};
            if (textures.cursor)
            {
                SDL_SetTextureAlphaMod(textures.cursor, cursor->active ? 255 : 128);
                SDL_RenderCopy(renderer, textures.cursor, nullptr, &pointer);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, palette.hint.r, palette.hint.g, palette.hint.b, 255);
                const SDL_Rect fallback{pointer.x, pointer.y, 8, 8};
                SDL_RenderFillRect(renderer, &fallback);
            }
        }
        const std::string connection = "Connection status: " + status;
        drawTextColored(renderer, connection, video.x, video.y + video.h + 6, text.gameplayStatusFontSize, palette.hint);
        int exitX = video.x;
        const int exitY = video.y + video.h + 50;
        exitX = drawControlHint(renderer, textures.hintStart, "START", "+", exitX, exitY, text.hintFontSize);
        exitX = drawControlHint(renderer, textures.hintR, "R", "+", exitX, exitY, text.hintFontSize);
        drawControlHint(renderer, textures.hintL, "L", ": Settings", exitX, exitY, text.hintFontSize);
        SDL_RenderPresent(renderer);
    }

}
