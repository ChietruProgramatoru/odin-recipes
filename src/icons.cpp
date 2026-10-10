#include "icons.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "rng.h"
#include "textures.h"

namespace {

struct Rgb {
    int r, g, b;
};

uint8_t clamp8(double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); }
Rgba shade(Rgb c, double f, uint8_t a = 255) { return {clamp8(c.r * f), clamp8(c.g * f), clamp8(c.b * f), a}; }

using Rows = std::vector<const char*>;
using Palette = std::vector<std::pair<char, Rgba>>;

// A material colour as a palette: k outline, d dark, m middle, l light.
Palette material(Rgb c) {
    return {{'k', shade(c, 0.35)}, {'d', shade(c, 0.72)}, {'m', shade(c, 1.0)}, {'l', shade(c, 1.3)}};
}
Palette operator+(Palette a, const Palette& b) {
    a.insert(a.begin(), b.begin(), b.end());  // earlier entries win
    return a;
}

const Palette HANDLE = {{'h', {137, 103, 56, 255}}, {'H', {73, 54, 28, 255}}};

// Draw a sprite of up to 16x16 characters, centred in a tile. '.' and unknown letters are see-through.
Image sprite(const Rows& rows, const Palette& palette) {
    Image img(TILE, TILE);
    int h = static_cast<int>(rows.size()), w = static_cast<int>(std::strlen(rows[0]));
    int x0 = (TILE - w) / 2, y0 = (TILE - h) / 2;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w && rows[y][x]; ++x)
            for (const auto& [ch, color] : palette)
                if (rows[y][x] == ch) {
                    img.at(x0 + x, y0 + y) = color;
                    break;
                }
    return img;
}

// --- Shapes -----------------------------------------------------------------------------

const Rows SWORD = {
    "..............kk",
    ".............klk",
    "............klmk",
    "...........klmk.",
    "..........klmk..",
    ".........klmk...",
    "........klmk....",
    ".......klmk.....",
    "..kk..klmk......",
    "..kdkklmk.......",
    "...kdkmk........",
    "...kHdk.........",
    "..khkkdk........",
    ".khk...kk.......",
    "kHk.............",
    "kk..............",
};
const Rows SHOVEL = {
    "...........kkk..",
    "..........kllmk.",
    ".........klmmmdk",
    ".........kmmmddk",
    "..........kmddk.",
    ".........khkkk..",
    "........khk.....",
    ".......khk......",
    "......khk.......",
    ".....khk........",
    "....khk.........",
    "...khk..........",
    "..khk...........",
    ".kHk............",
    ".kk.............",
};
const Rows PICKAXE = {
    "....kkkkkkk.....",
    "...kllmmmmmkk...",
    "....kkkkkkmddk..",
    "........khkkmdk.",
    ".......khk..kmdk",
    "......khk....kmk",
    ".....khk.....kmk",
    "....khk.......kk",
    "...khk..........",
    "..khk...........",
    ".khk............",
    "kHk.............",
    "kk..............",
};
const Rows AXE = {
    ".........kkk....",
    "........kllmk...",
    ".......klmmmdk..",
    ".......kmmmhkdk.",
    "........kmkhkdk.",
    ".........khkkk..",
    "........khk.....",
    ".......khk......",
    "......khk.......",
    ".....khk........",
    "....khk.........",
    "...khk..........",
    "..khk...........",
    ".kHk............",
    ".kk.............",
};
const Rows HOE = {
    ".......kkkkk....",
    "......klmmmmk...",
    ".......kkkhkdk..",
    ".........khk.k..",
    "........khk.....",
    ".......khk......",
    "......khk.......",
    ".....khk........",
    "....khk.........",
    "...khk..........",
    "..khk...........",
    ".kHk............",
    ".kk.............",
};
const Rows* TOOL_SHAPES[5] = {&SWORD, &SHOVEL, &PICKAXE, &AXE, &HOE};

const Rows HELMET = {
    "...kkkkkkkkkk...",
    "..klllmmmmmmdk..",
    "..klmmmmmmmmdk..",
    "..klmmkkkkkmdk..",
    "..kmmk.....kdk..",
    "..kmdk.....kdk..",
    "..kkkk.....kkk..",
};
const Rows CHESTPLATE = {
    ".kkkk......kkkk.",
    ".klmmkkkkkkmmdk.",
    ".klmmmmmmmmmmdk.",
    ".kkklmmmmmmdkkk.",
    "...klmmmmmmdk...",
    "...klmmmmmmdk...",
    "...klmmmmmmdk...",
    "...klmmmmmmdk...",
    "...kmmmmmmmdk...",
    "...kkkkkkkkkk...",
};
const Rows LEGGINGS = {
    "...kkkkkkkkkk...",
    "...klmmmmmmdk...",
    "...klmmkkmmdk...",
    "...klmk..kmdk...",
    "...klmk..kmdk...",
    "...klmk..kmdk...",
    "...klmk..kmdk...",
    "...klmk..kmdk...",
    "...kkkk..kkkk...",
};
const Rows BOOTS = {
    "..kkkk...kkkk...",
    "..klmk...klmk...",
    "..klmk...klmk...",
    ".klmmk..klmmk...",
    "klmmdk.klmmdk...",
    "kkkkkk.kkkkkk...",
};
const Rows* ARMOR_SHAPES[4] = {&HELMET, &CHESTPLATE, &LEGGINGS, &BOOTS};

