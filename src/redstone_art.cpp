// Pixel art for redstone (M13): dust, redstone torches, the lever's handle, repeaters and the back
// and inside of a piston. Drawn by the code like the rest of the textures.
#include <algorithm>
#include <cmath>
#include <functional>

#include "blocks.h"
#include "rng.h"
#include "textures.h"

namespace {

constexpr uint8_t TINTED = 254;  // grey pixels the world colours in (dust takes its power's red)

struct Rgb {
    double r, g, b;
};

uint8_t clamp8(double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); }
Rgba shade(Rgb c, double f = 1.0, uint8_t a = 255) { return {clamp8(c.r * f), clamp8(c.g * f), clamp8(c.b * f), a}; }

const Rgb STONE_GREY{150, 150, 150}, COBBLE_GREY{120, 120, 120}, WOOD{150, 115, 70};

Image noise(Rng& rng, Rgb c, double spread) {
    Image img(TILE, TILE);
    for (auto& p : img.pixels) p = shade(c, 1 + rng.uniform(-spread, spread));
    return img;
}

void fill(Image& img, int x0, int y0, int x1, int y1, Rgba c) {
    for (int y = std::max(0, y0); y <= std::min(img.height - 1, y1); ++y)
        for (int x = std::max(0, x0); x <= std::min(img.width - 1, x1); ++x) img.at(x, y) = c;
}

// Dust: grains in a band 6 pixels wide, grey (coloured by its power in the world), see-through around it.
void dust_band(Image& img, Rng& rng, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const bool edge = x == x0 || x == x1 || y == y0 || y == y1;
            if (edge && rng.below(3) == 0) continue;  // a ragged edge
            img.at(x, y) = shade({230, 230, 230}, rng.uniform(0.7, 1.0), TINTED);
        }
}

Image wire_line(Rng& rng) {
    Image img(TILE, TILE);
    dust_band(img, rng, 5, 0, 10, 15);
    return img;
}

Image wire_cross(Rng& rng) {
    Image img(TILE, TILE);
    dust_band(img, rng, 5, 0, 10, 15);
    dust_band(img, rng, 0, 5, 15, 10);
    return img;
}

// A redstone torch: the torch's stick with a red tip, glowing when lit.
Image redstone_torch(Rng& rng, bool lit) {
    Image img(TILE, TILE);
    for (int y = 8; y < TILE; ++y)
        for (int x = 7; x <= 8; ++x) img.at(x, y) = shade({120, 88, 50}, (x == 7 ? 1.05 : 0.85) * rng.uniform(0.92, 1.08));
    if (lit) {
        img.at(7, 6) = {255, 120, 100, 255};
        img.at(8, 6) = {255, 70, 50, 255};
        img.at(7, 7) = {230, 30, 20, 255};
        img.at(8, 7) = {190, 20, 15, 255};
    } else {
        img.at(7, 6) = {110, 40, 35, 255};
        img.at(8, 6) = {90, 30, 25, 255};
        img.at(7, 7) = {80, 25, 20, 255};
        img.at(8, 7) = {65, 20, 15, 255};
    }
    return img;
}

// The lever's handle: a wooden stick down the middle columns.
Image lever(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 7; x <= 8; ++x) img.at(x, y) = shade(WOOD, (x == 7 ? 1.05 : 0.85) * rng.uniform(0.9, 1.1));
    return img;
}

// The top of a repeater: smooth stone with a line of dust from back to front and an arrow head at the
// front (the top of the picture), red when it is on.
Image repeater_top(Rng& rng, bool on) {
    Image img = noise(rng, STONE_GREY, 0.05);
    for (int x = 0; x < TILE; ++x) img.at(x, 0) = img.at(x, TILE - 1) = shade(STONE_GREY, 0.8);
    for (int y = 0; y < TILE; ++y) img.at(0, y) = img.at(TILE - 1, y) = shade(STONE_GREY, 0.8);
    const Rgb dust = on ? Rgb{235, 30, 20} : Rgb{110, 20, 15};
    for (int y = 3; y <= 13; ++y)
        for (int x = 7; x <= 8; ++x) img.at(x, y) = shade(dust, rng.uniform(0.85, 1.05));
    for (int k = 0; k < 3; ++k) {  // the arrow head
        img.at(6 - k, 3 + k) = shade(dust, 0.95);
        img.at(9 + k, 3 + k) = shade(dust, 0.95);
    }
    return img;
}

// The back of a piston: cobblestone with a dark square in the middle.
Image piston_bottom(Rng& rng) {
    Image img = noise(rng, COBBLE_GREY, 0.12);
    for (int x = 0; x < TILE; ++x) img.at(x, 0) = img.at(x, TILE - 1) = shade(COBBLE_GREY, 0.7);
    for (int y = 0; y < TILE; ++y) img.at(0, y) = img.at(TILE - 1, y) = shade(COBBLE_GREY, 0.7);
    fill(img, 5, 5, 10, 10, shade({70, 70, 70}));
    return img;
}

// The front of a pushed-out piston: cobblestone round the hole the arm comes out of.
Image piston_inner(Rng& rng) {
    Image img = noise(rng, COBBLE_GREY, 0.12);
    fill(img, 3, 3, 12, 12, shade({60, 50, 40}));
    fill(img, 6, 6, 9, 9, shade(WOOD, rng.uniform(0.9, 1.0)));
    return img;
}

}  // namespace

std::map<int, Image> make_redstone_tiles() {
    std::map<int, std::function<Image(Rng&)>> gen = {
        {T_WIRE_CROSS, wire_cross},
        {T_WIRE_LINE, wire_line},
        {T_REDSTONE_TORCH_ON, [](Rng& r) { return redstone_torch(r, true); }},
        {T_REDSTONE_TORCH_OFF, [](Rng& r) { return redstone_torch(r, false); }},
        {T_LEVER, lever},
        {T_REPEATER_OFF, [](Rng& r) { return repeater_top(r, false); }},
        {T_REPEATER_ON, [](Rng& r) { return repeater_top(r, true); }},
        {T_PISTON_BOTTOM, piston_bottom},
        {T_PISTON_INNER, piston_inner},
    };
    std::map<int, Image> tiles;
    for (const auto& [index, g] : gen) {
        Rng rng(static_cast<uint64_t>(index) * 7919 + 13);
        tiles[index] = g(rng);
    }
    return tiles;
}
