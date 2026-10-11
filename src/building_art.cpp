// Pixel art for the building blocks (M11): doors, the trapdoor, the ladder, Nether blocks, the
// halves of a double chest, the black font for signs and the paintings. Drawn by the code like
// the rest of the textures; the paintings are this game's own pictures.
#include <algorithm>
#include <cmath>
#include <functional>

#include "blocks.h"
#include "font.h"
#include "painting.h"
#include "rng.h"
#include "textures.h"

namespace {

struct Rgb {
    double r, g, b;
};

uint8_t clamp8(double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); }
Rgba shade(Rgb c, double f = 1.0, uint8_t a = 255) { return {clamp8(c.r * f), clamp8(c.g * f), clamp8(c.b * f), a}; }
Rgb mix(Rgb a, Rgb b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

const Rgb WOOD{162, 130, 78}, DARK_WOOD{96, 70, 40}, IRON{200, 200, 205};

Image noise(Rng& rng, Rgb c, double spread) {
    Image img(TILE, TILE);
    for (auto& p : img.pixels) p = shade(c, 1 + rng.uniform(-spread, spread));
    return img;
}

void fill(Image& img, int x0, int y0, int x1, int y1, Rgba c) {
    for (int y = std::max(0, y0); y <= std::min(img.height - 1, y1); ++y)
        for (int x = std::max(0, x0); x <= std::min(img.width - 1, x1); ++x) img.at(x, y) = c;
}

void frame(Image& img, Rgba c, int x0 = 0, int y0 = 0, int x1 = TILE - 1, int y1 = TILE - 1) {
    for (int x = x0; x <= x1; ++x) img.at(x, y0) = img.at(x, y1) = c;
    for (int y = y0; y <= y1; ++y) img.at(x0, y) = img.at(x1, y) = c;
}

// Vertical boards with a dark frame: a door half or a trapdoor. Holes are see-through.
Image wood_door(Rng& rng, bool top) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const bool seam = x % 5 == 4;
            img.at(x, y) = shade(WOOD, seam ? 0.7 : rng.uniform(0.9, 1.05));
        }
    frame(img, shade(DARK_WOOD));
    if (top) {
        // Two little windows.
        fill(img, 3, 3, 6, 7, {0, 0, 0, 0});
        fill(img, 9, 3, 12, 7, {0, 0, 0, 0});
        for (int y = 2; y <= 8; ++y) img.at(2, y) = img.at(7, y) = img.at(8, y) = img.at(13, y) = shade(DARK_WOOD);
        for (int x = 2; x <= 13; ++x) img.at(x, 2) = img.at(x, 8) = shade(DARK_WOOD);
    } else {
        img.at(12, 1) = img.at(12, 2) = shade({60, 60, 60});  // the handle
        img.at(11, 2) = shade({60, 60, 60});
        for (int x = 1; x < 15; ++x) img.at(x, 7) = shade(DARK_WOOD, 0.9);
    }
    return img;
}

Image iron_door(Rng& rng, bool top) {
    Image img = noise(rng, IRON, 0.04);
    frame(img, shade(IRON, 0.6));
    for (int y = 1; y < 15; ++y) img.at(7, y) = img.at(8, y) = shade(IRON, 0.85);
    for (int k : {2, 13})
        for (int r : {2, 13}) img.at(k, r) = shade(IRON, 0.55);  // rivets
    if (top) {
        fill(img, 4, 3, 11, 6, {0, 0, 0, 0});
        frame(img, shade(IRON, 0.6), 3, 2, 12, 7);
    } else {
        img.at(3, 1) = img.at(3, 2) = shade({70, 70, 75});
    }
    return img;
}

Image trapdoor(Rng& rng) {
    Image img = wood_door(rng, false);
    fill(img, 1, 1, 14, 14, {0, 0, 0, 0});
    for (int y = 1; y < 15; ++y)
        for (int x = 1; x < 15; ++x)
            if (!((x >= 3 && x <= 6) || (x >= 9 && x <= 12)) || !((y >= 3 && y <= 6) || (y >= 9 && y <= 12)))
                img.at(x, y) = shade(WOOD, rng.uniform(0.88, 1.04));
    frame(img, shade(DARK_WOOD));
    return img;
}

