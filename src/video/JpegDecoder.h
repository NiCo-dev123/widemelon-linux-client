#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace widemelon
{

struct DecodedJpeg
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgb;
};

class JpegDecoder
{
public:
    static bool decode(const std::vector<std::uint8_t>& jpeg, DecodedJpeg& image, std::string& error);
};

}
