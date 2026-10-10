#pragma once

struct SDL_Renderer;
struct SDL_Texture;

#include <string>

namespace widemelon
{
    struct DecodedVideoFrame;
    struct Sm64TouchState;
}

namespace widemelon::display
{

    class GameplayScreen
    {
    public:
        static bool updateVideoTexture(SDL_Renderer *renderer, SDL_Texture *&texture, const DecodedVideoFrame &frame);
        static void render(SDL_Renderer *renderer, int width, int height, const std::string &status,
                           SDL_Texture *videoTexture, const Sm64TouchState *cursor = nullptr);
    };

}