Image ladder(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y) {
        img.at(1, y) = img.at(2, y) = shade(WOOD, rng.uniform(0.8, 1.0));
        img.at(13, y) = img.at(14, y) = shade(WOOD, rng.uniform(0.8, 1.0));
    }
    for (int r : {1, 5, 9, 13}) {
        for (int x = 3; x <= 12; ++x) img.at(x, r) = shade(WOOD, rng.uniform(0.95, 1.1));
        for (int x = 3; x <= 12; ++x) img.at(x, r + 1) = shade(DARK_WOOD);
    }
    return img;
}

Image netherrack(Rng& rng) {
    Image img = noise(rng, {111, 54, 52}, 0.14);
    for (int i = 0; i < 24; ++i) img.at(rng.below(TILE), rng.below(TILE)) = shade({70, 25, 25});
    for (int i = 0; i < 10; ++i) img.at(rng.below(TILE), rng.below(TILE)) = shade({160, 80, 75});
    return img;
}

Image soul_sand(Rng& rng) {
    Image img = noise(rng, {85, 64, 50}, 0.12);
    // Faint faces in the sand.
    for (int k = 0; k < 2; ++k) {
        const int x = 2 + rng.below(8), y = 2 + rng.below(8);
        const Rgba d = shade({50, 36, 28});
        img.at(x, y) = img.at(x + 3, y) = d;
        img.at(x + 1, y + 3) = img.at(x + 2, y + 3) = d;
    }
    return img;
}

// One half of a double chest's front or back (the picture is 32 pixels wide across both).
Image chest_half(Rng& rng, bool front, bool right) {
    Image img = noise(rng, {165, 112, 45}, 0.07);
    const Rgba edge = shade({80, 50, 20});
    for (int x = 0; x < TILE; ++x) img.at(x, 0) = img.at(x, TILE - 1) = img.at(x, 5) = edge;
    for (int y = 0; y < TILE; ++y) img.at(right ? TILE - 1 : 0, y) = edge;
    if (front) fill(img, right ? 0 : 15, 4, right ? 0 : 15, 7, {200, 200, 200, 255});  // the latch, across the middle
    return img;
}

Image painting_back(Rng& rng) {
    Image img = noise(rng, {120, 95, 60}, 0.06);
    frame(img, shade({80, 60, 35}));
    return img;
}

// --- Paintings ------------------------------------------------------------------------------

struct Canvas {
    Image img;
    Rng rng;
    Canvas(int w, int h, uint64_t seed) : img(w, h, {0, 0, 0, 255}), rng(seed) {}
    int w() const { return img.width; }
    int h() const { return img.height; }
    void px(int x, int y, Rgb c) {
        if (x >= 0 && y >= 0 && x < w() && y < h()) img.at(x, y) = shade(c);
    }
    // Sky from `top` at y 0 to `bottom` at y `to`.
    void sky(Rgb top, Rgb bottom, int to) {
        for (int y = 0; y < h(); ++y)
            for (int x = 0; x < w(); ++x) px(x, y, mix(top, bottom, std::min(1.0, y / std::max(1.0, to * 1.0))));
    }
    void disc(double cx, double cy, double r, Rgb c) {
        for (int y = 0; y < h(); ++y)
            for (int x = 0; x < w(); ++x)
                if ((x + 0.5 - cx) * (x + 0.5 - cx) + (y + 0.5 - cy) * (y + 0.5 - cy) <= r * r) px(x, y, c);
    }
    // Land below a wavy line around `base`.
    void ridge(double base, double amp, double freq, double phase, Rgb c, double jag = 0) {
        for (int x = 0; x < w(); ++x) {
            double top = base - amp * std::sin(x * freq + phase) - amp * 0.4 * std::sin(x * freq * 2.7 + phase * 1.7) -
                         jag * rng.random();
            for (int y = std::max(0, static_cast<int>(top)); y < h(); ++y)
                px(x, y, mix(c, {0, 0, 0}, std::min(0.4, (y - top) / (h() * 3.0))));
        }
    }
    void water(int from, Rgb c) {
        for (int y = from; y < h(); ++y)
            for (int x = 0; x < w(); ++x) px(x, y, ((x + y * 3) % 7 == 0) ? mix(c, {255, 255, 255}, 0.35) : c);
    }
    void pine(int x, int base, int height, Rgb c) {
        for (int k = 0; k < height; ++k) {
            const int half = (height - k) / 3 + ((k % 3) == 0 ? 1 : 0);
            for (int dx = -half; dx <= half; ++dx) px(x + dx, base - k, c);
        }
        px(x, base + 1, {90, 60, 30});
    }
    void rect(int x0, int y0, int x1, int y1, Rgb c) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) px(x, y, c);
    }
    void stars(int n, int below) {
        for (int i = 0; i < n; ++i) px(rng.below(w()), rng.below(below), {255, 255, rng.random() < 0.3 ? 200.0 : 255.0});
    }
    void frame_all() {
        for (int x = 0; x < w(); ++x) px(x, 0, {70, 50, 30}), px(x, h() - 1, {70, 50, 30});
        for (int y = 0; y < h(); ++y) px(0, y, {70, 50, 30}), px(w() - 1, y, {70, 50, 30});
    }
};

