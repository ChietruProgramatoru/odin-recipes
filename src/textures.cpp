#include "textures.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "biome.h"
#include "blocks.h"
#include "icons.h"
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

const Rgb DIRT_BROWN{134, 96, 67};

// Alpha 254 marks a pixel as grey grass or leaves, coloured by the biome when drawn (see renderer.cpp).
constexpr uint8_t TINTED = 254;
const Rgb GREY{215, 215, 215};

Image grass_top(Rng& rng) { return noise_tile(rng, GREY, 0.14, TINTED); }

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
        for (int y = 0; y < depth; ++y) img.at(x, y) = shade(GREY, 1 + rng.uniform(-0.12, 0.12), TINTED);
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

const Rgb LEAF_GREY{185, 185, 185};

// Fancy leaves: see-through gaps between the leaves.
Image leaves(Rng& rng) {
    Image img = noise_tile(rng, LEAF_GREY, 0.22, TINTED);
    for (int i = 0; i < 26; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        img.at(x, y) = shade(LEAF_GREY, 0.5, TINTED);
    }
    for (int i = 0; i < 40; ++i) img.at(rng.below(TILE), rng.below(TILE)).a = 0;
    return img;
}

// Fast leaves: solid, with dark gaps instead of holes.
Image leaves_fast(Rng& rng) {
    Image img = noise_tile(rng, LEAF_GREY, 0.22, TINTED);
    for (int i = 0; i < 50; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        img.at(x, y) = shade(LEAF_GREY, 0.3, TINTED);
    }
    return img;
}

// Spruce needles: short diagonal strokes, see-through (fancy) or with dark gaps (fast).
Image spruce_leaves(Rng& rng, bool fancy) {
    Image img = noise_tile(rng, LEAF_GREY, 0.15, TINTED);
    for (int i = 0; i < 18; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        for (int k = 0; k < 3; ++k) img.at((x + k) % TILE, (y + k) % TILE) = shade(LEAF_GREY, 0.55, TINTED);
    }
    for (int i = 0; i < 36; ++i) {
        Rgba& p = img.at(rng.below(TILE), rng.below(TILE));
        if (fancy) p.a = 0;
        else p = shade(LEAF_GREY, 0.3, TINTED);
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

Image gravel(Rng& rng) {
    Image img = noise_tile(rng, {130, 124, 122}, 0.1);
    for (int i = 0; i < 22; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        double f = rng.choice({0.6, 0.75, 1.2, 1.35});
        Rgb c = rng.below(3) == 0 ? Rgb{150, 120, 110} : Rgb{130, 124, 122};
        for (int k = 0; k < 4; ++k) img.at((x + (k & 1)) % TILE, (y + (k >> 1)) % TILE) = shade(c, f * rng.uniform(0.95, 1.05));
    }
    return img;
}

// Stone with clusters of coloured specks.
std::function<Image(Rng&)> ore(Rgb color) {
    return [=](Rng& rng) {
        Image img = stone(rng);
        for (int i = 0; i < 5; ++i) {
            int x = rng.randint(1, TILE - 3), y = rng.randint(1, TILE - 3);
            for (int k = 0; k < 5; ++k)
                img.at(x + rng.below(3), y + rng.below(3)) = shade(color, rng.uniform(0.8, 1.15));
        }
        return img;
    };
}

Image mossy_cobble(Rng& rng) {
    Image img = cobble(rng);
    for (int i = 0; i < 7; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE), r = rng.randint(1, 2);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx * dx + dy * dy <= r * r + 1)
                    img.at((x + dx + TILE) % TILE, (y + dy + TILE) % TILE) = shade({70, 110, 50}, rng.uniform(0.8, 1.15));
    }
    return img;
}

// A dark iron cage you can see through.
Image spawner(Rng& rng) {
    Image img(TILE, TILE, {0, 0, 0, 0});
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            bool frame = x == 0 || y == 0 || x == TILE - 1 || y == TILE - 1;
            bool bar = x % 5 == 0 || y % 5 == 0;
            if (frame || bar) img.at(x, y) = shade({52, 60, 72}, frame ? 1.0 : rng.uniform(0.7, 1.0));
        }
    return img;
}