const Rows INGOT = {
    "....kkkkkkkk",
    "...kllllllmk",
    "..klmmmmmmdk",
    ".klmmmmmmdk.",
    "kmmmmmmmdk..",
    "kdddddddk...",
    "kkkkkkkkk...",
};
const Rows GEM = {
    "...kkkkk...",
    "..kllmmmk..",
    ".klllmmmdk.",
    "kmmmmmmmddk",
    ".kmmmmmmdk.",
    "..kmmmmdk..",
    "...kmmdk...",
    "....kdk....",
    ".....k.....",
};
const Rows LUMP = {
    "....kkkk...",
    "..kkmlmmk..",
    ".kmmllmmdk.",
    "kmlmmmmdmdk",
    "kmmmmdmmmdk",
    ".kdmmmmmdk.",
    ".kkddmddk..",
    "...kkkkk...",
};
const Rows DUST = {
    "......l.....",
    "...m..lm..l.",
    "..lmd.md.m..",
    ".mlmmdmmdmd.",
    "..mdmmlmmdm.",
    ".lmmdmmmlmd.",
    "dmmldmmdmmmd",
    ".dmdddmddmd.",
};
const Rows BALL = {
    "...kkkk...",
    "..kllmmk..",
    ".kllmmmdk.",
    "kllmmmmmdk",
    "klmmmmmmdk",
    "kmmmmmmddk",
    ".kmmmmddk.",
    "..kddddk..",
    "...kkkk...",
};
const Rows DYE = {
    "....kkk....",
    "...kdmdk...",
    "....kmk....",
    "...kmmmk...",
    "..kllmmdk..",
    ".kllmmmmdk.",
    ".klmmmmmdk.",
    ".kmmmmmmdk.",
    "..kmmmmdk..",
    "...kkkkk...",
};
const Rows STICK_ROWS = {
    "..........kk",
    ".........khk",
    "........khk.",
    ".......khk..",
    "......khk...",
    ".....khk....",
    "....khk.....",
    "...khk......",
    "..khk.......",
    ".kHk........",
    ".kk.........",
};
const Rows STRING_ROWS = {
    "..........ww",
    ".........w..",
    "...ww...w...",
    "..w..w.w....",
    ".w....w.....",
    ".w..........",
    "..w.........",
    "...w....ww..",
    "....w..w..w.",
    ".....ww....w",
};
const Rows FEATHER_ROWS = {
    ".........wwk",
    ".......wwwwk",
    "......wwwwgk",
    ".....wwwwgk.",
    "....wwwwgk..",
    "...wwwwgk...",
    "..wwwwgk....",
    "..wwwgk.....",
    ".kwwgk......",
    ".kgk........",
    "kk..........",
};
const Rows FLINT_ROWS = {
    "....kkk...",
    "...kllmk..",
    "..klmmmdk.",
    ".klmmmmdk.",
    ".kmmmmmmdk",
    "kmmmdmmmdk",
    "kdmmmmmdk.",
    ".kddddkk..",
    "..kkkk....",
};
const Rows LEATHER_ROWS = {
    "..kkkkkkkk..",
    ".kllmmmmmdk.",
    "klmmmmmmmmdk",
    "kmmmmmmmmmdk",
    ".kmmmmmmmdk.",
    ".kmmmmmmmdk.",
    "kmmmmmmmmmdk",
    "kdmmmmmmmddk",
    ".kkkkkkkkkk.",
};
const Rows BRICK_ROWS = {
    "...kkkkkkkk",
    "..kllllmmmk",
    ".klmmmmmmdk",
    "kmmmmmmmdk.",
    "kddddddddk.",
    "kkkkkkkkkk.",
};
const Rows PAPER_ROWS = {
    "..kkkkkkkk..",
    "..kwwwwwwk..",
    ".kwwwwwwwk..",
    ".kwgggggwk..",
    ".kwwwwwwwwk.",
    ".kwggggggwk.",
    "..kwwwwwwwk.",
    "..kwggggwwk.",
    ".kwwwwwwwk..",
    ".kkkkkkkkk..",
};
const Rows BOOK_ROWS = {
    "..kkkkkkkkk.",
    ".kmmmmmmmmwk",
    ".kmllmmmmmwk",
    ".kmmmmmmmmwk",
    ".kmmmmmmmmwk",
    ".kmmmmmmmmwk",
    ".kmmmmmmmmwk",
    ".kdddddddddk",
    "..kkkkkkkkk.",
};
const Rows BONE_ROWS = {
    "........kk.k",
    ".......kwwkw",
    "........kwwk",
    ".......kwwk.",
    "......kwwk..",
    ".....kwwk...",
    "....kwwk....",
    "...kwwk.....",
    "..kwwk......",
    "kwwk........",
    "wkwwk.......",
    "k.kk........",
};
const Rows WHEAT_ROWS = {
    "........y.y.",
    ".......yyyy.",
    "......y.yy..",
    ".....yyyy.y.",
    "....y.yy....",
    "...yyyy.....",
    "..g.yy......",
    ".g..........",
    "g...........",
};
const Rows EGG_ROWS = {
    "...kkk...",
    "..kwwwk..",
    ".kwwwwwk.",
    ".kwwwwwk.",
    "kwwwwwwmk",
    "kwwwwwwmk",
    "kwwwwwmmk",
    ".kmwmmmk.",
    "..kkkkk..",
};
const Rows CANE_ROWS = {
    "...g...g....",
    "..gg..gg..g.",
    "..kg..kg.gg.",
    "..gg..gg.kg.",
    "..gg..gg.gg.",
    "..kg..kg.gg.",
    "..gg..gg.kg.",
    "..gg..gg.gg.",
    "..kg..kg.gg.",
    "..gg..gg.gg.",
};
const Rows BUCKET_ROWS = {
    "..kkkkkkkk..",
    ".kccccccccck",
    ".kllccccccdk",
    ".klmmmmmmmdk",
    "..klmmmmmdk.",
    "..klmmmmmdk.",
    "...kmmmmdk..",
    "...kkkkkkk..",
};
const Rows BOWL_ROWS = {
    "kkkkkkkkkkkk",
    "kccccccccccd",
    ".kllmmmmmdk.",
    "..kmmmmmmk..",
    "...kkkkkk...",
};
const Rows BREAD_ROWS = {
    "....kkkkk...",
    "..kkmllmmk..",
    ".kmlmlmmlmk.",
    "kmmlmlmmlmdk",
    "kmmmmmmmmmdk",
    ".kdddddddk..",
    "..kkkkkkk...",
};
const Rows COOKIE_ROWS = {
    "..kkkkk..",
    ".kmmcmmk.",
    "kmcmmmcmk",
    "kmmmmmmmk",
    "kmmcmmcmk",
    ".kmmmmmk.",
    "..kkkkk..",
};
const Rows CAKE_ROWS = {
    "...kkkkkkkk..",
    "..kwwrwwwrwk.",
    ".kwwwwwwwwwwk",
    ".kwwrwwwrwwwk",
    ".kwmwmwwmwmwk",
    ".kmmmmmmmmmmk",
    ".kmmmmmmmmmdk",
    "..kkkkkkkkkk.",
};
const Rows APPLE_ROWS = {
    ".....g....",
    "....g.....",
    "..mm.mmm..",
    ".mmmmmmlm.",
    ".mmmmmmmm.",
    ".mmmmmmmm.",
    ".dmmmmmmm.",
    "..dmmmmm..",
    "...dd.dd..",
};
const Rows CHOP_ROWS = {
    "...kkkkk....",
    "..kmmmmmkk..",
    ".kmmlmmmmmk.",
    "kmmmmmwmmmmk",
    "kmmmmmmmmwmk",
    ".kmmmmmmmmdk",
    "..kkdmmmddk.",
    "....kkkkkk..",
};
const Rows FISH_ROWS = {
    "..........kk",
    "...kkkkk.kmk",
    "..kmmmmmkmmk",
    ".kwmmlmmmmk.",
    "kkmmmmmmmmk.",
    ".kmmmmmmdkmk",
    "..kdddddkkmk",
    "...kkkkk..kk",
};
const Rows COMPASS_ROWS = {
    "...kkkkk...",
    "..kmmmmmk..",
    ".kmwwwwwmk.",
    "kmwwwrwwwmk",
    "kmwwwrwwwmk",
    "kmwwwkwwwmk",
    "kmwwwgwwwmk",
    ".kmwwwwwmk.",
    "..kmmmmmk..",
    "...kkkkk...",
};
const Rows MAP_ROWS = {
    "kkkkkkkkkkkk",
    "kwwwwwwwwwwk",
    "kwggwwwbbwwk",
    "kwgggwbbbbwk",
    "kwwggwwbbwwk",
    "kwwwwwwwwwwk",
    "kwbbwwgggwwk",
    "kwbbbwwggwwk",
    "kwwwwwwwwwwk",
    "kkkkkkkkkkkk",
};
const Rows DOOR_ROWS = {
    "kkkkkkkk",
    "kmlmmlmk",
    "kmdmmdmk",
    "kmdmmdmk",
    "kmmmmmmk",
    "kmmmmmmk",
    "kmlmmlmk",
    "kmdmmmck",
    "kmdmmdmk",
    "kmmmmmmk",
    "kmlmmlmk",
    "kmdmmdmk",
    "kmmmmmmk",
    "kkkkkkkk",
};
const Rows SIGN_ROWS = {
    "kkkkkkkkkkkk",
    "kmmmmmmmmmmk",
    "kmddddddddmk",
    "kmmmmmmmmmmk",
    "kmdddddddmmk",
    "kmmmmmmmmmmk",
    "kkkkkhhkkkkk",
    ".....hH.....",
    ".....hH.....",
    ".....hH.....",
};
const Rows BED_ROWS = {
    "............kk",
    "kkkkkkkkkk.kwk",
    "krrrrrrrrrkwwk",
    "krrrrrrrrrrwwk",
    "khhhhhhhhhhhhk",
    "kH..........Hk",
};
const Rows PAINTING_ROWS = {
    "hhhhhhhhhhhh",
    "hbbbbbbbbbbh",
    "hbbbbyybbbbh",
    "hbbbyyyybbbh",
    "hbbbbyybbbbh",
    "hbgggbbbbbbh",
    "hggggggbgggh",
    "hgggggggggggh",
    "hhhhhhhhhhhh",
};
const Rows BOAT_ROWS = {
    "kk..........kk",
    "khk........khk",
    "khhkkkkkkkkhhk",
    "khhhhhhhhhhhhk",
    ".kHhhhhhhhhhk.",
    "..kHHHHHHHHk..",
    "...kkkkkkkk...",
};
const Rows CART_ROWS = {
    "..cccccccccc..",
    "..cccccccccc..",
    "kkkkkkkkkkkkkk",
    "kmlllllllllldk",
    "kmmmmmmmmmmmdk",
    ".kmmmmmmmmmdk.",
    ".kkkkkkkkkkkk.",
    "..kdk....kdk..",
    "...k......k...",
};
const Rows REPEATER_ROWS = {
    "....r.....r.....",
    "...rrr...rrr....",
    "....h.....h.....",
    "kkkkhkkkkkhkkkkk",
    "kmmmmmrrrrmmmmmk",
    "kmmmmmmmmmmmmmmk",
    "kddddddddddddddk",
    "kkkkkkkkkkkkkkkk",
};
const Rows SADDLE_ROWS = {
    "..kkk.....kkk...",
    ".kmmmkkkkkmmmk..",
    "kmllmmmmmmmmmdk.",
    "kmmmmmmmmmmmmdk.",
    ".kkkdmmmmmdkkk..",
    "....kwk.kwk.....",
    "....kwk.kwk.....",
    "....kkk.kkk.....",
};
const Rows BOW_ROWS = {
    "........kkkk..",
    "......kkhhhw..",
    ".....khhkk.w..",
    "....khk...w...",
    "...khk...w....",
    "..khk...w.....",
    "..kh...w......",
    ".kh...w.......",
    ".kh..w........",
    ".khkw.........",
    ".khw..........",
    "..kk..........",
};
const Rows ARROW_ROWS = {
    "..........kk",
    ".........kik",
    "........kik.",
    ".......khk..",
    "......khk...",
    ".....khk....",
    "....khk.....",
    ".fwkhk......",
    ".fwhk.......",
    ".fwf........",
    "ff..........",
};
const Rows FLINT_STEEL_ROWS = {
    "..kkk.......",
    ".kiiik......",
    "kiik.ik.....",
    "kik...ik....",
    ".k.....k....",
    "....kkkk....",
    "...kllmmk...",
    "..klmmmmdk..",
    "..kmmmmmdk..",
    "...kdddkk...",
    "....kkk.....",
};
const Rows SHEARS_ROWS = {
    "......kk..",
    ".....kik..",
    "....kik...",
    "...kik.kk.",
    "...kkkkik.",
    "..kmk.kik.",
    ".kmk...k..",
    "kmmk......",
    "kmk.......",
    "kk........",
};
const Rows ROD_ROWS = {
    "..........kk",
    ".........khk",
    "........khkw",
    ".......khk.w",
    "......khk..w",
    ".....khk...w",
    "....khk....w",
    "...khk.....w",
    "..khk......w",
    ".kHk......kk",
    ".kk.......k.",
};
const Rows CLOCK_ROWS = {
    "...kkkkk...",
    "..kmmmmmk..",
    ".kmyyyyymk.",
    "kmyyykyyymk",
    "kmyyykyyymk",
    "kmyyykkkymk",
    "kmbbbbbbbmk",
    ".kmbbbbbmk.",
    "..kmmmmmk..",
    "...kkkkk...",
};
const Rows TORCH_ROWS = {
    "..ff..",
    ".fyyf.",
    ".fyyf.",
    "..hh..",
    "..hH..",
    "..hH..",
    "..hH..",
    "..hH..",
    "..hH..",
    "..hH..",
};
const Rows FENCE_ROWS = {
    ".kk.....kk..",
    ".kh.....kh..",
    "kkhkkkkkkhkk",
    "khhhhhhhhhhk",
    "kkhkkkkkkhkk",
    ".kh.....kh..",
    "kkhkkkkkkhkk",
    "khhhhhhhhhhk",
    "kkhkkkkkkhkk",
    ".kh.....kh..",
    ".kH.....kH..",
    ".kk.....kk..",
};
const Rows LADDER_ROWS = {
    ".kk.....kk.",
    ".kh.....kh.",
    "kkhkkkkkkhk",
    "khhhhhhhhhk",
    "kkhkkkkkkhk",
    ".kh.....kh.",
    "kkhkkkkkkhk",
    "khhhhhhhhhk",
    "kkhkkkkkkhk",
    ".kh.....kh.",
    "kkhkkkkkkhk",
    "khhhhhhhhhk",
    "kkhkkkkkkhk",
    ".kh.....kh.",
};
const Rows TRAPDOOR_ROWS = {
    "kkkkkkkkkkkk",
    "khhkhhhhkhhk",
    "khhkhHHhkhhk",
    "kkkkkkkkkkkk",
    "khhkhhhhkhhk",
    "khhkhHHhkhhk",
    "kkkkkkkkkkkk",
    "khhkhhhhkhhk",
    "khhkhHHhkhhk",
    "kkkkkkkkkkkk",
};
const Rows LEVER_ROWS = {
    ".......kk.",
    "......khk.",
    ".....khk..",
    "....khk...",
    "...khk....",
    "..kkkkkk..",
    ".kllmmmdk.",
    ".kmmmmmdk.",
    ".kkkkkkkk.",
};
const Rows BUTTON_ROWS = {
    "..kkkkkk..",
    ".kllmmmdk.",
    ".klmmmmdk.",
    ".kmmmmmdk.",
    "..kkkkkk..",
};
const Rows PLATE_ROWS = {
    "kkkkkkkkkkkk",
    "kllllllllmdk",
    "kmmmmmmmmmdk",
    "kddddddddddk",
    "kkkkkkkkkkkk",
};
const Rows RAIL_ROWS = {
    ".ii......ii.",
    "hhhhhhhhhhhh",
    ".ii......ii.",
    ".iiccccccii.",
    ".ii......ii.",
    "hhhhhhhhhhhh",
    ".ii......ii.",
    ".iiccccccii.",
    ".ii......ii.",
    "hhhhhhhhhhhh",
    ".ii......ii.",
};
const Rows FLOWER_ROWS = {
    "...c.c...",
    "..ccccc..",
    "...cyc...",
    "..ccccc..",
    "...c.c...",
    "....g....",
    "..g.g....",
    "...gg.g..",
    "....gg...",
    "....g....",
};
const Rows MUSHROOM_ROWS = {
    "...kkkk...",
    "..kmmwmk..",
    ".kmwmmmwk.",
    "kmmmmwmmmk",
    "kkkkkkkkkk",
    "...kwwk...",
    "...kwwk...",
    "...kwwk...",
    "...kkkk...",
};

