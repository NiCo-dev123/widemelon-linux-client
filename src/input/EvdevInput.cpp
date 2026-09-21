#include "input/EvdevInput.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

namespace widemelon
{

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
    if (fileDescriptor < 0) return false;

    input_event event{};
    while (::read(fileDescriptor, &event, sizeof(event)) == sizeof(event))
    {
        if (event.type != EV_KEY) continue;
        const bool pressed = event.value != 0;
        switch (event.code)
        {
        case BTN_START: startPressed = pressed; break;
        case BTN_TL: leftPressed = pressed; break;
        case BTN_TR: rightPressed = pressed; break;
        default: break;
        }
    }
    return startPressed && leftPressed && rightPressed;
}

}
