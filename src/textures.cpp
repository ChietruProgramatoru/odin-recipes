#include "textures.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "blocks.h"
#include "rng.h"

namespace {

struct Rgb {
    double r, g, b;
};

uint8_t clamp8(double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); }

Rgba shade(Rgb c, double f, uint8_t a = 255) { return {clamp8(c.r * f), clamp8(c.g * f), clamp8(c.b * f), a}; }

Image noise_tile(Rng& rng, Rgb base, double spread = 0.15, uint8_t alpha = 255) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade(base, 1 + rng.uniform(-spread, spread), alpha);
    return img;
}

const Rgb GRASS_GREEN{95, 159, 53};
const Rgb DIRT_BROWN{134, 96, 67};

Image grass_top(Rng& rng) { return noise_tile(rng, GRASS_GREEN, 0.18); }

Image dirt(Rng& rng) {
    Image img = noise_tile(rng, DIRT_BROWN, 0.12);
    for (int i = 0; i < 14; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        img.at(x, y) = shade(DIRT_BROWN, rng.choice({0.7, 1.25}));
    }
    return img;
}

Image grass_side(Rng& rng) {
    Image img = dirt(rng);
    for (int x = 0; x < TILE; ++x) {
        int depth = 3 + rng.choice({0, 0, 1, 1, 2});
        for (int y = 0; y < depth; ++y) img.at(x, y) = shade(GRASS_GREEN, 1 + rng.uniform(-0.15, 0.15));
    }
    return img;
}

Image stone(Rng& rng) {
    const Rgb grey{125, 125, 125};
    Image img = noise_tile(rng, grey, 0.08);
    for (int i = 0; i < 6; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE), n = rng.randint(2, 5);
        for (int k = 0; k < n; ++k) img.at((x + k) % TILE, y) = shade(grey, 0.78);
    }
    return img;
}

int wrap_dist(int a, int b) { return std::min(std::abs(a - b), TILE - std::abs(a - b)); }

Image cobble(Rng& rng) {
    // Voronoi-ish stones with dark mortar between them.
    Image img(TILE, TILE);
    struct Seed {
        int x, y;
        double f;
    };
    std::vector<Seed> seeds;
    for (int i = 0; i < 9; ++i) {
        int sx = rng.below(TILE), sy = rng.below(TILE);
        seeds.push_back({sx, sy, rng.uniform(0.8, 1.15)});
    }
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            std::vector<std::pair<int, double>> d;
            for (const Seed& s : seeds) {
                int dx = wrap_dist(x, s.x), dy = wrap_dist(y, s.y);
                d.push_back({dx * dx + dy * dy, s.f});
            }
            std::sort(d.begin(), d.end());
            bool edge = d[1].first - d[0].first < 6;
            double f = edge ? 0.55 : d[0].second * rng.uniform(0.93, 1.07);
            img.at(x, y) = shade({128, 128, 128}, f);
        }
    return img;
}

Image sand(Rng& rng) { return noise_tile(rng, {219, 207, 163}, 0.07); }

const Rgb BARK{104, 83, 50};

Image log_side(Rng& rng) {
    Image img(TILE, TILE);
    double col_f[TILE];
    for (double& f : col_f) f = rng.uniform(0.8, 1.15);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade(BARK, col_f[x] * rng.uniform(0.94, 1.06));
    return img;
}

Image log_top(Rng& rng) {
    Image img(TILE, TILE);
    double c = (TILE - 1) / 2.0;
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double r = std::max(std::abs(x - c), std::abs(y - c));
            if (r > 6.6) {
                img.at(x, y) = shade(BARK, rng.uniform(0.85, 1.05));
            } else {
                double ring = static_cast<int>(r) % 2 ? 0.85 : 1.0;
                img.at(x, y) = shade({176, 143, 88}, ring * rng.uniform(0.95, 1.05));
            }
        }
    return img;
}

const Rgb LEAF_GREEN{58, 120, 36};

// Fancy leaves: see-through gaps between the leaves.
Image leaves(Rng& rng) {
    Image img = noise_tile(rng, LEAF_GREEN, 0.22);
    for (int i = 0; i < 26; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        img.at(x, y) = shade(LEAF_GREEN, 0.5);
    }
    for (int i = 0; i < 40; ++i) img.at(rng.below(TILE), rng.below(TILE)).a = 0;
    return img;
}

// Fast leaves: solid, with dark gaps instead of holes.
Image leaves_fast(Rng& rng) {
    Image img = noise_tile(rng, LEAF_GREEN, 0.22);
    for (int i = 0; i < 50; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        img.at(x, y) = shade(LEAF_GREEN, 0.3);
    }
    return img;
}

