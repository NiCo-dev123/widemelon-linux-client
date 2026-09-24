#include "input/EvdevInput.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <algorithm>
#include <sys/ioctl.h>
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
constexpr std::uint16_t DirectionMask = ButtonRight | ButtonLeft | ButtonUp | ButtonDown;

void configureStickAxis(int fileDescriptor, unsigned int axis, int& center, int& threshold)
{
    input_absinfo info{};
    if (ioctl(fileDescriptor, EVIOCGABS(axis), &info) != 0) return;
    center = info.minimum + (info.maximum - info.minimum) / 2;
    threshold = std::max(info.flat, (info.maximum - info.minimum) / 4);
}
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
    configureStickAxis(fileDescriptor, ABS_X, leftStickXCenter, leftStickXThreshold);
    configureStickAxis(fileDescriptor, ABS_Y, leftStickYCenter, leftStickYThreshold);
    return true;
}

void EvdevInput::mergeDirectionalSources()
{
    const std::uint16_t previous = mask;
    mask = static_cast<std::uint16_t>((mask & ~DirectionMask) | dpadMask | leftStickMask);
    stateChanged = stateChanged || previous != mask;
}

void EvdevInput::updateDirectionMask(std::uint16_t& sourceMask, bool horizontal, int value, int center, int threshold)
{
    const std::uint16_t affected = horizontal ? ButtonLeft | ButtonRight : ButtonUp | ButtonDown;
    const std::uint16_t direction = horizontal
        ? (value < center - threshold ? ButtonLeft : value > center + threshold ? ButtonRight : 0)
        : (value < center - threshold ? ButtonUp : value > center + threshold ? ButtonDown : 0);
    const std::uint16_t updatedMask = static_cast<std::uint16_t>((sourceMask & ~affected) | direction);
    // Analog hardware can emit many position samples while held. Only a
    // threshold crossing is an input transition, so keep the current D-pad
    // direction latched until the stick returns to the dead zone.
    if (updatedMask == sourceMask) return;
    sourceMask = updatedMask;
    mergeDirectionalSources();
}

void EvdevInput::setUiDirection(bool horizontal, int value, int center, int threshold)
{
    if (uiAction != UiAction::None || (value >= center - threshold && value <= center + threshold)) return;
    if (horizontal) uiAction = value < center ? UiAction::Left : UiAction::Right;
    else uiAction = value < center ? UiAction::Up : UiAction::Down;
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
                else if (event.code == 304) uiAction = UiAction::Delete;
                else if (event.code == 308) uiAction = UiAction::Back;
                else if (event.code == BTN_START) uiAction = UiAction::Start;
            }
        }
        else if (event.type == EV_ABS && (event.code == ABS_HAT0X || event.code == ABS_HAT0Y))
        {
            const bool horizontal = event.code == ABS_HAT0X;
            updateDirectionMask(dpadMask, horizontal, event.value, 0, 0);
            setUiDirection(horizontal, event.value, 0, 0);
        }
        else if (event.type == EV_ABS && (event.code == ABS_X || event.code == ABS_Y))
        {
            const bool horizontal = event.code == ABS_X;
            const int center = horizontal ? leftStickXCenter : leftStickYCenter;
            const int threshold = horizontal ? leftStickXThreshold : leftStickYThreshold;
            updateDirectionMask(leftStickMask, horizontal, event.value, center, threshold);
            setUiDirection(horizontal, event.value, center, threshold);
        }
    }
    return description;
}

}
