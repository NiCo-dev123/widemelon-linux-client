#pragma once

#include "config/Config.h"

struct SDL_Renderer;

namespace widemelon
{
    class EvdevInput;
}

namespace widemelon::display
{

    class MenuScreen
    {
    public:
        static bool editConfiguration(SDL_Renderer *renderer, int width, int height, Config &config, EvdevInput &input);
        static void renderConnecting(SDL_Renderer *renderer, int width, int height, const Config &config);
    };

}