// --- Block faces --------------------------------------------------------------------------

Image noise(Rng& rng, Rgb base, double spread) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade(base, 1 + rng.uniform(-spread, spread));
    return img;
}

const Rgb PLANK{162, 130, 78};
const Rgb COBBLE_GREY{120, 120, 120};

Image planks_face(Rng& rng) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) {
            bool seam = y % 4 == 3 || (x + (y / 4) * 7) % TILE == 0;
            img.at(x, y) = shade(PLANK, seam ? 0.62 : rng.uniform(0.92, 1.06));
        }
    return img;
}

// Frame of `width` pixels around the edge of a tile.
void frame(Image& img, Rgb c, int width, double f = 1.0) {
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x)
            if (x < width || y < width || x >= TILE - width || y >= TILE - width) img.at(x, y) = shade(c, f);
}

void fill(Image& img, int x0, int y0, int x1, int y1, Rgba c) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) img.at(x, y) = c;
}

Image overlay(Image base, const Rows& rows, const Palette& palette) {
    Image top = sprite(rows, palette);
    for (size_t i = 0; i < base.pixels.size(); ++i)
        if (top.pixels[i].a) base.pixels[i] = top.pixels[i];
    return base;
}

Image craft_top(Rng& rng) {
    Image img = planks_face(rng);
    frame(img, {110, 80, 45}, 1);
    for (int i = 2; i < 14; ++i) {
        img.at(i, 5) = img.at(i, 10) = shade({96, 70, 40}, 1);
        img.at(5, i) = img.at(10, i) = shade({96, 70, 40}, 1);
    }
    return img;
}

