#include "protocol/WideMelonProtocol.h"

#include <cstring>

namespace
{

constexpr std::size_t VideoHeaderSize = 24;
constexpr std::size_t MaximumJpegBytes = 2 * 1024 * 1024;

std::uint16_t readLe16(const char* bytes)
{
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0]))
        | (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1])) << 8);
}

std::uint32_t readLe32(const char* bytes)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
}

std::uint64_t readLe64(const char* bytes)
{
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index)
        value = (value << 8) | static_cast<unsigned char>(bytes[index]);
    return value;
}

}

namespace widemelon
{

bool WideMelonProtocol::parseVideoFrame(const std::string& payload, VideoJpegFrame& frame, std::string& error)
{
    if (payload.size() < VideoHeaderSize)
    {
        error = "Video frame is shorter than the 24-byte header";
        return false;
    }
    if (std::memcmp(payload.data(), "WMF2", 4) != 0)
    {
        error = "Video frame magic is not WMF2";
        return false;
    }
    if (static_cast<unsigned char>(payload[20]) != 1)
    {
        error = "Video frame is not JPEG";
        return false;
    }
    if (payload.size() - VideoHeaderSize > MaximumJpegBytes)
    {
        error = "Video JPEG exceeds the 2 MiB limit";
        return false;
    }

    frame.sequence = readLe32(payload.data() + 4);
    frame.capturedUs = readLe64(payload.data() + 8);
    frame.width = readLe16(payload.data() + 16);
    frame.height = readLe16(payload.data() + 18);
    if (frame.width != 256 || frame.height != 192)
    {
        error = "Unexpected video dimensions";
        return false;
    }
    frame.jpeg.assign(payload.begin() + VideoHeaderSize, payload.end());
    if (frame.jpeg.empty())
    {
        error = "Video frame has no JPEG payload";
        return false;
    }
    return true;
}

}