Image planks(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y) {
        int board = y / 4;
        int offset = (board * 7) % TILE;
        double board_f = rng.uniform(0.92, 1.08);
        for (int x = 0; x < TILE; ++x) {
            bool seam = (y % 4 == 3) || ((x + offset) % TILE == 0);
            double f = seam ? 0.62 : board_f * rng.uniform(0.95, 1.05);
            img.at(x, y) = shade({162, 130, 78}, f);
        }
    }
    return img;
}

Image bedrock(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade({90, 90, 90}, rng.choice({0.4, 0.6, 0.8, 1.0, 1.2}));
    return img;
}

Image water(Rng& rng) { return noise_tile(rng, {50, 90, 200}, 0.1, 170); }

Image brick(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y) {
        int row = y / 4;
        int offset = row % 2 ? 4 : 0;
        for (int x = 0; x < TILE; ++x) {
            bool mortar = (y % 4 == 3) || ((x + offset) % 8 == 7);
            if (mortar)
                img.at(x, y) = shade({190, 185, 175}, rng.uniform(0.9, 1.0));
            else
                img.at(x, y) = shade({150, 72, 55}, rng.uniform(0.85, 1.12));
        }
    }
    return img;
}

Image cow_hide(Rng& rng) {
    Image img(TILE, TILE);
    struct Patch {
        int x, y;
        double r;
    };
    std::vector<Patch> patches;
    for (int i = 0; i < 4; ++i) {
        int px = rng.below(TILE), py = rng.below(TILE);
        patches.push_back({px, py, rng.uniform(3, 5)});
    }
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            bool dark = false;
            for (const Patch& p : patches) {
                int dx = wrap_dist(x, p.x), dy = wrap_dist(y, p.y);
                if (dx * dx + dy * dy < p.r * p.r) dark = true;
            }
            img.at(x, y) = shade(dark ? Rgb{60, 40, 30} : Rgb{235, 230, 225}, rng.uniform(0.94, 1.06));
        }
    return img;
}

// Glowstone: bright yellow crystals with darker amber cracks between them.
Image glowstone(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double r = rng.random();
            Rgb c = r < 0.18 ? Rgb{150, 100, 45} : r < 0.55 ? Rgb{215, 170, 90} : Rgb{252, 222, 140};
            img.at(x, y) = shade(c, rng.uniform(0.92, 1.08));
        }
    return img;
}

// Torch: a stick in the two middle columns with a flame on top, see-through around it.
// Only columns 7-8, rows 6-15 are drawn (see add_torch in world.cpp).
Image torch(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 8; y < TILE; ++y)
        for (int x = 7; x <= 8; ++x) img.at(x, y) = shade({120, 88, 50}, (x == 7 ? 1.05 : 0.85) * rng.uniform(0.92, 1.08));
    img.at(7, 6) = {255, 250, 200, 255};
    img.at(8, 6) = {255, 220, 120, 255};
    img.at(7, 7) = {255, 190, 60, 255};
    img.at(8, 7) = {230, 140, 40, 255};
    return img;
}

std::function<Image(Rng&)> solid(Rgb c, double spread = 0.04) {
    return [=](Rng& rng) { return noise_tile(rng, c, spread); };
}

// --- Pixel-art sprites --------------------------------------------------------

using Palette = std::vector<std::pair<char, Rgba>>;

Image sprite(const std::vector<const char*>& rows, const Palette& palette) {
    int h = static_cast<int>(rows.size()), w = static_cast<int>(std::char_traits<char>::length(rows[0]));
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (const auto& [ch, color] : palette)
                if (rows[y][x] == ch) img.at(x, y) = color;
    return img;
}

std::function<Image(Rng&)> padded_sprite(std::vector<const char*> rows, Palette palette) {
    return [=](Rng&) {
        Image img(TILE, TILE);
        Image spr = sprite(rows, palette);
        img.paste(spr, (TILE - spr.width) / 2, (TILE - spr.height) / 2);
        return img;
    };
}

const std::vector<const char*> HEART = {
    ".kk...kk.",
    "krrk.krrk",
    "krwrkrrrk",
    "krrrrrrrk",
    ".krrrrrk.",
    "..krrrk..",
    "...krk...",
    "....k....",
};
const std::vector<const char*> DRUMSTICK = {
    ".....kkkk",
    "....kbbbk",
    "...kbbbbk",
    "..kbbbbbk",
    ".kbbbbbk.",
    "kwkbbkk..",
    "kwwkk....",
    ".kk......",
};
const std::vector<const char*> APPLE_SPRITE = {
    ".....g....",
    "....g.....",
    "..rr.rrr..",
    ".rrrrrrwr.",
    ".rrrrrrrr.",
    ".rrrrrrrr.",
    ".rrrrrrrr.",
    "..rrrrrr..",
    "...rr.rr..",
};
const std::vector<const char*> BEEF_SPRITE = {
    "..........",
    "...kkkk...",
    "..krrrpk..",
    ".krrrrrpk.",
    "krrwrrrrk.",
    "krrrrwrrk.",
    ".krrrrrrk.",
    "..kkrrrk..",
    "....kkk...",
};