Image craft_side(Rng& rng, bool front) {
    Image img = planks_face(rng);
    fill(img, 0, 0, 15, 3, shade({120, 88, 50}, 1));
    for (int x = 0; x < TILE; ++x) img.at(x, 3) = shade({70, 50, 28}, 1);
    if (front)  // a saw and a hammer hanging on the side
        return overlay(img, {"................", "................", "................", "................",
                             "..kkkk.....iii..", "..kiik.....iii..", "..kiik......h...", "..kiik......h...",
                             "..kiik......h...", "..kkhk......h...", "....h.......h...", "....h...........",
                             "................", "................", "................", "................"},
                       {{'k', {60, 60, 60, 255}}, {'i', {190, 190, 190, 255}}, {'h', {90, 62, 30, 255}}});
    return img;
}

Image chest_face(Rng& rng, int kind) {  // 0 top, 1 side, 2 front
    Image img = noise(rng, {165, 112, 45}, 0.07);
    frame(img, {80, 50, 20}, 1);
    if (kind != 0)
        for (int x = 1; x < 15; ++x) img.at(x, 5) = shade({80, 50, 20}, 1);
    if (kind == 2) fill(img, 7, 4, 8, 7, {200, 200, 200, 255});
    return img;
}

Image furnace_face(Rng& rng, int kind) {  // 0 top, 1 side, 2 front, 3 dispenser front
    Image img = noise(rng, COBBLE_GREY, 0.15);
    for (int i = 0; i < 20; ++i) img.at(rng.below(TILE), rng.below(TILE)) = shade(COBBLE_GREY, 0.6);
    if (kind == 0) {
        fill(img, 2, 2, 13, 13, shade({100, 100, 100}, 1));
        frame(img, {80, 80, 80}, 1);
    }
    if (kind == 2) {
        fill(img, 4, 8, 11, 13, {25, 25, 25, 255});
        fill(img, 3, 3, 12, 4, shade({80, 80, 80}, 1));
    }
    if (kind == 3) {
        fill(img, 5, 5, 10, 10, {25, 25, 25, 255});
        fill(img, 6, 6, 9, 9, {60, 60, 60, 255});
    }
    return img;
}

