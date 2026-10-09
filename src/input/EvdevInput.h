#pragma once

#include <cstdint>
#include <string>

namespace widemelon
{

struct LeftStickState
{
    int x = 0;
    int y = 0;
    int xMinimum = 0;
    int xMaximum = 0;
    int yMinimum = 0;
    int yMaximum = 0;
    int xCenter = 0;
    int yCenter = 0;
};

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
    bool takeLeftStickChanged();
    LeftStickState leftStickState() const;
    LeftStickState rightStickState() const;
    bool r2Pressed() const { return rightTriggerPressed; }
    void setLeftStickDpadEnabled(bool enabled);
    void setLeftStickDpadThresholdFraction(float fraction);

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
    int leftStickX = 0;
    int leftStickY = 0;
    int leftStickXMinimum = 0;
    int leftStickXMaximum = 0;
    int leftStickYMinimum = 0;
    int leftStickYMaximum = 0;
    int leftStickXCenter = 0;
    int leftStickYCenter = 0;
    int leftStickXThreshold = 8192;
    int leftStickYThreshold = 8192;
    int rightStickX = 0;
    int rightStickY = 0;
    int rightStickXMinimum = 0;
    int rightStickXMaximum = 0;
    int rightStickYMinimum = 0;
    int rightStickYMaximum = 0;
    int rightStickXCenter = 0;
    int rightStickYCenter = 0;
    bool stateChanged = false;
    bool leftStickChanged = false;
    bool leftStickDpadEnabled = true;
    bool rightTriggerPressed = false;
    UiAction uiAction = UiAction::None;
};

}
