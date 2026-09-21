#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace widemelon
{

struct VideoJpegFrame
{
    std::uint32_t sequence = 0;
    std::uint64_t capturedUs = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> jpeg;
};

class WideMelonProtocol
{
public:
    static bool parseVideoFrame(const std::string& payload, VideoJpegFrame& frame, std::string& error);
};

}