Image wool(Rng& rng, Rgb c) {
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x)
            img.at(x, y) = shade(c, ((x + y) % 4 == 0 ? 0.88 : 1.0) * rng.uniform(0.93, 1.05));
    return img;
}

Image tnt(Rng& rng, bool top) {
    if (top) {
        Image img = noise(rng, {180, 60, 50}, 0.06);
        fill(img, 6, 6, 9, 9, {60, 60, 60, 255});
        return img;
    }
    Image img = noise(rng, {210, 55, 40}, 0.06);
    fill(img, 0, 5, 15, 10, shade({225, 220, 210}, 1));
    // "TNT" in 3x4 letters.
    const char* letters[4] = {"### # # ###", ".#. ### .#.", ".#. ### .#.", ".#. # # .#."};
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 11; ++x)
            if (letters[y][x] == '#') img.at(2 + x, 6 + y) = {30, 30, 30, 255};
    return img;
}

Image bookshelf(Rng& rng) {
    Image img = planks_face(rng);
    const Rgb colors[6] = {{170, 40, 40}, {40, 90, 160}, {60, 130, 60}, {170, 130, 40}, {110, 60, 140}, {90, 60, 40}};
    for (int shelf = 0; shelf < 2; ++shelf)
        for (int x = 1; x < 15; ++x) {
            Rgb c = colors[(x * 7 + shelf * 3) % 6];
            for (int y = 1 + shelf * 8; y < 7 + shelf * 8; ++y) img.at(x, y) = shade(c, x % 2 ? 1.0 : 0.85);
        }
    return img;
}

Image metal_block(Rng& rng, Rgb c) {
    Image img = noise(rng, c, 0.04);
    frame(img, c, 1, 0.7);
    for (int i = 2; i < 6; ++i) img.at(i, 2) = img.at(2, i) = shade(c, 1.3);
    return img;
}

Image glowstone(Rng& rng) {
    Image img = noise(rng, {200, 160, 80}, 0.2);
    for (int i = 0; i < 30; ++i) img.at(rng.below(TILE), rng.below(TILE)) = shade({255, 230, 150}, 1);
    return img;
}

Image pumpkin(Rng& rng, int kind) {  // 0 top, 1 side, 2 lit face, 3 dark face
    Image img(TILE, TILE);
    for (int y = 0; y < TILE; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = shade({215, 125, 25}, (x % 4 == 0 ? 0.8 : 1.0) * rng.uniform(0.95, 1.05));
    if (kind == 0) fill(img, 7, 7, 8, 8, {90, 70, 30, 255});
    if (kind >= 2) {
        Rgba hole = kind == 2 ? Rgba{255, 220, 80, 255} : Rgba{50, 30, 10, 255};
        fill(img, 3, 4, 5, 6, hole);
        fill(img, 10, 4, 12, 6, hole);
        fill(img, 3, 10, 12, 11, hole);
        fill(img, 5, 12, 6, 12, hole);
        fill(img, 9, 12, 10, 12, hole);
    }
    return img;
}

Image sandstone(Rng& rng, bool top) {
    Image img = noise(rng, {219, 205, 160}, 0.05);
    if (!top) {
        for (int x = 0; x < TILE; ++x) img.at(x, 3) = img.at(x, 12) = shade({190, 175, 130}, 1);
        fill(img, 0, 13, 15, 15, shade({200, 185, 140}, 1));
    }
    return img;
}

Image note_block(Rng& rng) {
    Image img = planks_face(rng);
    frame(img, {80, 50, 30}, 1);
    return overlay(img, {"............", ".....kkkkk..", ".....k...k..", ".....k...k..", "...kkk.kkk..",
                         "..kkkk.kkkk.", "...kk...kk.."},
                   {{'k', {40, 25, 15, 255}}});
}

Image jukebox_top(Rng& rng) {
    Image img = planks_face(rng);
    frame(img, {80, 50, 30}, 1);
    fill(img, 3, 7, 12, 8, {30, 20, 15, 255});
    return img;
}

Image piston_top(Rng& rng, bool sticky) {
    Image img = planks_face(rng);
    frame(img, {110, 80, 45}, 1);
    if (sticky) fill(img, 2, 2, 13, 13, shade({110, 170, 90}, rng.uniform(0.95, 1.05)));
    fill(img, 6, 6, 9, 9, {150, 150, 150, 255});
    return img;
}

Image piston_side(Rng& rng) {
    Image img = noise(rng, COBBLE_GREY, 0.12);
    Image top = planks_face(rng);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < TILE; ++x) img.at(x, y) = top.at(x, y);
    fill(img, 6, 4, 9, 12, {170, 170, 170, 255});
    return img;
}

