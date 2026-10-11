// Procedurally generated pixel-art textures, so the game ships without image files.
#pragma once
#include <array>
#include <map>
#include <string>

#include "image.h"

constexpr int TILE = 16;
constexpr int ATLAS_COLS = 16;
constexpr int ATLAS_ROWS = 40;  // world tiles first, item tiles from ITEM_TILE_BASE (icons.h), then:
// The HUD font in black (for the text on signs), 96 x 48 pixels from this tile's top-left corner,
// laid out like make_font_image(); then the paintings (painting.h), each row by row from its top-left.
constexpr int FONT_TILE_BASE = 512;
constexpr int PAINTING_TILE_BASE = 560;

// UV rectangle (u0, v0, u1, v1) of an atlas tile. The atlas is uploaded so v = 1 is its top row.
std::array<float, 4> tile_uv(int index);

// One 16x16 image per tile index, world tiles and item tiles.
std::map<int, Image> make_tiles(int seed = 1234);
// The building blocks' tiles (building_art.cpp), the sign font and the paintings; part of make_tiles.
std::map<int, Image> make_building_tiles();
// The Nether's tiles (nether_art.cpp): portal, ghast, fireball, zombie pigman; part of make_tiles.
std::map<int, Image> make_nether_tiles();
Image make_atlas(const std::map<int, Image>& tiles);

// Rain streaks and snowflakes, drawn repeating on the columns of falling weather.
Image make_rain_image();
Image make_snow_image();

// HUD icons: heart_full, heart_half, heart_empty, food_full, food_half, food_empty, armor_*, bubble (9x8 each).
std::map<std::string, Image> make_hud_icons();