Image lava(Rng& rng) {
    Image img = noise_tile(rng, {207, 92, 20}, 0.12);
    for (int i = 0; i < 12; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        for (int k = 0; k < 3; ++k) img.at((x + k) % TILE, y) = shade({252, 190, 60}, rng.uniform(0.9, 1.05));
    }
    return img;
}

Image obsidian(Rng& rng) {
    Image img = noise_tile(rng, {20, 18, 30}, 0.25);
    for (int i = 0; i < 10; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        for (int k = 0; k < 2; ++k) img.at((x + k) % TILE, y) = shade({70, 45, 105}, rng.uniform(0.85, 1.1));
    }
    return img;
}

Image ice(Rng& rng) {
    Image img = noise_tile(rng, {165, 195, 250}, 0.04, 165);
    for (int i = 0; i < 4; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE);
        for (int k = 0; k < 4; ++k) img.at((x + k) % TILE, (y + k) % TILE) = shade({220, 235, 255}, 1.0, 190);
    }
    return img;
}

Image birch_log(Rng& rng) {
    Image img = noise_tile(rng, {216, 215, 208}, 0.04);
    for (int i = 0; i < 10; ++i) {
        int x = rng.below(TILE), y = rng.below(TILE), n = rng.randint(2, 4);
        for (int k = 0; k < n; ++k) img.at((x + k) % TILE, y) = shade({40, 40, 36}, rng.uniform(0.9, 1.3));
    }
    return img;
}

Image spruce_log(Rng& rng) {
    Image img(TILE, TILE);
    double col_f[TILE];
    for (double& f : col_f) f = rng.uniform(0.75, 1.15);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade({62, 44, 26}, col_f[x] * rng.uniform(0.92, 1.08));
    return img;
}

// A grey grass/leaves tile coloured in like the world would, for icons.
std::function<Image(Rng&)> coloured(std::function<Image(Rng&)> grey, Rgb8 tint) {
    return [=](Rng& rng) {
        Image img = grey(rng);
        for (int y = 0; y < img.height; ++y)
            for (int x = 0; x < img.width; ++x) {
                Rgba& p = img.at(x, y);
                if (p.a != TINTED) continue;
                p = {static_cast<uint8_t>(p.r * tint.r / 255), static_cast<uint8_t>(p.g * tint.g / 255),
                     static_cast<uint8_t>(p.b * tint.b / 255), 255};
            }
        return img;
    };
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

// Creeper skin: green in blotches of lighter and darker green, like Beta's.
Image creeper_skin(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const double r = rng.random();
            const Rgb c = r < 0.18 ? Rgb{150, 215, 140} : r < 0.36 ? Rgb{40, 120, 40} : Rgb{85, 175, 75};
            img.at(x, y) = shade(c, rng.uniform(0.92, 1.08));
        }
    return img;
}

// Spider: dark, hairy brown-grey with lighter flecks.
Image spider_skin(Rng& rng) {
    Image img = noise_tile(rng, {52, 45, 40}, 0.18);
    for (int i = 0; i < 18; ++i) img.at(rng.below(TILE), rng.below(TILE)) = shade({95, 84, 74}, rng.uniform(0.9, 1.1));
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

// Fire: tongues of flame rising from the bottom, yellow at the base and red at the tips,
// see-through between them.
Image fire(Rng& rng) {
    Image img(TILE, TILE);
    for (int x = 0; x < TILE; ++x) {
        int height = static_cast<int>(rng.uniform(6, 15) * (1.0 - std::abs(x - 7.5) / 20.0));
        for (int k = 0; k < height; ++k) {
            int y = TILE - 1 - k;
            double t = static_cast<double>(k) / height;  // 0 at the base, 1 at the tip
            Rgb c = t < 0.35 ? Rgb{255, 230, 90} : t < 0.7 ? Rgb{250, 150, 30} : Rgb{215, 60, 20};
            img.at(x, y) = shade(c, rng.uniform(0.9, 1.1));
        }
    }
    return img;
}

// Armor worn on the player: a plain colour with a darker rim, chain mail with see-through links.
std::function<Image(Rng&)> armor_tile(Rgb c, bool chain = false) {
    return [=](Rng& rng) {
        Image img(TILE, TILE);
        for (int y = 0; y < TILE; ++y)
            for (int x = 0; x < TILE; ++x) {
                if (chain && (x + y) % 3 == 0) continue;
                bool rim = x == 0 || y == 0 || x == TILE - 1 || y == TILE - 1;
                img.at(x, y) = shade(c, (rim ? 0.7 : 1.0) * rng.uniform(0.92, 1.08));
            }
        return img;
    };
}

// --- Beds ------------------------------------------------------------------------
// Red blanket, white pillow and a wooden frame. Tops point their head end up (row 0). Sides and
// ends only use rows 7-15 (the bed is 9/16 tall): blanket, then frame, then legs.

const Rgb BLANKET{158, 28, 28};
const Rgb PILLOW{232, 232, 232};
const Rgb FRAME{130, 96, 56};

Image bed_top(Rng& rng, bool head) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double f = rng.uniform(0.93, 1.07);
            bool edge = x == 0 || x == TILE - 1;
            Rgb c = BLANKET;
            if (head && y >= 2 && y <= 6 && x >= 2 && x <= 13) c = PILLOW;      // pillow
            if (head && y < 2) c = FRAME;                                     // headboard
            if (!head && y == 0) f *= 0.8;                                    // fold of the blanket
            if (!head && y == TILE - 1) f *= 0.75;                            // foot end
            img.at(x, y) = shade(c, edge ? f * 0.8 : f);
        }
    return img;
}