Image slab_face(Rng& rng, bool top) {
    Image img = noise(rng, {160, 160, 160}, 0.04);
    frame(img, {160, 160, 160}, 1, 0.75);
    if (!top)
        for (int x = 0; x < TILE; ++x) img.at(x, 7) = img.at(x, 8) = shade({160, 160, 160}, 0.75);
    return img;
}

// --- Flat icons ---------------------------------------------------------------------------

const Rgb TOOL_COLORS[5] = {{160, 122, 72}, {130, 130, 130}, {215, 215, 215}, {245, 215, 60}, {90, 225, 215}};
const Rgb ARMOR_COLORS[5] = {{150, 92, 50}, {150, 150, 150}, {215, 215, 215}, {245, 215, 60}, {90, 225, 215}};
const Rgb DYE_COLORS[16] = {{30, 30, 35},    {185, 45, 40},   {60, 100, 30},   {100, 65, 35},  {40, 70, 180},
                            {130, 60, 180}, {50, 130, 150},  {155, 155, 160}, {75, 75, 80},   {230, 140, 165},
                            {120, 200, 50}, {230, 210, 50},  {120, 160, 225}, {195, 80, 190}, {230, 130, 50},
                            {235, 235, 225}};
// Wool colour for each wool item (white first), which is dye colour 15 - c.
Rgb wool_color(int c) { return c == 0 ? Rgb{232, 232, 232} : DYE_COLORS[15 - c]; }

