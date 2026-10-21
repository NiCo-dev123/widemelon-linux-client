#include "input/InputMapper.h"

#include "input/EvdevInput.h"

#include <algorithm>
#include <cmath>

namespace widemelon
{
namespace
{
constexpr std::uint16_t buttonBit(DsButton button)
{
    switch (button)
    {
    case DsButton::A: return 1U << 0;
    case DsButton::B: return 1U << 1;
    case DsButton::Select: return 1U << 2;
    case DsButton::Start: return 1U << 3;
    case DsButton::DpadRight: return 1U << 4;
    case DsButton::DpadLeft: return 1U << 5;
    case DsButton::DpadUp: return 1U << 6;
    case DsButton::DpadDown: return 1U << 7;
    case DsButton::R: return 1U << 8;
    case DsButton::L: return 1U << 9;
    case DsButton::X: return 1U << 10;
    case DsButton::Y: return 1U << 11;
    }
    return 0;
}
}

void InputMapper::setMappings(const std::array<InputMapping, PhysicalInputCount> &value)
{
    mappings = value;
    previousPressed.fill(false);
    togglePressed.fill(false);
    leftStickWasOutsideDeadzone = false;
    rightStickWasOutsideDeadzone = false;
    leftStickLeavePulseFramesRemaining = 0;
    rightStickLeavePulseFramesRemaining = 0;
    leftStickEnterPulseFramesRemaining = 0;
    rightStickEnterPulseFramesRemaining = 0;
    virtualButtonMask = 0;
    toggledButtonMask = 0;
    stylusPressed = false;
    screenPressHeld = false;
    screenPressXValue = 127;
    screenPressYValue = 95;
    screenPressReleaseDelayFramesValue = ScreenPressReleaseDelayFramesDefault;
}

void InputMapper::setStickDeadzoneInputThresholds(std::uint8_t leftPercent, std::uint8_t rightPercent)
{
    leftStickDeadzonePercent = std::clamp(leftPercent, StickDeadzoneInputMinimumPercent, StickDeadzoneInputMaximumPercent);
    rightStickDeadzonePercent = std::clamp(rightPercent, StickDeadzoneInputMinimumPercent, StickDeadzoneInputMaximumPercent);
}

bool InputMapper::outsideDeadzone(const LeftStickState &stick, std::uint8_t percent)
{
    const auto normalize = [](int value, int center, int minimum, int maximum)
    {
        const int distance = value < center ? center - minimum : maximum - center;
        if (distance <= 0) return 0.0F;
        return std::clamp(static_cast<float>(value - center) / static_cast<float>(distance), -1.0F, 1.0F);
    };
    const float horizontal = normalize(stick.x, stick.xCenter, stick.xMinimum, stick.xMaximum);
    const float vertical = normalize(stick.y, stick.yCenter, stick.yMinimum, stick.yMaximum);
    return std::sqrt(horizontal * horizontal + vertical * vertical) > static_cast<float>(percent) / 100.0F;
}

bool InputMapper::isPressed(const EvdevInput &input, PhysicalInput source)
{
    const std::uint16_t mask = input.buttonMask();
    switch (source)
    {
    case PhysicalInput::A: return mask & (1U << 0);
    case PhysicalInput::B: return mask & (1U << 1);
    case PhysicalInput::Select: return mask & (1U << 2);
    case PhysicalInput::Start: return mask & (1U << 3);
    case PhysicalInput::DpadRight: return mask & (1U << 4);
    case PhysicalInput::DpadLeft: return mask & (1U << 5);
    case PhysicalInput::DpadUp: return mask & (1U << 6);
    case PhysicalInput::DpadDown: return mask & (1U << 7);
    case PhysicalInput::R: return mask & (1U << 8);
    case PhysicalInput::L: return mask & (1U << 9);
    case PhysicalInput::X: return mask & (1U << 10);
    case PhysicalInput::Y: return mask & (1U << 11);
    case PhysicalInput::L2: return input.l2Pressed();
    case PhysicalInput::R2: return input.r2Pressed();
    case PhysicalInput::L3: return input.l3Pressed();
    case PhysicalInput::R3: return input.r3Pressed();
    default: return false;
    }
}

std::uint16_t InputMapper::dsButtonMask(DsButton button)
{
    return buttonBit(button);
}

bool InputMapper::update(const EvdevInput &input)
{
    const std::uint16_t previousButtons = virtualButtonMask;
    const bool previousStylus = stylusPressed;
    const bool previousScreenPressHeld = screenPressHeld;
    const std::uint16_t previousScreenPressX = screenPressXValue;
    const std::uint16_t previousScreenPressY = screenPressYValue;
    const std::uint8_t previousScreenPressDelay = screenPressReleaseDelayFramesValue;
    virtualButtonMask = 0;
    stylusPressed = false;
    screenPressHeld = false;
    const bool leftOutside = outsideDeadzone(input.leftStickState(), leftStickDeadzonePercent);
    const bool rightOutside = outsideDeadzone(input.rightStickState(), rightStickDeadzonePercent);
    if (leftOutside && !leftStickWasOutsideDeadzone) leftStickLeavePulseFramesRemaining = StickDeadzoneInputPulseFrames;
    if (rightOutside && !rightStickWasOutsideDeadzone) rightStickLeavePulseFramesRemaining = StickDeadzoneInputPulseFrames;
    if (!leftOutside && leftStickWasOutsideDeadzone) leftStickEnterPulseFramesRemaining = StickDeadzoneInputPulseFrames;
    if (!rightOutside && rightStickWasOutsideDeadzone) rightStickEnterPulseFramesRemaining = StickDeadzoneInputPulseFrames;
    leftStickWasOutsideDeadzone = leftOutside;
    rightStickWasOutsideDeadzone = rightOutside;
    for (std::size_t index = 0; index < mappings.size(); ++index)
    {
        const PhysicalInput source = static_cast<PhysicalInput>(index);
        const bool pressed = source == PhysicalInput::LeftStickLeavesDeadzone ? leftStickLeavePulseFramesRemaining > 0
            : source == PhysicalInput::RightStickLeavesDeadzone ? rightStickLeavePulseFramesRemaining > 0
            : source == PhysicalInput::LeftStickEntersDeadzone ? leftStickEnterPulseFramesRemaining > 0
            : source == PhysicalInput::RightStickEntersDeadzone ? rightStickEnterPulseFramesRemaining > 0 : isPressed(input, source);
        const InputMapping &mapping = mappings[index];
        if (mapping.output == MappingOutput::KeyPress && pressed)
            virtualButtonMask |= dsButtonMask(mapping.button);
        if (mapping.output == MappingOutput::KeyHold && pressed && !previousPressed[index])
            toggledButtonMask ^= dsButtonMask(mapping.button);
        if (mapping.output == MappingOutput::ScreenPress && pressed)
        {
            screenPressHeld = true;
            screenPressXValue = mapping.screenX;
            screenPressYValue = mapping.screenY;
            screenPressReleaseDelayFramesValue = mapping.screenPressReleaseDelayFrames;
        }
        if (mapping.output == MappingOutput::StylusHold && pressed && !previousPressed[index])
            togglePressed[index] = !togglePressed[index];
        if (mapping.output != MappingOutput::StylusHold)
            togglePressed[index] = false;
        if (mapping.output == MappingOutput::StylusHold && togglePressed[index])
            stylusPressed = true;
        if (mapping.output == MappingOutput::StylusPress && pressed) stylusPressed = true;
        previousPressed[index] = pressed;
    }
    if (leftStickLeavePulseFramesRemaining > 0) --leftStickLeavePulseFramesRemaining;
    if (rightStickLeavePulseFramesRemaining > 0) --rightStickLeavePulseFramesRemaining;
    if (leftStickEnterPulseFramesRemaining > 0) --leftStickEnterPulseFramesRemaining;
    if (rightStickEnterPulseFramesRemaining > 0) --rightStickEnterPulseFramesRemaining;
    virtualButtonMask |= toggledButtonMask;
    return previousButtons != virtualButtonMask || previousStylus != stylusPressed
        || previousScreenPressHeld != screenPressHeld || previousScreenPressX != screenPressXValue
        || previousScreenPressY != screenPressYValue || previousScreenPressDelay != screenPressReleaseDelayFramesValue;
}

bool InputMapper::hasStylusMapping() const
{
    for (const InputMapping &mapping : mappings)
        if (mapping.output == MappingOutput::StylusPress || mapping.output == MappingOutput::StylusHold || mapping.output == MappingOutput::ScreenPress) return true;
    return false;
}
}