// The long side: `leg_left` puts the leg at the left end (the foot half seen from the +x side).
Image bed_side(Rng& rng, bool head, bool leg_left) {
    Image img(TILE, TILE);
    for (int y = 7; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double f = rng.uniform(0.93, 1.07);
            if (y <= 9) {
                bool pillow = head && y == 7 && x >= 9 && x <= 13;
                img.at(x, y) = shade(pillow ? PILLOW : BLANKET, y == 9 ? f * 0.8 : f);
            } else if (y <= 12) {
                img.at(x, y) = shade(FRAME, y == 12 ? f * 0.75 : f);
            } else {
                bool leg = leg_left ? x <= 2 : x >= TILE - 3;
                if (leg) img.at(x, y) = shade(FRAME, f * 0.7);
            }
        }
    return img;
}

// The foot end (blanket over the frame) or the head end (headboard), with a leg at each corner.
Image bed_end(Rng& rng, bool head) {
    Image img(TILE, TILE);
    for (int y = 7; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double f = rng.uniform(0.93, 1.07);
            if (y <= 12) {
                bool blanket = !head && y <= 9;
                img.at(x, y) = shade(blanket ? BLANKET : FRAME, y == 12 || y == 9 ? f * 0.8 : f);
            } else if (x <= 2 || x >= TILE - 3) {
                img.at(x, y) = shade(FRAME, f * 0.7);
            }
        }
    return img;
}

using Palette = std::vector<std::pair<char, Rgba>>;
Image sprite(const std::vector<const char*>& rows, const Palette& palette);

// --- Farming ---------------------------------------------------------------------
// Plants are drawn as crossed or # shaped sheets (see world.cpp), so their tiles stand on the
// bottom row and are see-through around them.

// Farmland: tilled dirt with furrows across it, darker when wet.
Image farmland(Rng& rng, bool wet) {
    const Rgb base = wet ? Rgb{88, 58, 36} : Rgb{128, 92, 62};
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            double f = rng.uniform(0.9, 1.1);
            if (y % 4 == 0) f *= 0.7;  // the furrow between two ridges
            if (y % 4 == 1) f *= 1.12;
            img.at(x, y) = shade(base, f);
        }
    return img;
}

// Wheat at stage 0-7: stalks that grow taller and turn from green to gold, with ears at stage 7.
Image wheat(Rng& rng, int stage) {
    Image img(TILE, TILE);
    const int height = 2 + stage * 2 - (stage == 7 ? 1 : 0);
    const double ripe = stage / 7.0;
    const Rgb green{70, 150, 40}, gold{200, 175, 70};
    auto mix = [&](double t) {
        return Rgb{green.r + (gold.r - green.r) * t, green.g + (gold.g - green.g) * t, green.b + (gold.b - green.b) * t};
    };
    for (int x = 1; x < TILE; x += 3) {
        int h = std::max(1, height - rng.below(3));
        int lean = rng.below(3) - 1;
        for (int k = 0; k < h; ++k) {
            int px = std::clamp(x + (k > h / 2 ? lean : 0), 0, TILE - 1);
            img.at(px, TILE - 1 - k) = shade(mix(ripe * (0.6 + 0.4 * k / std::max(1, h))), rng.uniform(0.85, 1.1));
        }
        if (stage == 7)  // the ear: a fat golden head on top of each stalk
            for (int k = h - 4; k < h; ++k)
                for (int dx = 0; dx <= 1; ++dx) {
                    int px = std::clamp(x + lean + dx, 0, TILE - 1);
                    if (k >= 0) img.at(px, TILE - 1 - k) = shade({215, 185, 80}, rng.uniform(0.8, 1.1));
                }
    }
    return img;
}