Image paint(int motif, const PaintingMotif& m) {
    Canvas c(m.w * TILE, m.h * TILE, 9000 + motif);
    const int W = c.w(), H = c.h();
    switch (motif) {
        case 0:  // Sunrise
            c.sky({250, 120, 60}, {255, 220, 130}, H * 2 / 3);
            c.disc(W / 2.0, H * 0.68, 4.5, {255, 245, 180});
            c.ridge(H * 0.72, 1.5, 0.6, 0.3, {60, 40, 50});
            break;
        case 1:  // Pine
            c.sky({110, 160, 230}, {200, 225, 250}, H);
            c.ridge(H * 0.82, 0.7, 0.4, 1.0, {80, 150, 60});
            c.pine(W / 2, H - 4, 11, {40, 100, 50});
            break;
        case 2:  // Pond
            c.rect(0, 0, W - 1, H - 1, {90, 150, 60});
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if ((x - 8) * (x - 8) / 30.0 + (y - 9) * (y - 9) / 14.0 < 1) c.px(x, y, {60, 110, 200});
            c.px(3, 4, {40, 90, 30}), c.px(3, 5, {40, 90, 30}), c.px(12, 3, {40, 90, 30}), c.px(12, 4, {40, 90, 30});
            break;
        case 3:  // Mushroom
            c.sky({30, 25, 40}, {60, 45, 50}, H);
            c.disc(8, 8, 5.5, {200, 40, 40});
            c.rect(0, 9, W - 1, H - 1, {30, 25, 40});
            c.rect(6, 8, 9, 13, {230, 220, 200});
            c.px(6, 5, {250, 250, 250}), c.px(10, 4, {250, 250, 250}), c.px(8, 6, {250, 250, 250});
            c.rect(0, 14, W - 1, H - 1, {70, 55, 40});
            break;
        case 4:  // Hills
            c.sky({120, 170, 235}, {210, 230, 250}, H);
            c.rect(5, 3, 10, 4, {250, 250, 250}), c.rect(20, 2, 26, 3, {250, 250, 250});
            c.ridge(H * 0.6, 2, 0.25, 0.5, {110, 170, 80});
            c.ridge(H * 0.8, 2, 0.35, 2.0, {70, 130, 50});
            break;
        case 5:  // Sea
            c.sky({100, 150, 220}, {190, 215, 245}, H / 2);
            c.water(H / 2, {40, 90, 170});
            c.rect(14, 5, 14, 9, {90, 60, 30});
            for (int k = 0; k < 4; ++k) c.rect(15, 5 + k, 15 + k, 5 + k, {245, 245, 240});
            c.rect(11, 10, 18, 10, {110, 70, 35});
            break;
        case 6:  // Tower
            c.sky({90, 130, 200}, {200, 210, 235}, H);
            c.rect(0, H - 6, W - 1, H - 1, {80, 140, 60});
            c.rect(5, 6, 10, H - 6, {150, 150, 150});
            for (int x = 4; x <= 11; x += 2) c.rect(x, 4, x, 5, {150, 150, 150});
            c.rect(7, 10, 8, 12, {40, 30, 30});
            c.rect(7, H - 9, 8, H - 6, {70, 50, 30});
            break;
        case 7:  // Waterfall
            c.rect(0, 0, W - 1, H - 1, {110, 110, 115});
            for (int i = 0; i < 40; ++i) c.px(c.rng.below(W), c.rng.below(H), {85, 85, 90});
            c.rect(6, 0, 9, H - 6, {80, 140, 230});
            for (int y = 0; y < H - 6; y += 3) c.px(7 + (y / 3) % 2, y, {220, 235, 255});
            c.water(H - 6, {50, 100, 200});
            break;
        case 8:  // Lake
            c.sky({130, 170, 230}, {220, 230, 245}, H / 2);
            c.ridge(H * 0.45, 4, 0.18, 0.2, {120, 120, 140}, 1.0);
            c.ridge(H * 0.52, 1.5, 0.3, 1.2, {60, 120, 60});
            c.water(H * 6 / 10, {60, 110, 190});
            for (int x = 2; x < W; x += 6) c.pine(x, H * 6 / 10 - 1, 6, {35, 85, 45});
            break;
        case 9:  // Cave
            c.rect(0, 0, W - 1, H - 1, {80, 80, 85});
            for (int i = 0; i < 70; ++i) c.px(c.rng.below(W), c.rng.below(H), {60, 60, 65});
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if ((x - 16) * (x - 16) / 120.0 + (y - 20) * (y - 20) / 90.0 < 1) c.px(x, y, {20, 18, 22});
            c.disc(20, 18, 3, {255, 190, 80});
            c.rect(20, 18, 20, 23, {120, 85, 45});
            for (int i = 0; i < 6; ++i) c.px(2 + c.rng.below(6), 2 + c.rng.below(8), {90, 230, 240});
            for (int i = 0; i < 6; ++i) c.px(24 + c.rng.below(6), 3 + c.rng.below(8), {240, 210, 60});
            break;
        case 10:  // Orchard
            c.sky({140, 190, 240}, {220, 235, 250}, H);
            c.rect(0, H * 2 / 3, W - 1, H - 1, {100, 160, 70});
            for (int k = 0; k < 3; ++k) {
                const int x = 5 + k * 11, y = H / 2;
                c.rect(x, y, x + 1, y + 8, {100, 70, 35});
                c.disc(x + 1, y - 2, 5, {50, 120, 50});
                for (int a = 0; a < 4; ++a) c.px(x - 2 + c.rng.below(7), y - 5 + c.rng.below(6), {220, 30, 30});
            }
            break;
        case 11:  // Valley
            c.sky({120, 165, 230}, {230, 225, 210}, H);
            c.disc(W * 0.8, H * 0.25, 3.5, {255, 240, 170});
            c.ridge(H * 0.45, 5, 0.12, 0.0, {100, 110, 130}, 1.0);
            c.ridge(H * 0.62, 3, 0.2, 1.5, {90, 150, 70});
            for (int x = 0; x < W; ++x) {
                const int y = H - 1 - static_cast<int>(std::abs(std::sin(x * 0.08)) * 9);
                c.rect(x, y, x, std::min(H - 1, y + 2), {70, 120, 210});
            }
            break;
        case 12:  // Peaks
            c.sky({90, 140, 220}, {210, 225, 245}, H);
            for (int k = 0; k < 4; ++k) {
                const double px = 8 + k * 16 + c.rng.uniform(-3, 3), top = 8 + c.rng.below(8);
                for (int y = static_cast<int>(top); y < H; ++y) {
                    const double half = (y - top) * 0.9;
                    for (int x = static_cast<int>(px - half); x <= px + half; ++x)
                        c.px(x, y, y < top + 5 ? Rgb{245, 245, 250} : Rgb{120, 115, 125});
                }
            }
            c.ridge(H * 0.85, 1.5, 0.3, 0.3, {60, 110, 60});
            break;
        case 13:  // Night Sky
            c.sky({10, 12, 35}, {35, 40, 80}, H);
            c.stars(70, H * 3 / 4);
            c.disc(W * 0.7, H * 0.25, 6, {240, 240, 220});
            c.disc(W * 0.7 + 3, H * 0.25 - 2, 5.5, {14, 16, 42});
            c.ridge(H * 0.8, 4, 0.1, 0.7, {15, 25, 20});
            for (int x = 4; x < W; x += 9) c.pine(x, H - 6, 9 + (x % 4), {8, 15, 10});
            break;
        case 14:  // Volcano
            c.sky({90, 20, 15}, {220, 110, 50}, H);
            for (int y = 18; y < H; ++y) {
                const double half = (y - 18) * 1.1 + 4;
                for (int x = static_cast<int>(W / 2 - half); x <= W / 2 + half; ++x) c.px(x, y, {60, 45, 40});
            }
            for (int y = 10; y < 26; ++y) c.rect(W / 2 - 2 + (y % 3) - 1, y, W / 2 + 2 + (y % 2), y, {255, 120, 20});
            for (int y = 26; y < H - 6; ++y) c.px(W / 2 + static_cast<int>(std::sin(y * 0.3) * 4), y, {255, 160, 40});
            c.water(H - 6, {230, 90, 20});
            break;
        default: break;
    }
    c.frame_all();
    return c.img;
}

}  // namespace

