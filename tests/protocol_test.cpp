#include "config/Config.h"
#include "protocol/WideMelonProtocol.h"
#ifdef WIDEMELON_HAVE_JPEG
#include "video/JpegDecoder.h"
#endif

#include <cstdint>
#ifdef WIDEMELON_HAVE_JPEG
#include <cstdlib>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef WIDEMELON_HAVE_JPEG
extern "C"
{
#include <jpeglib.h>
}
#endif

namespace
{

int failures = 0;

void expect(bool condition, const std::string& description)
{
    if (!condition)
    {
        std::cerr << "Failed: " << description << '\n';
        ++failures;
    }
}

widemelon::ConfigLoadResult loadText(const std::string& text)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("widemelon-client-config-" + std::to_string(getpid()) + ".conf");
    {
        std::ofstream file(path);
        file << text;
    }
    const widemelon::ConfigLoadResult result = widemelon::ConfigLoader::loadFile(path);
    std::filesystem::remove(path);
    return result;
}

#ifdef WIDEMELON_HAVE_JPEG
std::vector<std::uint8_t> makeJpeg()
{
    std::vector<std::uint8_t> rgb(256 * 192 * 3, 0);
    for (std::size_t pixel = 0; pixel < rgb.size(); pixel += 3) rgb[pixel] = 255;
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    unsigned char* encoded = nullptr;
    unsigned long encodedSize = 0;
    jpeg_mem_dest(&encoder, &encoded, &encodedSize);
    encoder.image_width = 256;
    encoder.image_height = 192;
    encoder.input_components = 3;
    encoder.in_color_space = JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_start_compress(&encoder, TRUE);
    while (encoder.next_scanline < encoder.image_height)
    {
        JSAMPROW row = rgb.data() + static_cast<std::size_t>(encoder.next_scanline) * 256 * 3;
        jpeg_write_scanlines(&encoder, &row, 1);
    }
    jpeg_finish_compress(&encoder);
    std::vector<std::uint8_t> result(encoded, encoded + encodedSize);
    std::free(encoded);
    jpeg_destroy_compress(&encoder);
    return result;
}
#endif

#ifdef WIDEMELON_HAVE_JPEG
void appendLe32(std::string& value, std::uint32_t number)
{
    for (int index = 0; index < 4; ++index) value.push_back(static_cast<char>(number >> (index * 8)));
}

void appendLe64(std::string& value, std::uint64_t number)
{
    for (int index = 0; index < 8; ++index) value.push_back(static_cast<char>(number >> (index * 8)));
}

std::string makeVideoPacket(const std::vector<std::uint8_t>& jpeg)
{
    std::string packet("WMF2", 4);
    appendLe32(packet, 42);
    appendLe64(packet, 123456);
    packet.push_back(0);
    packet.push_back(1);
    packet.push_back(192);
    packet.push_back(0);
    packet.push_back(1);
    packet.append(3, '\0');
    packet.append(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    return packet;
}
#endif

}

int main()
{
    const auto valid = loadText("host=192.168.1.20\nport=24872\npairing_code=1234567890\n");
    expect(valid.ok, "accepts a valid configuration");
    expect(valid.config.host == "192.168.1.20", "loads host");
    expect(valid.config.port == 24872, "loads port");

    const auto defaultPort = loadText("host=192.168.1.20\npairing_code=1234567890\n");
    expect(defaultPort.ok && defaultPort.config.port == 24872, "uses the default port");

    expect(!loadText("host=example.com\npairing_code=1234567890\n").ok,
           "rejects a non-IPv4 host");
    expect(!loadText("host=192.168.1.20\nport=70000\npairing_code=1234567890\n").ok,
           "rejects an invalid port");
    expect(loadText("host=192.168.1.20\npairing_code=873321355\n").ok,
           "accepts the server's nine-digit pairing code");
    expect(!loadText("host=192.168.1.20\nunknown=value\npairing_code=1234567890\n").ok,
           "rejects unknown settings");

    widemelon::VideoJpegFrame videoFrame;
    std::string videoError;
#ifdef WIDEMELON_HAVE_JPEG
    const std::vector<std::uint8_t> jpeg = makeJpeg();
    expect(widemelon::WideMelonProtocol::parseVideoFrame(makeVideoPacket(jpeg), videoFrame, videoError),
           "parses a WMF2 JPEG frame");
    expect(videoFrame.sequence == 42 && videoFrame.capturedUs == 123456 && videoFrame.jpeg == jpeg,
           "retains the JPEG payload and frame metadata");
    widemelon::DecodedJpeg decoded;
    expect(widemelon::JpegDecoder::decode(videoFrame.jpeg, decoded, videoError), "decodes a JPEG frame");
    expect(decoded.width == 256 && decoded.height == 192 && decoded.rgb.size() == 256 * 192 * 3,
           "returns the expected RGB image");
#endif
    expect(!widemelon::WideMelonProtocol::parseVideoFrame("invalid", videoFrame, videoError),
           "rejects malformed video frames");

    return failures == 0 ? 0 : 1;
}
