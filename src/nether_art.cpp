// Pixel art for the Nether (M14): the portal's purple swirl, the ghast and its fireball, and the
// zombie pigman. Drawn by the code like the rest of the textures.
#include <algorithm>
#include <cmath>
#include <functional>

#include "blocks.h"
#include "rng.h"
#include "textures.h"

namespace {

uint8_t clamp8(double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); }
Rgba rgb(double r, double g, double b, double f = 1.0, uint8_t a = 255) { return {clamp8(r * f), clamp8(g * f), clamp8(b * f), a}; }

Image noise(Rng& rng, double r, double g, double b, double spread) {
    Image img(TILE, TILE);
    for (auto& p : img.pixels) p = rgb(r, g, b, 1 + rng.uniform(-spread, spread));
    return img;
}

// See-through purple swirling out from the middle (drawn with the water, so it blends).
Image portal(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const double dx = x - 7.5, dy = y - 7.5, r = std::hypot(dx, dy);
            const double swirl = std::sin(std::atan2(dy, dx) * 3 + r * 0.9) * 0.5 + 0.5;
            const double f = 0.55 + swirl * 0.45 + rng.uniform(-0.08, 0.08);
            img.at(x, y) = rgb(120 + swirl * 60, 30 + swirl * 40, 220, f, 190);
        }
    return img;
}

// Ghast skin: off-white with faint grey patches.
Image ghast(Rng& rng) {
    Image img = noise(rng, 238, 238, 238, 0.04);
    for (int i = 0; i < 10; ++i) img.at(rng.below(TILE), rng.below(TILE)) = rgb(205, 205, 210);
    return img;
}

// The face: sad closed eyes and a small mouth, or (shooting) open red eyes and a wide mouth.
Image ghast_face(Rng& rng, bool firing) {
    Image img = ghast(rng);
    const Rgba dark = rgb(60, 60, 60), red = rgb(200, 30, 30);
    for (int x = 3; x <= 5; ++x) img.at(x, 5) = img.at(x + 7, 5) = firing ? red : dark;
    if (firing) {
        for (int x = 3; x <= 5; ++x) img.at(x, 6) = img.at(x + 7, 6) = red;
        for (int y = 9; y <= 12; ++y)
            for (int x = 5; x <= 10; ++x) img.at(x, y) = dark;
    } else {
        for (int x = 4; x <= 5; ++x) img.at(x, 7) = img.at(x + 6, 7) = rgb(120, 120, 120);  // tear streaks
        for (int x = 7; x <= 8; ++x) img.at(x, 11) = dark;
    }
    return img;
}

// The fireball: orange and yellow flames around a hot white middle.
Image fireball(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            const double r = std::hypot(x - 7.5, y - 7.5) / 8 + rng.uniform(-0.12, 0.12);
            img.at(x, y) = r < 0.35 ? rgb(255, 250, 200) : r < 0.65 ? rgb(255, 190, 40) : rgb(230, 80, 20, 1 - (r - 0.65));
        }
    return img;
}

Image pigman_skin(Rng& rng) {
    Image img = noise(rng, 232, 160, 155, 0.06);
    for (int i = 0; i < 8; ++i) img.at(rng.below(TILE), rng.below(TILE)) = rgb(210, 130, 128);
    return img;
}

Image pigman_rot(Rng& rng) {
    Image img = noise(rng, 110, 140, 70, 0.12);
    for (int y = 2; y < TILE; y += 4)
        for (int x = 1; x < TILE - 1; ++x) img.at(x, y) = rgb(215, 210, 180, 1 + rng.uniform(-0.05, 0.05));  // bone
    return img;
}

}  // namespace

std::map<int, Image> make_nether_tiles() {
    std::map<int, std::function<Image(Rng&)>> gen = {
        {T_PORTAL, portal},
        {T_GHAST, ghast},
        {T_GHAST_FACE, [](Rng& r) { return ghast_face(r, false); }},
        {T_GHAST_FACE_FIRING, [](Rng& r) { return ghast_face(r, true); }},
        {T_FIREBALL, fireball},
        {T_PIGMAN_SKIN, pigman_skin},
        {T_PIGMAN_ROT, pigman_rot},
    };
    std::map<int, Image> tiles;
    for (const auto& [index, g] : gen) {
        Rng rng(static_cast<uint64_t>(index) * 7919 + 14);
        tiles[index] = g(rng);
    }
    return tiles;
}
