#include "video/JpegDecoder.h"

#include <csetjmp>
#include <cstdio>

extern "C"
{
#include <jpeglib.h>
}

namespace
{

struct JpegErrorManager
{
    jpeg_error_mgr base{};
    std::jmp_buf jumpBuffer{};
    char message[JMSG_LENGTH_MAX]{};
};

void onJpegError(j_common_ptr decoder)
{
    auto* error = reinterpret_cast<JpegErrorManager*>(decoder->err);
    (*decoder->err->format_message)(decoder, error->message);
    std::longjmp(error->jumpBuffer, 1);
}

}

namespace widemelon
{

bool JpegDecoder::decode(const std::vector<std::uint8_t>& jpeg, DecodedJpeg& image, std::string& error)
{
    if (jpeg.empty())
    {
        error = "Empty JPEG payload";
        return false;
    }

    jpeg_decompress_struct decoder{};
    JpegErrorManager jpegError{};
    DecodedJpeg decoded;
    decoder.err = jpeg_std_error(&jpegError.base);
    jpegError.base.error_exit = onJpegError;
    if (setjmp(jpegError.jumpBuffer) != 0)
    {
        jpeg_destroy_decompress(&decoder);
        error = jpegError.message;
        return false;
    }

    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, jpeg.data(), static_cast<unsigned long>(jpeg.size()));
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    if (decoder.output_width != 256 || decoder.output_height != 192 || decoder.output_components != 3)
    {
        jpeg_destroy_decompress(&decoder);
        error = "Unexpected JPEG dimensions or color format";
        return false;
    }

    decoded.width = static_cast<std::uint16_t>(decoder.output_width);
    decoded.height = static_cast<std::uint16_t>(decoder.output_height);
    decoded.rgb.resize(static_cast<std::size_t>(decoded.width) * decoded.height * 3);
    while (decoder.output_scanline < decoder.output_height)
    {
        JSAMPROW row = decoded.rgb.data() + static_cast<std::size_t>(decoder.output_scanline) * decoded.width * 3;
        jpeg_read_scanlines(&decoder, &row, 1);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    image = std::move(decoded);
    return true;
}

}
