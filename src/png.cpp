#include "png.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "zlib.h"

namespace {

uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<uint8_t>(x >> s));
}

void chunk(std::vector<uint8_t>& png, const char* type, const std::vector<uint8_t>& body) {
    put32(png, static_cast<uint32_t>(body.size()));
    size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), body.begin(), body.end());
    put32(png, crc32(png.data() + start, png.size() - start));
}

}  // namespace

std::vector<uint8_t> encode_png(int width, int height, const std::vector<uint8_t>& rgb) {
    // Each row gets the "Sub" filter: store the difference to the pixel on the left.
    const size_t stride = static_cast<size_t>(width) * 3;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * height);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = rgb.data() + y * stride;
        raw.push_back(1);
        for (size_t x = 0; x < stride; ++x) raw.push_back(static_cast<uint8_t>(row[x] - (x >= 3 ? row[x - 3] : 0)));
    }
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(width));
    put32(ihdr, static_cast<uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8-bit RGB, no interlacing
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", zlib_compress(raw));
    chunk(png, "IEND", {});
    return png;
}

bool write_png(const std::string& path, int width, int height, const std::vector<uint8_t>& rgb) {
    std::vector<uint8_t> png = encode_png(width, height, rgb);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    return std::fclose(f) == 0 && ok;
}