// Sugar cane: three pale green stalks with darker joints, from the bottom to the top.
Image sugar_cane(Rng& rng) {
    Image img(TILE, TILE);
    for (int x : {3, 7, 12})
        for (int y = 0; y < TILE; ++y)
            for (int dx = 0; dx <= 1; ++dx) {
                bool joint = (y + x) % 5 == 0;
                img.at(x + dx, y) = shade(joint ? Rgb{110, 160, 70} : Rgb{150, 205, 105}, (dx ? 0.85 : 1.0) * rng.uniform(0.93, 1.07));
            }
    return img;
}

// Tall grass and ferns are grey and coloured by the biome, like grass.
Image tall_grass(Rng& rng) {
    Image img(TILE, TILE);
    for (int x = 0; x < TILE; ++x) {
        if (rng.below(3) == 0) continue;
        int h = rng.below(8) + 5 + (x > 3 && x < 12 ? 3 : 0);
        for (int k = 0; k < h && k < TILE; ++k) img.at(x, TILE - 1 - k) = shade(GREY, rng.uniform(0.75, 1.1), TINTED);
    }
    return img;
}

Image fern(Rng& rng) {
    Image img(TILE, TILE);
    for (int side : {-1, 1})
        for (int k = 0; k < 12; ++k) {
            int y = TILE - 1 - k, x = 7 + (side > 0 ? 1 : 0) + side * (k / 3);
            img.at(x, y) = shade(GREY, 0.8, TINTED);
            for (int f = 1; f <= 2 && k % 2 == 0; ++f)  // the leaflets
                if (x + side * f >= 0 && x + side * f < TILE) img.at(x + side * f, y - 1 + f / 2) = shade(GREY, rng.uniform(0.85, 1.1), TINTED);
        }
    return img;
}

// A flower or mushroom: a stem from the bottom row with a head of `rows`, drawn from the top.
Image small_plant(const std::vector<const char*>& rows, const Palette& palette) {
    Image img(TILE, TILE);
    Image spr = sprite(rows, palette);
    img.paste(spr, (TILE - spr.width) / 2, TILE - spr.height);
    return img;
}

// h head, c its spots or centre, s stem.
const std::vector<const char*> FLOWER_ROWS = {
    ".hhh.", "hhchh", "hcccc", "hhchh", ".hhh.", "..s..", ".ss..", "..s..", "..ss.", "..s..",
};
const std::vector<const char*> MUSHROOM_ROWS = {
    ".hhhh.", "hhchch", "hhhhhh", "..ss..", "..ss..", "..ss..",
};

// A sapling: a thin trunk from the bottom row with a ragged clump of leaves on it.
Image sapling(Rng& rng, Rgb leaf, Rgb trunk) {
    Image img(TILE, TILE);
    for (int y = 9; y < TILE; ++y) img.at(7 + (y < 12 ? 1 : 0), y) = shade(trunk, rng.uniform(0.85, 1.05));
    for (int y = 1; y < 11; ++y) {
        const int half = 5 - std::abs(y - 5) / 2 - (y > 8 ? 2 : 0);
        for (int x = 8 - half; x < 8 + half; ++x)
            if (rng.below(5) != 0) img.at(x, y) = shade(leaf, rng.uniform(0.7, 1.15));
    }
    return img;
}

// Cactus: dark green ribs with pale spines (sides), a rounded top with a lighter centre.
Image cactus_side(Rng& rng) {
    Image img = noise_tile(rng, {16, 112, 32}, 0.06);
    for (int x : {1, 4, 7, 10, 13})
        for (int y = 0; y < TILE; ++y) img.at(x, y) = shade({10, 80, 22}, rng.uniform(0.9, 1.05));
    for (int i = 0; i < 12; ++i) img.at(2 + 3 * rng.below(4), rng.below(TILE)) = shade({220, 230, 190}, 1.0);
    return img;
}

