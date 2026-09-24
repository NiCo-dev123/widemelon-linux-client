#pragma once

#include <cstdint>
#include <string>

namespace widemelon
{

enum class UiAction
{
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Delete,
    Back,
    Start,
};

class EvdevInput
{
public:
    EvdevInput() = default;
    ~EvdevInput();

    EvdevInput(const EvdevInput&) = delete;
    EvdevInput& operator=(const EvdevInput&) = delete;

    bool open(const std::string& path, std::string& error);
    std::string pollEvent();
    bool exitComboPressed();
    UiAction takeUiAction();
    UiAction heldUiDirection() const;
    std::uint16_t buttonMask() const { return mask; }
    bool takeStateChanged();

private:
    bool updateDirectionMask(std::uint16_t& sourceMask, bool horizontal, int value, int center, int threshold);
    void mergeDirectionalSources();
    void setUiDirection(bool horizontal, int value, int center, int threshold);

    int fileDescriptor = -1;
    bool startPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;
    bool exitCombo = false;
    std::uint16_t mask = 0;
    std::uint16_t dpadMask = 0;
    std::uint16_t leftStickMask = 0;
    int leftStickXCenter = 0;
    int leftStickYCenter = 0;
    int leftStickXThreshold = 8192;
    int leftStickYThreshold = 8192;
    bool stateChanged = false;
    UiAction uiAction = UiAction::None;
};

}
