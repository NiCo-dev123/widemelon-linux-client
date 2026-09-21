#pragma once

#include <string>

namespace widemelon
{

class EvdevInput
{
public:
    EvdevInput() = default;
    ~EvdevInput();

    EvdevInput(const EvdevInput&) = delete;
    EvdevInput& operator=(const EvdevInput&) = delete;

    bool open(const std::string& path, std::string& error);
    bool exitComboPressed();

private:
    int fileDescriptor = -1;
    bool startPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;
};

}
