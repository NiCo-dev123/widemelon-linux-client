#include "input/configs/Default.h"

namespace widemelon::input_configs
{
InputPreset defaultInputPreset()
{
    InputPreset preset;
    preset.name = "Default";
    const auto identity = [&](PhysicalInput input, DsButton button)
    {
        preset.mappings[static_cast<std::size_t>(input)] = {MappingOutput::KeyPress, button};
    };
    identity(PhysicalInput::A, DsButton::A);
    identity(PhysicalInput::B, DsButton::B);
    identity(PhysicalInput::X, DsButton::X);
    identity(PhysicalInput::Y, DsButton::Y);
    identity(PhysicalInput::L, DsButton::L);
    identity(PhysicalInput::R, DsButton::R);
    identity(PhysicalInput::DpadLeft, DsButton::DpadLeft);
    identity(PhysicalInput::DpadRight, DsButton::DpadRight);
    identity(PhysicalInput::DpadUp, DsButton::DpadUp);
    identity(PhysicalInput::DpadDown, DsButton::DpadDown);
    identity(PhysicalInput::Select, DsButton::Select);
    identity(PhysicalInput::Start, DsButton::Start);
    return preset;
}
}
