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
        // Returns true only when the user asks to close the application from
        // an in-game menu. Returning normally resumes the active session.
        static bool editInGameSettings(SDL_Renderer *renderer, int width, int height, const Config &config, EvdevInput &input);
        static void renderConnecting(SDL_Renderer *renderer, int width, int height, const Config &config);
    };

}