Image cactus_top(Rng& rng, bool top) {
    Image img = noise_tile(rng, {16, 112, 32}, 0.05);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const double d = std::hypot(x - 7.5, y - 7.5);
            if (d < 3.5 && top) img.at(x, y) = shade({70, 150, 60}, rng.uniform(0.9, 1.05));
            else if (d > 6.5 && d < 7.5) img.at(x, y) = shade({10, 80, 22}, 1.0);
        }
    return img;
}

// Cobweb: threads from the middle out to the edges, and rings across them.
Image cobweb(Rng&) {
    Image img(TILE, TILE);
    const Rgba thread{235, 235, 240, 220};
    for (int k = 0; k < TILE; ++k) {
        img.at(k, k) = thread;
        img.at(TILE - 1 - k, k) = thread;
        img.at(k, 7) = thread;
        img.at(8, k) = thread;
    }
    for (int r : {3, 6})
        for (int k = -r; k <= r; ++k)
            for (auto [x, y] : {std::pair{8 + k, 7 - r}, std::pair{8 + k, 7 + r}, std::pair{8 - r, 7 + k}, std::pair{8 + r, 7 + k}})
                if (x >= 0 && x < TILE && y >= 0 && y < TILE && (k + r) % 2 == 0) img.at(x, y) = thread;
    return img;
}

// The side of grass with snow on it: dirt with a white top (not coloured by the biome).
Image grass_snow_side(Rng& rng) {
    Image img = dirt(rng);
    for (int x = 0; x < TILE; ++x) {
        int depth = 3 + rng.choice({0, 0, 1, 1, 2});
        for (int y = 0; y < depth; ++y) img.at(x, y) = shade({240, 248, 250}, rng.uniform(0.95, 1.02));
    }
    return img;
}

// Cake: white icing on top with red dots, a sponge side with icing dripping over its top.
Image cake_top(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 1; y < 15; ++y)
        for (int x = 1; x < 15; ++x) img.at(x, y) = shade({245, 245, 240}, rng.uniform(0.95, 1.03));
    for (int i = 0; i < 6; ++i) img.at(2 + rng.below(12), 2 + rng.below(12)) = shade({220, 40, 40}, 1);
    return img;
}

Image cake_side(Rng& rng, bool inside) {
    Image img(TILE, TILE);
    for (int y = 8; y < TILE; ++y)
        for (int x = 1; x < 15; ++x) {
            bool icing = y <= 9 || (y == 10 && (x * 7) % 3 == 0);
            Rgb c = icing && !inside ? Rgb{245, 245, 240} : inside ? Rgb{215, 170, 110} : Rgb{180, 115, 60};
            if (inside && y <= 8) c = {245, 245, 240};
            img.at(x, y) = shade(c, rng.uniform(0.93, 1.05));
        }
    return img;
}

// --- Transport (M12) ---------------------------------------------------------------
// Rails run from the bottom of the picture to the top: two rails over wooden sleepers. `rail` colours
// the rails, `middle` (if any) the strip between them: a powered rail's red line, a detector's plate.
Image rail_tile(Rng& rng, Rgb rail, std::optional<Rgb> middle) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 1; x < 15; ++x)
            if (y % 4 == 1 || y % 4 == 2) img.at(x, y) = shade({120, 85, 50}, (y % 4 == 2 ? 0.8 : 1.0) * rng.uniform(0.9, 1.1));
    for (int y = 0; y < TILE; ++y) {
        for (int x : {2, 3, 12, 13}) img.at(x, y) = shade(rail, (x == 3 || x == 13 ? 0.75 : 1.0) * rng.uniform(0.95, 1.05));
        if (middle)
            for (int x = 6; x < 10; ++x)
                if (y > 1 && y < 14) img.at(x, y) = shade(*middle, (x == 6 || x == 9 ? 0.75 : 1.0) * rng.uniform(0.95, 1.05));
    }
    return img;
}