Image flat_icon(ItemId id) {
    auto mat = [](Rgb c) { return material(c); };
    const Palette ink = {{'w', {240, 240, 240, 255}}, {'g', {120, 120, 120, 255}}};
    if (id >= WOOD_SWORD && id <= DIAMOND_HOE) {
        int m = (id - WOOD_SWORD) / 5, k = (id - WOOD_SWORD) % 5;
        return sprite(*TOOL_SHAPES[k], HANDLE + mat(TOOL_COLORS[m]));
    }
    if (id >= LEATHER_HELMET && id <= DIAMOND_BOOTS) {
        int m = (id - LEATHER_HELMET) / 4, p = (id - LEATHER_HELMET) % 4;
        Image img = sprite(*ARMOR_SHAPES[p], mat(ARMOR_COLORS[m]));
        if (m == 1)  // chain: little holes
            for (int y = 0; y < TILE; y += 2)
                for (int x = y % 4; x < TILE; x += 4)
                    if (img.at(x, y).a && img.at(x, y).r > 80) img.at(x, y) = {70, 70, 70, 255};
        return img;
    }
    if (id >= INK_SAC && id <= BONE_MEAL) {
        Rgb c = DYE_COLORS[id - INK_SAC];
        if (id == BONE_MEAL || id == SUGAR) return sprite(DUST, mat(c));
        if (id == COCOA_BEANS) return sprite(LUMP, mat(c));
        if (id == LAPIS_LAZULI) return sprite(GEM, mat(c));
        return sprite(DYE, mat(c));
    }
    Rgb wood{160, 122, 72}, iron{215, 215, 215}, gold{245, 215, 60};
    switch (id) {
    case STICK: return sprite(STICK_ROWS, HANDLE + mat({60, 40, 20}));
    case COAL: return sprite(LUMP, mat({55, 55, 55}));
    case CHARCOAL: return sprite(LUMP, mat({70, 55, 40}));
    case DIAMOND: return sprite(GEM, mat({90, 230, 220}));
    case IRON_INGOT: return sprite(INGOT, mat(iron));
    case GOLD_INGOT: return sprite(INGOT, mat(gold));
    case REDSTONE: return sprite(DUST, mat({200, 20, 20}));
    case GLOWSTONE_DUST: return sprite(DUST, mat({240, 200, 90}));
    case GUNPOWDER: return sprite(DUST, mat({90, 90, 90}));
    case SUGAR: return sprite(DUST, mat({245, 245, 245}));
    case STRING: return sprite(STRING_ROWS, {{'w', {235, 235, 235, 255}}});
    case FEATHER: return sprite(FEATHER_ROWS, {{'w', {245, 245, 245, 255}}, {'g', {180, 180, 180, 255}}, {'k', {90, 90, 90, 255}}});
    case FLINT: return sprite(FLINT_ROWS, mat({70, 70, 75}));
    case LEATHER: return sprite(LEATHER_ROWS, mat({150, 92, 50}));
    case CLAY_BALL: return sprite(BALL, mat({160, 165, 180}));
    case SLIMEBALL: return sprite(BALL, mat({110, 200, 90}));
    case SNOWBALL: return sprite(BALL, mat({240, 245, 250}));
    case CLAY_BRICK: return sprite(BRICK_ROWS, mat({170, 80, 55}));
    case PAPER: return sprite(PAPER_ROWS, ink + Palette{{'k', {150, 150, 140, 255}}});
    case BOOK: return sprite(BOOK_ROWS, Palette{{'w', {240, 235, 220, 255}}} + mat({120, 70, 40}));
    case BONE: return sprite(BONE_ROWS, {{'w', {240, 235, 220, 255}}, {'k', {120, 115, 100, 255}}});
    case WHEAT: return sprite(WHEAT_ROWS, {{'y', {210, 180, 70, 255}}, {'g', {120, 140, 50, 255}}});
    case EGG: return sprite(EGG_ROWS, {{'w', {240, 225, 190, 255}}, {'m', {215, 195, 150, 255}}, {'k', {120, 100, 70, 255}}});
    case SUGAR_CANE: return sprite(CANE_ROWS, {{'g', {140, 200, 90, 255}}, {'k', {90, 140, 60, 255}}});
    case BOW: return sprite(BOW_ROWS, HANDLE + Palette{{'k', {60, 40, 20, 255}}, {'w', {230, 230, 230, 255}}});
    case ARROW:
        return sprite(ARROW_ROWS, HANDLE + Palette{{'k', {50, 50, 50, 255}}, {'i', {200, 200, 200, 255}},
                                                   {'f', {240, 240, 240, 255}}, {'w', {200, 200, 200, 255}}});
    case FLINT_AND_STEEL:
        return sprite(FLINT_STEEL_ROWS, Palette{{'i', {200, 200, 200, 255}}} + mat({70, 70, 75}));
    case SHEARS: return sprite(SHEARS_ROWS, Palette{{'i', {220, 220, 220, 255}}} + mat({140, 140, 140}));
    case FISHING_ROD: return sprite(ROD_ROWS, HANDLE + Palette{{'k', {50, 35, 20, 255}}, {'w', {220, 220, 220, 255}}});
    case BUCKET: return sprite(BUCKET_ROWS, Palette{{'c', {60, 60, 60, 255}}} + mat(iron));
    case WATER_BUCKET: return sprite(BUCKET_ROWS, Palette{{'c', {50, 90, 210, 255}}} + mat(iron));
    case LAVA_BUCKET: return sprite(BUCKET_ROWS, Palette{{'c', {240, 110, 20, 255}}} + mat(iron));
    case MILK_BUCKET: return sprite(BUCKET_ROWS, Palette{{'c', {250, 250, 250, 255}}} + mat(iron));
    case BOWL: return sprite(BOWL_ROWS, Palette{{'c', {90, 62, 35, 255}}} + mat(wood));
    case MUSHROOM_STEW: return sprite(BOWL_ROWS, Palette{{'c', {200, 150, 110, 255}}} + mat(wood));
    case BREAD: return sprite(BREAD_ROWS, mat({190, 135, 60}));
    case COOKIE: return sprite(COOKIE_ROWS, Palette{{'c', {80, 45, 20, 255}}} + mat({210, 150, 80}));
    case CAKE:
        return sprite(CAKE_ROWS, {{'w', {245, 245, 240, 255}}, {'r', {220, 40, 40, 255}}, {'m', {190, 130, 70, 255}},
                                  {'d', {150, 100, 50, 255}}, {'k', {80, 55, 30, 255}}});
    case APPLE: return sprite(APPLE_ROWS, Palette{{'g', {70, 110, 30, 255}}} + mat({210, 30, 30}));
    case GOLDEN_APPLE: return sprite(APPLE_ROWS, Palette{{'g', {70, 110, 30, 255}}} + mat({245, 205, 50}));
    case BEEF: return sprite(CHOP_ROWS, Palette{{'w', {245, 225, 225, 255}}} + mat({215, 70, 70}));
    case RAW_PORKCHOP: return sprite(CHOP_ROWS, Palette{{'w', {250, 235, 230, 255}}} + mat({240, 150, 150}));
    case COOKED_PORKCHOP: return sprite(CHOP_ROWS, Palette{{'w', {240, 220, 180, 255}}} + mat({180, 110, 60}));
    case RAW_FISH: return sprite(FISH_ROWS, Palette{{'w', {255, 255, 255, 255}}} + mat({120, 160, 190}));
    case COOKED_FISH: return sprite(FISH_ROWS, Palette{{'w', {255, 255, 255, 255}}} + mat({200, 150, 90}));
    case COMPASS:
        return sprite(COMPASS_ROWS, {{'w', {225, 225, 230, 255}}, {'r', {210, 30, 30, 255}}, {'g', {90, 90, 90, 255}},
                                     {'m', {150, 150, 155, 255}}, {'k', {60, 60, 60, 255}}});
    case CLOCK:
        return sprite(CLOCK_ROWS, Palette{{'y', {120, 180, 240, 255}}, {'b', {40, 40, 90, 255}}} + mat(gold));
    case MAP:
        return sprite(MAP_ROWS, {{'w', {230, 220, 180, 255}}, {'g', {110, 160, 70, 255}}, {'b', {90, 130, 210, 255}},
                                 {'k', {120, 100, 60, 255}}});
    case WOOD_DOOR: return sprite(DOOR_ROWS, Palette{{'c', {200, 200, 200, 255}}} + mat(wood));
    case IRON_DOOR: return sprite(DOOR_ROWS, Palette{{'c', {120, 120, 120, 255}}} + mat(iron));
    case SIGN: return sprite(SIGN_ROWS, HANDLE + mat(wood));
    case BED:
        return sprite(BED_ROWS, HANDLE + Palette{{'r', {180, 40, 40, 255}}, {'w', {235, 235, 235, 255}},
                                                 {'k', {50, 35, 20, 255}}});
    case PAINTING:
        return sprite(PAINTING_ROWS, HANDLE + Palette{{'b', {120, 170, 220, 255}}, {'y', {245, 220, 80, 255}},
                                                      {'g', {90, 150, 60, 255}}});
    case BOAT: return sprite(BOAT_ROWS, HANDLE + Palette{{'k', {60, 40, 20, 255}}});
    case MINECART: return sprite(CART_ROWS, mat({150, 150, 155}));
    case STORAGE_MINECART: return sprite(CART_ROWS, Palette{{'c', {165, 112, 45, 255}}} + mat({150, 150, 155}));
    case POWERED_MINECART: return sprite(CART_ROWS, Palette{{'c', {90, 90, 90, 255}}} + mat({150, 150, 155}));
    case REPEATER:
        return sprite(REPEATER_ROWS, HANDLE + Palette{{'r', {220, 30, 30, 255}}} + mat({160, 160, 160}));
    case SADDLE: return sprite(SADDLE_ROWS, Palette{{'w', {190, 190, 190, 255}}} + mat({140, 80, 40}));
    case TORCH: return sprite(TORCH_ROWS, HANDLE + Palette{{'f', {255, 160, 40, 255}}, {'y', {255, 240, 150, 255}}});
    case REDSTONE_TORCH: return sprite(TORCH_ROWS, HANDLE + Palette{{'f', {150, 20, 20, 255}}, {'y', {255, 60, 40, 255}}});
    case FENCE: return sprite(FENCE_ROWS, HANDLE + Palette{{'k', {60, 40, 20, 255}}});
    case LADDER: return sprite(LADDER_ROWS, HANDLE + Palette{{'k', {60, 40, 20, 255}}});
    case TRAPDOOR: return sprite(TRAPDOOR_ROWS, HANDLE + Palette{{'k', {70, 50, 25, 255}}});
    case LEVER: return sprite(LEVER_ROWS, HANDLE + mat({130, 130, 130}));
    case STONE_BUTTON: return sprite(BUTTON_ROWS, mat({140, 140, 140}));
    case STONE_PLATE: return sprite(PLATE_ROWS, mat({150, 150, 150}));
    case WOOD_PLATE: return sprite(PLATE_ROWS, mat(wood));
    case RAIL: return sprite(RAIL_ROWS, HANDLE + Palette{{'i', {170, 170, 170, 255}}});
    case POWERED_RAIL:
        return sprite(RAIL_ROWS, HANDLE + Palette{{'i', {240, 210, 70, 255}}, {'c', {200, 30, 30, 255}}});
    case DETECTOR_RAIL:
        return sprite(RAIL_ROWS, HANDLE + Palette{{'i', {170, 170, 170, 255}}, {'c', {120, 30, 30, 255}}});
    case YELLOW_FLOWER:
        return sprite(FLOWER_ROWS, {{'c', {245, 220, 40, 255}}, {'y', {250, 160, 30, 255}}, {'g', {60, 130, 40, 255}}});
    case RED_ROSE:
        return sprite(FLOWER_ROWS, {{'c', {215, 30, 30, 255}}, {'y', {120, 10, 10, 255}}, {'g', {60, 130, 40, 255}}});
    case BROWN_MUSHROOM: return sprite(MUSHROOM_ROWS, Palette{{'w', {225, 215, 195, 255}}} + mat({150, 110, 80}));
    case RED_MUSHROOM: return sprite(MUSHROOM_ROWS, Palette{{'w', {240, 240, 240, 255}}} + mat({210, 40, 40}));
    default: break;
    }
    // Anything without its own picture: a question mark on a dark square.
    Image img(TILE, TILE, {40, 40, 40, 255});
    return overlay(img, {"..kkk..", ".k...k.", "....k..", "...k...", "...k...", ".......", "...k..."},
                   {{'k', {255, 0, 255, 255}}});
}

}  // namespace

