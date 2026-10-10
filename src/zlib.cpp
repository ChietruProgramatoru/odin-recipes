#include "zlib.h"

#include <algorithm>
#include <iterator>
#include <utility>

uint32_t adler32(const std::vector<uint8_t>& data) {
    uint32_t a = 1, b = 0;
    for (uint8_t v : data) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}


namespace {

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

}  // namespace

// --- Compression --------------------------------------------------------------------

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


// --- Decompression ------------------------------------------------------------------

namespace {

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size, size_t pos) : data_(data), size_(size), pos_(pos) {}
    bool failed = false;

    // Read `need` bits, least significant first.
    int bits(int need) {
        uint32_t val = buf_;
        while (count_ < need) {
            if (pos_ >= size_) {
                failed = true;
                return 0;
            }
            val |= static_cast<uint32_t>(data_[pos_++]) << count_;
            count_ += 8;
        }
        buf_ = need >= 32 ? 0 : val >> need;
        count_ -= need;
        return static_cast<int>(val & ((1u << need) - 1));
    }
    // Drop the bits left in the current byte (before a stored block or the checksum).
    void align() {
        buf_ = 0;
        count_ = 0;
    }
    size_t pos() const { return pos_; }
    bool take(size_t n, const uint8_t*& out) {
        if (size_ - pos_ < n) return false;
        out = data_ + pos_;
        pos_ += n;
        return true;
    }

private:
    const uint8_t* data_;
    size_t size_, pos_;
    uint32_t buf_ = 0;
    int count_ = 0;
};

// Canonical Huffman code: how many codes of each length, and the symbols in code order.
struct Huffman {
    short count[16];
    short symbol[288];
};

// Returns false if the code lengths are over-subscribed.
bool build(Huffman& h, const short* length, int n) {
    std::fill(std::begin(h.count), std::end(h.count), short(0));
    for (int s = 0; s < n; ++s) ++h.count[length[s]];
    if (h.count[0] == n) return true;  // no codes at all
    int left = 1;
    for (int len = 1; len < 16; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return false;
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; ++len) offs[len + 1] = static_cast<short>(offs[len] + h.count[len]);
    for (int s = 0; s < n; ++s)
        if (length[s]) h.symbol[offs[length[s]]++] = static_cast<short>(s);
    return true;
}

int decode(BitReader& in, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= in.bits(1);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

bool inflate_codes(BitReader& in, std::vector<uint8_t>& out, const Huffman& lens, const Huffman& dists, size_t max_size) {
    for (;;) {
        int sym = decode(in, lens);
        if (sym < 0 || in.failed) return false;
        if (sym < 256) {
            if (out.size() >= max_size) return false;
            out.push_back(static_cast<uint8_t>(sym));
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            size_t len = LEN_BASE[sym] + in.bits(LEN_EXTRA[sym]);
            int ds = decode(in, dists);
            if (ds < 0 || ds >= 30) return false;
            size_t dist = DIST_BASE[ds] + in.bits(DIST_EXTRA[ds]);
            if (in.failed || dist > out.size() || out.size() + len > max_size) return false;
            size_t from = out.size() - dist;
            for (size_t k = 0; k < len; ++k) out.push_back(out[from + k]);
        }
    }
}

bool inflate_fixed(BitReader& in, std::vector<uint8_t>& out, size_t max_size) {
    static const std::pair<Huffman, Huffman> codes = [] {
        short lengths[288];
        for (int s = 0; s < 288; ++s) lengths[s] = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
        std::pair<Huffman, Huffman> c;
        build(c.first, lengths, 288);
        for (int s = 0; s < 30; ++s) lengths[s] = 5;
        build(c.second, lengths, 30);
        return c;
    }();
    return inflate_codes(in, out, codes.first, codes.second, max_size);
}

bool inflate_dynamic(BitReader& in, std::vector<uint8_t>& out, size_t max_size) {
    static const int ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int nlen = in.bits(5) + 257, ndist = in.bits(5) + 1, ncode = in.bits(4) + 4;
    if (in.failed || nlen > 286 || ndist > 30) return false;
    short lengths[320] = {};
    for (int i = 0; i < ncode; ++i) lengths[ORDER[i]] = static_cast<short>(in.bits(3));
    Huffman lencode, distcode;
    if (!build(lencode, lengths, 19)) return false;
    int index = 0;
    while (index < nlen + ndist) {
        int sym = decode(in, lencode);
        if (sym < 0 || in.failed) return false;
        if (sym < 16) {
            lengths[index++] = static_cast<short>(sym);
            continue;
        }
        short len = 0;
        int repeat;
        if (sym == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            repeat = 3 + in.bits(2);
        } else if (sym == 17) {
            repeat = 3 + in.bits(3);
        } else {
            repeat = 11 + in.bits(7);
        }
        if (index + repeat > nlen + ndist) return false;
        while (repeat--) lengths[index++] = len;
    }
    if (lengths[256] == 0) return false;  // no end-of-block code
    if (!build(lencode, lengths, nlen) || !build(distcode, lengths + nlen, ndist)) return false;
    return inflate_codes(in, out, lencode, distcode, max_size);
}

}  // namespace

std::optional<std::vector<uint8_t>> zlib_decompress(const std::vector<uint8_t>& data, size_t max_size) {
    if (data.size() < 6) return std::nullopt;
    int cmf = data[0], flg = data[1];
    if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20)) return std::nullopt;
    BitReader in(data.data(), data.size(), 2);
    std::vector<uint8_t> out;
    bool last = false;
    while (!last) {
        last = in.bits(1);
        int type = in.bits(2);
        if (in.failed) return std::nullopt;
        bool ok = false;
        if (type == 0) {
            in.align();
            const uint8_t* p;
            if (!in.take(4, p)) return std::nullopt;
            unsigned len = p[0] | (p[1] << 8), nlen = p[2] | (p[3] << 8);
            if (len != (~nlen & 0xFFFF) || out.size() + len > max_size || !in.take(len, p)) return std::nullopt;
            out.insert(out.end(), p, p + len);
            ok = true;
        } else if (type == 1) {
            ok = inflate_fixed(in, out, max_size);
        } else if (type == 2) {
            ok = inflate_dynamic(in, out, max_size);
        }
        if (!ok) return std::nullopt;
    }
    in.align();
    const uint8_t* p;
    if (!in.take(4, p)) return std::nullopt;
    uint32_t expected = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    if (adler32(out) != expected) return std::nullopt;
    return out;
}