// Left half from `full`, right half from `empty`.
Image halve(const Image& full, const Image& empty) {
    Image img = empty;
    for (int y = 0; y < full.height; ++y)
        for (int x = 0; x < full.width / 2 + 1; ++x) img.at(x, y) = full.at(x, y);
    return img;
}

}  // namespace

std::array<float, 4> tile_uv(int index) {
    int col = index % ATLAS_COLS, row = index / ATLAS_COLS;
    const float inset = 0.0005f;
    float u0 = static_cast<float>(col) / ATLAS_COLS + inset;
    float u1 = static_cast<float>(col + 1) / ATLAS_COLS - inset;
    float v1 = 1.0f - static_cast<float>(row) / ATLAS_ROWS - inset;
    float v0 = 1.0f - static_cast<float>(row + 1) / ATLAS_ROWS + inset;
    return {u0, v0, u1, v1};
}

std::map<int, Image> make_tiles(int seed) {
    const std::map<int, std::function<Image(Rng&)>> generators = {
        {T_GRASS_TOP, grass_top}, {T_GRASS_SIDE, grass_side}, {T_DIRT, dirt}, {T_STONE, stone},
        {T_COBBLE, cobble}, {T_SAND, sand}, {T_LOG_SIDE, log_side}, {T_LOG_TOP, log_top},
        {T_LEAVES, leaves}, {T_PLANKS, planks}, {T_GLOWSTONE, glowstone}, {T_TORCH, torch}, {T_BEDROCK, bedrock}, {T_WATER, water}, {T_BRICK, brick},
        {T_COW_HIDE, cow_hide}, {T_WHITE, solid({255, 255, 255}, 0)}, {T_HOOF, solid({70, 48, 35})},
        {T_COW_HEAD, solid({85, 60, 45})}, {T_SNOUT, solid({225, 165, 160})}, {T_BLACK, solid({16, 16, 16}, 0)},
        {T_HORN, solid({235, 230, 215})}, {T_LEAVES_FAST, leaves_fast},
        {T_SKIN, solid({222, 170, 130})}, {T_HAIR, solid({84, 52, 28}, 0.1)}, {T_SHIRT, solid({196, 72, 48}, 0.08)},
        {T_PANTS, solid({52, 62, 112}, 0.08)}, {T_SHOE, solid({64, 64, 64})}, {T_EYE, solid({240, 240, 240}, 0)},
        {T_APPLE, padded_sprite(APPLE_SPRITE, {{'g', {70, 110, 30, 255}}, {'r', {210, 30, 30, 255}},
                                               {'w', {255, 180, 180, 255}}})},
        {T_BEEF, padded_sprite(BEEF_SPRITE, {{'k', {90, 20, 20, 255}}, {'r', {215, 70, 70, 255}},
                                             {'p', {240, 150, 150, 255}}, {'w', {245, 225, 225, 255}}})},
    };
    std::map<int, Image> tiles;
    for (const auto& [index, gen] : generators) {
        Rng rng(static_cast<uint64_t>(seed) * 100 + index);
        tiles[index] = gen(rng);
    }
    return tiles;
}

Image make_atlas(const std::map<int, Image>& tiles) {
    Image atlas(TILE * ATLAS_COLS, TILE * ATLAS_ROWS, {255, 0, 255, 255});
    for (const auto& [index, img] : tiles) atlas.paste(img, (index % ATLAS_COLS) * TILE, (index / ATLAS_COLS) * TILE);
    return atlas;
}

std::map<std::string, Image> make_hud_icons() {
    Image heart_full = sprite(HEART, {{'k', {40, 10, 10, 255}}, {'r', {220, 30, 30, 255}}, {'w', {255, 200, 200, 255}}});
    Image heart_empty = sprite(HEART, {{'k', {40, 10, 10, 255}}, {'r', {60, 30, 30, 200}}, {'w', {60, 30, 30, 200}}});
    Image food_full = sprite(DRUMSTICK, {{'k', {50, 25, 10, 255}}, {'b', {170, 100, 40, 255}}, {'w', {240, 230, 220, 255}}});
    Image food_empty = sprite(DRUMSTICK, {{'k', {50, 25, 10, 255}}, {'b', {60, 40, 30, 200}}, {'w', {60, 40, 30, 200}}});
    return {
        {"heart_full", heart_full}, {"heart_half", halve(heart_full, heart_empty)}, {"heart_empty", heart_empty},
        {"food_full", food_full}, {"food_half", halve(food_full, food_empty)}, {"food_empty", food_empty},
    };
}
