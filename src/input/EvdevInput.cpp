#include "input/EvdevInput.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

namespace widemelon
{

namespace
{
constexpr std::uint16_t ButtonA = 1U << 0;
constexpr std::uint16_t ButtonB = 1U << 1;
constexpr std::uint16_t ButtonSelect = 1U << 2;
constexpr std::uint16_t ButtonStart = 1U << 3;
constexpr std::uint16_t ButtonRight = 1U << 4;
constexpr std::uint16_t ButtonLeft = 1U << 5;
constexpr std::uint16_t ButtonUp = 1U << 6;
constexpr std::uint16_t ButtonDown = 1U << 7;
constexpr std::uint16_t ButtonR = 1U << 8;
constexpr std::uint16_t ButtonL = 1U << 9;
constexpr std::uint16_t ButtonX = 1U << 10;
constexpr std::uint16_t ButtonY = 1U << 11;
}

EvdevInput::~EvdevInput()
{
    if (fileDescriptor >= 0) close(fileDescriptor);
}

bool EvdevInput::open(const std::string& path, std::string& error)
{
    fileDescriptor = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fileDescriptor < 0)
    {
        error = std::strerror(errno);
        return false;
    }
    return true;
}

bool EvdevInput::exitComboPressed()
{
    const bool pressed = exitCombo;
    exitCombo = false;
    return pressed;
}

bool EvdevInput::takeStateChanged()
{
    const bool changed = stateChanged;
    stateChanged = false;
    return changed;
}

UiAction EvdevInput::takeUiAction()
{
    const UiAction action = uiAction;
    uiAction = UiAction::None;
    return action;
}

std::string EvdevInput::pollEvent()
{
    if (fileDescriptor < 0) return {};

    input_event event{};
    std::string description;
    while (::read(fileDescriptor, &event, sizeof(event)) == sizeof(event))
    {
        description = "EVENT T" + std::to_string(event.type) + " C" + std::to_string(event.code)
            + " V" + std::to_string(event.value);
        if (event.type == EV_KEY)
        {
            const bool pressed = event.value != 0;
            std::uint16_t button = 0;
            switch (event.code)
            {
            case 304: button = ButtonB; break;
            case 305: button = ButtonA; break;
            case 307: button = ButtonY; break;
            case 308: button = ButtonX; break;
            case BTN_SELECT: button = ButtonSelect; break;
            case BTN_START: button = ButtonStart; startPressed = pressed; break;
            case BTN_TL: button = ButtonL; leftPressed = pressed; break;
            case BTN_TR: button = ButtonR; rightPressed = pressed; break;
            default: break;
            }
            if (button != 0)
            {
                const std::uint16_t previous = mask;
                if (pressed) mask |= button;
                else mask &= static_cast<std::uint16_t>(~button);
                stateChanged = stateChanged || previous != mask;
            }
            if (startPressed && leftPressed && rightPressed) exitCombo = true;
            if (event.value == 1 && uiAction == UiAction::None)
            {
                if (event.code == 305) uiAction = UiAction::Confirm;
                else if (event.code == 304) uiAction = UiAction::Back;
                else if (event.code == BTN_START) uiAction = UiAction::Start;
            }
        }
        else if (event.type == EV_ABS && (event.code == 16 || event.code == 17))
        {
            const std::uint16_t horizontal = ButtonLeft | ButtonRight;
            const std::uint16_t vertical = ButtonUp | ButtonDown;
            const std::uint16_t affected = event.code == 16 ? horizontal : vertical;
            const std::uint16_t direction = event.code == 16
                ? (event.value < 0 ? ButtonLeft : event.value > 0 ? ButtonRight : 0)
                : (event.value < 0 ? ButtonUp : event.value > 0 ? ButtonDown : 0);
            const std::uint16_t previous = mask;
            mask &= static_cast<std::uint16_t>(~affected);
            mask |= direction;
            stateChanged = stateChanged || previous != mask;
            if (event.value != 0 && uiAction == UiAction::None)
            {
                if (event.code == 16) uiAction = event.value < 0 ? UiAction::Left : UiAction::Right;
                else uiAction = event.value < 0 ? UiAction::Up : UiAction::Down;
            }
        }
    }
    return description;
}

}
