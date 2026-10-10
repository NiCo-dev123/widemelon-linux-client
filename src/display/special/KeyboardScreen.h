#pragma once

#include <cstddef>
#include <string>

struct SDL_Renderer;

namespace widemelon
{
    class EvdevInput;
}

namespace widemelon::display
{
    class KeyboardScreen
    {
    public:
        enum class Result { Cancelled, Accepted, Exit };

        static Result edit(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                           const std::string &title, std::string &value, std::size_t maximumLength);
    };
}
