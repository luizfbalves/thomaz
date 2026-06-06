#include "platform/image_transcode.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <webp/decode.h>

// stb_image_write provides stbi_write_png_to_func (in-memory PNG encode). We
// only need the memory path, so disable the stdio file writers. The
// IMPLEMENTATION macro must live in exactly one translation unit — this one.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

namespace thomaz::platform {

namespace {

// WebP files are a RIFF container: "RIFF" <4-byte size> "WEBP" ...
bool is_webp(const std::string& b) {
    return b.size() >= 12 &&
           std::memcmp(b.data(), "RIFF", 4) == 0 &&
           std::memcmp(b.data() + 8, "WEBP", 4) == 0;
}

// stb_image_write callback: append each chunk to the destination string.
void append_chunk(void* ctx, void* data, int size) {
    if (size <= 0) return;
    auto* out = static_cast<std::string*>(ctx);
    out->append(static_cast<const char*>(data), static_cast<size_t>(size));
}

} // namespace

std::string to_decodable_image(std::string bytes) {
    if (!is_webp(bytes))
        return bytes;

    int w = 0, h = 0;
    uint8_t* rgba = WebPDecodeRGBA(
        reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &w, &h);
    if (rgba == nullptr || w <= 0 || h <= 0) {
        // libwebp allocates even on some partial failures; release if non-null.
        if (rgba != nullptr) std::free(rgba);
        return bytes; // leave the caller to attempt + log the raw bytes
    }

    // The PNG is decoded again immediately and thrown away, so its size is
    // irrelevant — encode for speed, not ratio (level 1 ~= fastest deflate).
    stbi_write_png_compression_level = 1;

    std::string png;
    png.reserve(bytes.size()); // PNG of a small preview is in the same ballpark
    const int ok = stbi_write_png_to_func(
        append_chunk, &png, w, h, /*comp=*/4, rgba, /*stride=*/w * 4);
    std::free(rgba);

    if (ok == 0 || png.empty())
        return bytes; // encode failed — fall back to the original bytes

    return png;
}

} // namespace thomaz::platform
