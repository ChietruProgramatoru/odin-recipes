// Procedurally generated pixel-art textures, so the game ships without image files.
#pragma once
#include <array>
#include <map>
#include <string>

#include "image.h"

constexpr int TILE = 16;
constexpr int ATLAS_COLS = 8;
constexpr int ATLAS_ROWS = 4;

// UV rectangle (u0, v0, u1, v1) of an atlas tile. The atlas is uploaded so v = 1 is its top row.
std::array<float, 4> tile_uv(int index);

// One 16x16 image per tile index.
std::map<int, Image> make_tiles(int seed = 1234);
Image make_atlas(const std::map<int, Image>& tiles);

// HUD icons: heart_full, heart_half, heart_empty, food_full, food_half, food_empty (9x8 each).
std::map<std::string, Image> make_hud_icons();
