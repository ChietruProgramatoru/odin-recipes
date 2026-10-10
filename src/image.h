// A plain RGBA image in memory, top row first.
#pragma once
#include <cstdint>
#include <vector>

struct Rgba {
    uint8_t r = 0, g = 0, b = 0, a = 0;
};

struct Image {
    int width = 0, height = 0;
    std::vector<Rgba> pixels;

    Image() = default;
    Image(int w, int h, Rgba fill = {}) : width(w), height(h), pixels(static_cast<size_t>(w) * h, fill) {}
    Rgba& at(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    const Rgba& at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
    void paste(const Image& src, int x0, int y0) {
        for (int y = 0; y < src.height; ++y)
            for (int x = 0; x < src.width; ++x) at(x0 + x, y0 + y) = src.at(x, y);
    }
};