std::map<int, Image> make_building_tiles() {
    std::map<int, std::function<Image(Rng&)>> gen = {
        {T_WOOD_DOOR_TOP, [](Rng& r) { return wood_door(r, true); }},
        {T_WOOD_DOOR_BOTTOM, [](Rng& r) { return wood_door(r, false); }},
        {T_IRON_DOOR_TOP, [](Rng& r) { return iron_door(r, true); }},
        {T_IRON_DOOR_BOTTOM, [](Rng& r) { return iron_door(r, false); }},
        {T_TRAPDOOR, trapdoor},
        {T_LADDER, ladder},
        {T_NETHERRACK, netherrack},
        {T_SOUL_SAND, soul_sand},
        {T_CHEST_FRONT_LEFT, [](Rng& r) { return chest_half(r, true, false); }},
        {T_CHEST_FRONT_RIGHT, [](Rng& r) { return chest_half(r, true, true); }},
        {T_CHEST_BACK_LEFT, [](Rng& r) { return chest_half(r, false, false); }},
        {T_CHEST_BACK_RIGHT, [](Rng& r) { return chest_half(r, false, true); }},
        {T_PAINTING_BACK, painting_back},
    };
    std::map<int, Image> tiles;
    for (const auto& [index, g] : gen) {
        Rng rng(static_cast<uint64_t>(index) * 31337 + 5);
        tiles[index] = g(rng);
    }
    // The font in black, cut into tiles.
    Image font = make_font_image();
    for (auto& p : font.pixels) p = p.a ? Rgba{0, 0, 0, 255} : Rgba{0, 0, 0, 0};
    for (int ty = 0; ty * TILE < font.height; ++ty)
        for (int tx = 0; tx * TILE < font.width; ++tx) {
            Image t(TILE, TILE);
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x)
                    if (tx * TILE + x < font.width && ty * TILE + y < font.height) t.at(x, y) = font.at(tx * TILE + x, ty * TILE + y);
            tiles[FONT_TILE_BASE + ty * ATLAS_COLS + tx] = t;
        }
    // The paintings, cut into tiles row by row.
    const auto& motifs = painting_motifs();
    for (int m = 0; m < static_cast<int>(motifs.size()); ++m) {
        const Image art = paint(m, motifs[m]);
        for (int y = 0; y < motifs[m].h; ++y)
            for (int x = 0; x < motifs[m].w; ++x) {
                Image t(TILE, TILE);
                for (int py = 0; py < TILE; ++py)
                    for (int px = 0; px < TILE; ++px) t.at(px, py) = art.at(x * TILE + px, y * TILE + py);
                tiles[motifs[m].tile + y * motifs[m].w + x] = t;
            }
    }
    return tiles;
}
