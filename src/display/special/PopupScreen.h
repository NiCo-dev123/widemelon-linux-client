#pragma once

#include "input/InputPreset.h"

#include <string_view>

struct SDL_Renderer;

namespace widemelon
{
    class EvdevInput;
    struct ConnectionProgress;
}

namespace widemelon::display
{
    class PopupScreen
    {
    public:
        enum class Result { Save, Discard, Delete, Cancel, Acknowledge, MappingDisabled, MappingKeyPress, MappingPress, MappingHold, Exit };
        struct MappingResult
        {
            bool accepted{false};
            bool exit{false};
            InputMapping mapping{};
        };

        static Result saveChanges(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                  std::string_view message = "Save changes?");
        static Result confirmDelete(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                    std::string_view message = "Delete this preset?");
        static Result acknowledge(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                  std::string_view message);
        static Result cannotDeleteLastPreset(SDL_Renderer *renderer, int width, int height, EvdevInput &input);
        static void renderConnection(SDL_Renderer *renderer, int width, int height, const ConnectionProgress &progress);
        static MappingResult editInputMapping(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                              std::string_view inputName, InputMapping current);
    };
}
