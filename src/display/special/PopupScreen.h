#pragma once

#include <string_view>

struct SDL_Renderer;

namespace widemelon
{
    class EvdevInput;
}

namespace widemelon::display
{
    class PopupScreen
    {
    public:
        enum class Result { Save, Discard, Delete, Cancel, Acknowledge, Exit };

        static Result saveChanges(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                  std::string_view message = "Save changes?");
        static Result confirmDelete(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                    std::string_view message = "Delete this preset?");
        static Result acknowledge(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                  std::string_view message);
        static Result cannotDeleteLastPreset(SDL_Renderer *renderer, int width, int height, EvdevInput &input);
    };
}
