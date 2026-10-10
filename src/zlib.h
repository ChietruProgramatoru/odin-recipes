// zlib streams (RFC 1950/1951): a small compressor and a full decompressor.
// Used for PNG screenshots and for the chunk data in save files.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

uint32_t adler32(const std::vector<uint8_t>& data);
// One fixed-Huffman deflate block with LZ77 matching. Any zlib reader can read it.
std::vector<uint8_t> zlib_compress(const std::vector<uint8_t>& data);
// Decompress any valid zlib stream (stored, fixed and dynamic Huffman blocks).
// Returns nothing if the stream is damaged, fails its checksum or would exceed max_size bytes.
std::optional<std::vector<uint8_t>> zlib_decompress(const std::vector<uint8_t>& data, size_t max_size = 64u << 20);
