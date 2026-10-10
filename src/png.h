// Minimal PNG writer (RGB, 8 bits per channel) with its own small deflate compressor.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Encode an RGB image, top row first, into PNG file bytes.
std::vector<uint8_t> encode_png(int width, int height, const std::vector<uint8_t>& rgb);
bool write_png(const std::string& path, int width, int height, const std::vector<uint8_t>& rgb);
