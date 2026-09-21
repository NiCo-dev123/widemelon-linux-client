#pragma once

#include <cstdint>
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
    std::string pollEvent();
    bool exitComboPressed();
    std::uint16_t buttonMask() const { return mask; }
    bool takeStateChanged();

private:
    int fileDescriptor = -1;
    bool startPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;
    bool exitCombo = false;
    std::uint16_t mask = 0;
    bool stateChanged = false;
};

}
