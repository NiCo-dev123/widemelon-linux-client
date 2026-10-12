#include "input/InputMapper.h"

#include "input/EvdevInput.h"

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
    singlePressPending.fill(false);
    virtualButtonMask = 0;
    stylusPressed = false;
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
    virtualButtonMask = 0;
    stylusPressed = false;
    const std::array<bool, PhysicalInputCount> singlePressActive = singlePressPending;
    singlePressPending.fill(false);
    for (std::size_t index = 0; index < mappings.size(); ++index)
    {
        const bool pressed = isPressed(input, static_cast<PhysicalInput>(index));
        const InputMapping &mapping = mappings[index];
        if (mapping.output == MappingOutput::KeyPress && pressed)
            virtualButtonMask |= dsButtonMask(mapping.button);
        if (mapping.output == MappingOutput::StylusPress && pressed && !previousPressed[index])
            singlePressPending[index] = true;
        if (mapping.output == MappingOutput::StylusHold && pressed && !previousPressed[index])
            togglePressed[index] = !togglePressed[index];
        if (mapping.output != MappingOutput::StylusHold)
            togglePressed[index] = false;
        if (mapping.output != MappingOutput::StylusPress)
            singlePressPending[index] = false;
        if (mapping.output == MappingOutput::StylusHold && togglePressed[index])
            stylusPressed = true;
        if (mapping.output == MappingOutput::StylusPress && (singlePressActive[index] || singlePressPending[index])) stylusPressed = true;
        previousPressed[index] = pressed;
    }
    return previousButtons != virtualButtonMask || previousStylus != stylusPressed;
}

bool InputMapper::hasStylusMapping() const
{
    for (const InputMapping &mapping : mappings)
        if (mapping.output == MappingOutput::StylusPress || mapping.output == MappingOutput::StylusHold) return true;
    return false;
}
}