// A curve from the bottom of the picture round to its right side.
Image rail_curve(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const double dx = 16 - (x + 0.5), dy = 16 - (y + 0.5);
            const double r = std::hypot(dx, dy), a = std::atan2(dy, dx) * 180 / 3.14159265358979;
            const int sleeper = static_cast<int>(a / 11.25);
            if (r > 1 && r < 15 && sleeper % 2 == 1) img.at(x, y) = shade({120, 85, 50}, rng.uniform(0.85, 1.1));
            if ((r >= 2 && r < 4) || (r >= 12 && r < 14))
                img.at(x, y) = shade({170, 170, 175}, (r < 3 || (r >= 12 && r < 13) ? 1.0 : 0.75) * rng.uniform(0.95, 1.05));
        }
    return img;
}

std::function<Image(Rng&)> solid(Rgb c, double spread = 0.04) {
    return [=](Rng& rng) { return noise_tile(rng, c, spread); };
}

// --- Pixel-art sprites --------------------------------------------------------


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
const std::vector<const char*> BUBBLE = {
    "..kkkk...",
    ".kbbbbk..",
    "kbwbbbbk.",
    "kbwbbbbk.",
    "kbbbbbbk.",
    "kbbbbbbk.",
    ".kbbbbk..",
    "..kkkk...",
};
const std::vector<const char*> CHESTPLATE = {
    "kkk...kkk",
    "kwwk.kwwk",
    "kwgkkkggk",
    ".kgggggk.",
    ".kgggggk.",
    ".kgggggk.",
    ".kgggggk.",
    ".kkkkkkk.",
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
    std::map<int, std::function<Image(Rng&)>> generators = {
        {T_GRASS_TOP, grass_top}, {T_GRASS_SIDE, grass_side}, {T_DIRT, dirt}, {T_STONE, stone},
        {T_COBBLE, cobble}, {T_SAND, sand}, {T_LOG_SIDE, log_side}, {T_LOG_TOP, log_top},
        {T_LEAVES, leaves}, {T_PLANKS, planks}, {T_TORCH, torch}, {T_BEDROCK, bedrock}, {T_WATER, water}, {T_BRICK, brick},
        {T_COW_HIDE, cow_hide}, {T_WHITE, solid({255, 255, 255}, 0)}, {T_HOOF, solid({70, 48, 35})},
        {T_COW_HEAD, solid({85, 60, 45})}, {T_SNOUT, solid({225, 165, 160})}, {T_BLACK, solid({16, 16, 16}, 0)},
        {T_HORN, solid({235, 230, 215})}, {T_LEAVES_FAST, leaves_fast},
        {T_SKIN, solid({222, 170, 130})}, {T_HAIR, solid({84, 52, 28}, 0.1)}, {T_SHIRT, solid({196, 72, 48}, 0.08)},
        {T_PANTS, solid({52, 62, 112}, 0.08)}, {T_SHOE, solid({64, 64, 64})}, {T_EYE, solid({240, 240, 240}, 0)},
        {T_APPLE, padded_sprite(APPLE_SPRITE, {{'g', {70, 110, 30, 255}}, {'r', {210, 30, 30, 255}},
                                               {'w', {255, 180, 180, 255}}})},
        {T_BEEF, padded_sprite(BEEF_SPRITE, {{'k', {90, 20, 20, 255}}, {'r', {215, 70, 70, 255}},
                                             {'p', {240, 150, 150, 255}}, {'w', {245, 225, 225, 255}}})},
        {T_GRAVEL, gravel}, {T_COAL_ORE, ore({35, 35, 35})}, {T_IRON_ORE, ore({216, 175, 147})},
        {T_GOLD_ORE, ore({250, 236, 78})}, {T_DIAMOND_ORE, ore({100, 232, 240})}, {T_REDSTONE_ORE, ore({200, 10, 10})},
        {T_LAPIS_ORE, ore({30, 64, 175})}, {T_MOSSY_COBBLE, mossy_cobble}, {T_SPAWNER, spawner}, {T_LAVA, lava},
        {T_ICE, ice}, {T_BIRCH_LOG_SIDE, birch_log}, {T_SPRUCE_LOG_SIDE, spruce_log},
        {T_SPRUCE_LEAVES, [](Rng& rng) { return spruce_leaves(rng, true); }},
        {T_SPRUCE_LEAVES_FAST, [](Rng& rng) { return spruce_leaves(rng, false); }},
        {T_GRASS_TOP_ICON, coloured(grass_top, grass_color(ICON_TEMPERATURE, ICON_RAINFALL))},
        {T_GRASS_ICON, coloured(grass_side, grass_color(ICON_TEMPERATURE, ICON_RAINFALL))},
        {T_LEAVES_ICON, coloured(leaves, foliage_color(ICON_TEMPERATURE, ICON_RAINFALL))},
        {T_BIRCH_LEAVES_ICON, coloured(leaves, BIRCH_COLOR)},
        {T_SPRUCE_LEAVES_ICON, coloured([](Rng& rng) { return spruce_leaves(rng, true); }, SPRUCE_COLOR)},
        {T_FIRE, fire}, {T_ARMOR_LEATHER, armor_tile({160, 101, 64})}, {T_ARMOR_CHAIN, armor_tile({150, 150, 155}, true)},
        {T_ARMOR_IRON, armor_tile({215, 215, 215})}, {T_ARMOR_GOLD, armor_tile({245, 205, 50})},
        {T_ARMOR_DIAMOND, armor_tile({90, 220, 210})},
        {T_ZOMBIE_SKIN, solid({92, 140, 76}, 0.1)}, {T_ZOMBIE_SHIRT, solid({50, 150, 160}, 0.1)},
        {T_ZOMBIE_PANTS, solid({70, 60, 150}, 0.1)}, {T_BONE, solid({200, 200, 195}, 0.08)}, {T_CREEPER, creeper_skin},
        {T_SPIDER, spider_skin}, {T_SPIDER_EYE, solid({205, 20, 20}, 0)}, {T_SLIME, solid({110, 195, 95}, 0.12)},
        {T_SMOKE, solid({210, 210, 210}, 0.1)}, {T_OBSIDIAN, obsidian},
        {T_BED_TOP_FOOT, [](Rng& rng) { return bed_top(rng, false); }},
        {T_BED_TOP_HEAD, [](Rng& rng) { return bed_top(rng, true); }},
        {T_BED_SIDE_FOOT, [](Rng& rng) { return bed_side(rng, false, true); }},
        {T_BED_SIDE_HEAD, [](Rng& rng) { return bed_side(rng, true, false); }},
        {T_BED_END_FOOT, [](Rng& rng) { return bed_end(rng, false); }},
        {T_BED_END_HEAD, [](Rng& rng) { return bed_end(rng, true); }},
        {T_FARMLAND_DRY, [](Rng& rng) { return farmland(rng, false); }},
        {T_FARMLAND_WET, [](Rng& rng) { return farmland(rng, true); }},
        {T_SUGAR_CANE, sugar_cane}, {T_TALL_GRASS, tall_grass}, {T_FERN, fern},
        {T_DANDELION, [](Rng&) { return small_plant(FLOWER_ROWS, {{'h', {245, 220, 40, 255}}, {'c', {200, 160, 20, 255}},
                                                                  {'s', {70, 140, 40, 255}}}); }},
        {T_ROSE, [](Rng&) { return small_plant(FLOWER_ROWS, {{'h', {205, 30, 30, 255}}, {'c', {150, 15, 15, 255}},
                                                             {'s', {70, 140, 40, 255}}}); }},
        {T_BROWN_MUSHROOM, [](Rng&) { return small_plant(MUSHROOM_ROWS, {{'h', {150, 110, 80, 255}}, {'c', {120, 85, 60, 255}},
                                                                         {'s', {225, 215, 195, 255}}}); }},
        {T_RED_MUSHROOM, [](Rng&) { return small_plant(MUSHROOM_ROWS, {{'h', {210, 40, 40, 255}}, {'c', {245, 245, 245, 255}},
                                                                       {'s', {225, 215, 195, 255}}}); }},
        {T_CAKE_TOP, cake_top}, {T_CAKE_SIDE, [](Rng& rng) { return cake_side(rng, false); }},
        {T_CAKE_INSIDE, [](Rng& rng) { return cake_side(rng, true); }},
        {T_PIG, solid({240, 165, 160}, 0.06)}, {T_PIG_SNOUT, solid({220, 130, 130}, 0.04)},
        {T_SADDLE, solid({125, 75, 40}, 0.1)}, {T_SHEEP_SKIN, solid({225, 195, 170}, 0.06)},
        {T_SHEEP_FACE, solid({205, 175, 150}, 0.06)}, {T_BEAK, solid({235, 175, 40}, 0.04)},
        {T_WATTLE, solid({200, 30, 30}, 0.04)}, {T_SQUID, solid({40, 60, 95}, 0.12)},
        {T_WOLF, solid({205, 200, 195}, 0.14)}, {T_WOLF_DARK, solid({140, 130, 120}, 0.12)},
        {T_RAIL, [](Rng& rng) { return rail_tile(rng, {170, 170, 175}, std::nullopt); }},
        {T_RAIL_CURVE, rail_curve},
        {T_POWERED_RAIL, [](Rng& rng) { return rail_tile(rng, {235, 200, 60}, Rgb{90, 20, 20}); }},
        {T_POWERED_RAIL_ON, [](Rng& rng) { return rail_tile(rng, {235, 200, 60}, Rgb{240, 40, 30}); }},
        {T_DETECTOR_RAIL, [](Rng& rng) { return rail_tile(rng, {170, 170, 175}, Rgb{140, 120, 110}); }},
        {T_CART, solid({105, 105, 112}, 0.12)},
        {T_SAPLING, [](Rng& rng) { return sapling(rng, {72, 140, 40}, {105, 80, 45}); }},
        {T_SPRUCE_SAPLING, [](Rng& rng) { return sapling(rng, {45, 85, 50}, {70, 50, 30}); }},
        {T_BIRCH_SAPLING, [](Rng& rng) { return sapling(rng, {120, 165, 75}, {215, 215, 205}); }},
        {T_CACTUS_SIDE, cactus_side}, {T_CACTUS_TOP, [](Rng& rng) { return cactus_top(rng, true); }},
        {T_CACTUS_BOTTOM, [](Rng& rng) { return cactus_top(rng, false); }}, {T_COBWEB, cobweb},
        {T_GRASS_SNOW_SIDE, grass_snow_side},
    };
    std::map<int, Image> tiles;
    for (int k = 0; k < 8; ++k) generators.emplace(T_WHEAT_0 + k, [k](Rng& rng) { return wheat(rng, k); });
    for (const auto& [index, gen] : generators) {
        Rng rng(static_cast<uint64_t>(seed) * 100 + index);
        tiles[index] = gen(rng);
    }
    tiles.merge(make_item_tiles());
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
    Image armor_full = sprite(CHESTPLATE, {{'k', {30, 30, 30, 255}}, {'g', {200, 200, 210, 255}}, {'w', {250, 250, 255, 255}}});
    Image armor_empty = sprite(CHESTPLATE, {{'k', {30, 30, 30, 255}}, {'g', {50, 50, 55, 200}}, {'w', {50, 50, 55, 200}}});
    return {
        {"armor_full", armor_full}, {"armor_half", halve(armor_full, armor_empty)}, {"armor_empty", armor_empty},
        {"heart_full", heart_full}, {"heart_half", halve(heart_full, heart_empty)}, {"heart_empty", heart_empty},
        {"food_full", food_full}, {"food_half", halve(food_full, food_empty)}, {"food_empty", food_empty},
        {"bubble", sprite(BUBBLE, {{'k', {30, 60, 140, 255}}, {'b', {90, 150, 240, 230}}, {'w', {240, 250, 255, 255}}})},
    };
}

Image make_rain_image() {
    // Thin streaks scattered so that the picture repeats up and down and sideways without a seam.
    // One picture covers a column 1 block wide and 4 blocks tall (64 pixels a block, like Beta).
    Image img(64, 256);
    Rng rng(4242);
    for (int i = 0; i < 48; ++i) {
        int x = rng.below(64), y = rng.below(256), len = rng.randint(14, 30);
        for (int k = 0; k < len; ++k) img.at(x, (y + k) % 256) = {205, 220, 255, static_cast<uint8_t>(60 + 120 * k / len)};
    }
    return img;
}

Image make_snow_image() {
    // Flakes of 2x2 pixels, on a picture 1 block wide and 2 blocks tall.
    Image img(64, 128);
    Rng rng(4343);
    for (int i = 0; i < 40; ++i) {
        int x = rng.below(64), y = rng.below(128);
        for (int k = 0; k < 4; ++k) img.at((x + (k & 1)) % 64, (y + (k >> 1)) % 128) = {255, 255, 255, 230};
    }
    return img;
}
