#pragma once

struct SDL_Renderer;
struct SDL_Texture;

#include <string>
#include <chrono>

namespace widemelon
{
    struct DecodedVideoFrame;
    struct Sm64TouchState;
}

namespace widemelon::display
{

    inline constexpr std::chrono::milliseconds StylusSpriteInactivityHideDelay{1000};

    class GameplayScreen
    {
    public:
        static bool updateVideoTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const DecodedVideoFrame &frame);
        // Returns true when the cursor visibility changed and the gameplay
        // screen needs to be rendered again.
        static bool updateCursorVisibility(const Sm64TouchState *cursor);
        static void render(SDL_Renderer *renderer, int width, int height, const std::string &status,
                           SDL_Texture *videoTexture, const Sm64TouchState *cursor = nullptr);
    };

}