std::map<int, Image> make_item_tiles() {
    std::map<int, Image> tiles;
    auto add = [&](int index, const std::function<Image(Rng&)>& gen) {
        Rng rng(static_cast<uint64_t>(index) * 7919 + 17);
        tiles[index] = gen(rng);
    };
    add(IT_CRAFT_TOP, craft_top);
    add(IT_CRAFT_SIDE, [](Rng& r) { return craft_side(r, false); });
    add(IT_CRAFT_FRONT, [](Rng& r) { return craft_side(r, true); });
    for (int k = 0; k < 3; ++k) add(IT_CHEST_TOP + k, [k](Rng& r) { return chest_face(r, k); });
    for (int k = 0; k < 3; ++k) add(IT_FURNACE_TOP + k, [k](Rng& r) { return furnace_face(r, k); });
    for (int c = 0; c < 16; ++c) add(IT_WOOL + c, [c](Rng& r) { return wool(r, wool_color(c)); });
    add(IT_TNT_TOP, [](Rng& r) { return tnt(r, true); });
    add(IT_TNT_SIDE, [](Rng& r) { return tnt(r, false); });
    add(IT_BOOKSHELF, bookshelf);
    add(IT_GOLD_BLOCK, [](Rng& r) { return metal_block(r, {245, 210, 60}); });
    add(IT_IRON_BLOCK, [](Rng& r) { return metal_block(r, {220, 220, 220}); });
    add(IT_DIAMOND_BLOCK, [](Rng& r) { return metal_block(r, {100, 225, 215}); });
    add(IT_LAPIS_BLOCK, [](Rng& r) { return metal_block(r, {40, 70, 180}); });
    add(IT_GLOWSTONE, glowstone);
    for (int k = 0; k < 4; ++k) add(IT_PUMPKIN_TOP + k, [k](Rng& r) { return pumpkin(r, k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 2 : 3); });
    add(IT_SANDSTONE_TOP, [](Rng& r) { return sandstone(r, true); });
    add(IT_SANDSTONE_SIDE, [](Rng& r) { return sandstone(r, false); });
    add(IT_SNOW, [](Rng& r) { return noise(r, {240, 248, 250}, 0.03); });
    add(IT_CLAY, [](Rng& r) { return noise(r, {160, 165, 180}, 0.05); });
    add(IT_NOTE_BLOCK, note_block);
    add(IT_JUKEBOX_TOP, jukebox_top);
    add(IT_DISPENSER_FRONT, [](Rng& r) { return furnace_face(r, 3); });
    add(IT_PISTON_TOP, [](Rng& r) { return piston_top(r, false); });
    add(IT_STICKY_TOP, [](Rng& r) { return piston_top(r, true); });
    add(IT_PISTON_SIDE, piston_side);
    add(IT_SLAB_TOP, [](Rng& r) { return slab_face(r, true); });
    add(IT_SLAB_SIDE, [](Rng& r) { return slab_face(r, false); });
    for (ItemId id : all_items()) {
        const ItemInfo* info = item_info(id);
        if (info->icon.shape == ItemIcon::Flat) tiles[info->icon.top] = flat_icon(id);
    }
    return tiles;
}
