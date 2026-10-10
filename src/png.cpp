#include "png.h"

#include <algorithm>
#include <array>
#include <cstdio>

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

uint32_t adler32(const std::vector<uint8_t>& data) {
    uint32_t a = 1, b = 0;
    for (uint8_t v : data) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

class BitWriter {
public:
    std::vector<uint8_t> out;
    void bits(uint32_t value, int count) {  // least significant bit first
        for (int i = 0; i < count; ++i) {
            if (bit_ == 0) out.push_back(0);
            if (value & (1u << i)) out.back() |= static_cast<uint8_t>(1u << bit_);
            bit_ = (bit_ + 1) & 7;
        }
    }
    void code(uint32_t code, int length) {  // Huffman codes go most significant bit first
        for (int i = length - 1; i >= 0; --i) bits((code >> i) & 1, 1);
    }

private:
    int bit_ = 0;
};

void literal(BitWriter& w, int v) {  // fixed Huffman table from the deflate spec
    if (v < 144)
        w.code(0x30 + v, 8);
    else if (v < 256)
        w.code(0x190 + (v - 144), 9);
    else if (v < 280)
        w.code(v - 256, 7);
    else
        w.code(0xC0 + (v - 280), 8);
}

const int LEN_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                           257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void match(BitWriter& w, int length, int dist) {
    int li = 28;
    while (LEN_BASE[li] > length) --li;
    literal(w, 257 + li);
    w.bits(length - LEN_BASE[li], LEN_EXTRA[li]);
    int di = 29;
    while (DIST_BASE[di] > dist) --di;
    w.code(di, 5);
    w.bits(dist - DIST_BASE[di], DIST_EXTRA[di]);
}

// zlib stream: one fixed-Huffman deflate block with hash-chain LZ77 matching.
std::vector<uint8_t> zlib_compress(const std::vector<uint8_t>& data) {
    constexpr int WINDOW = 32768, HASH = 1 << 15, MAX_CHAIN = 24, MIN_MATCH = 3, MAX_MATCH = 258;
    BitWriter w;
    w.out = {0x78, 0x01};
    w.bits(1, 1);  // final block
    w.bits(1, 2);  // fixed Huffman codes
    std::vector<int> head(HASH, -1), prev(data.size(), -1);
    const int n = static_cast<int>(data.size());
    auto hash = [&](int i) {
        return ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & (HASH - 1);
    };
    auto insert = [&](int i) {
        if (i + MIN_MATCH > n) return;
        int h = hash(i);
        prev[i] = head[h];
        head[h] = i;
    };
    int i = 0;
    while (i < n) {
        int best_len = 0, best_dist = 0;
        if (i + MIN_MATCH <= n) {
            int cand = head[hash(i)];
            for (int chain = 0; cand >= 0 && i - cand <= WINDOW && chain < MAX_CHAIN; ++chain, cand = prev[cand]) {
                int len = 0, limit = std::min(MAX_MATCH, n - i);
                while (len < limit && data[cand + len] == data[i + len]) ++len;
                if (len > best_len) {
                    best_len = len;
                    best_dist = i - cand;
                    if (len == limit) break;
                }
            }
        }
        if (best_len >= MIN_MATCH) {
            match(w, best_len, best_dist);
            for (int k = 0; k < best_len; ++k) insert(i + k);
            i += best_len;
        } else {
            literal(w, data[i]);
            insert(i);
            ++i;
        }
    }
    literal(w, 256);  // end of block
    uint32_t ad = adler32(data);
    for (int s = 24; s >= 0; s -= 8) w.out.push_back(static_cast<uint8_t>(ad >> s));
    return w.out;
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
