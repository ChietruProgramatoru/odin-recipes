// Block and item definitions.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using BlockId = uint8_t;
using ItemId = int;

enum Block : BlockId {
    AIR, GRASS, DIRT, STONE, COBBLE, SAND, LOG, LEAVES, PLANKS, BEDROCK, WATER, BRICK,
    BLOCK_COUNT
};

// Food items live above the block id range.
enum Food : ItemId { APPLE = 100, BEEF = 101 };

// Atlas tile indices (see textures.h).
enum Tile : int {
    T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE, T_SAND, T_LOG_SIDE, T_LOG_TOP,
    T_LEAVES, T_PLANKS, T_BEDROCK, T_WATER, T_BRICK,
    // Tiles for cows, dropped food and the block outline.
    T_COW_HIDE, T_WHITE, T_HOOF, T_COW_HEAD, T_SNOUT, T_BLACK, T_HORN, T_APPLE, T_BEEF,
    // Solid leaves for Fast graphics, then the player model.
    T_LEAVES_FAST, T_SKIN, T_HAIR, T_SHIRT, T_PANTS, T_SHOE, T_EYE,
    TILE_COUNT
};

struct BlockInfo {
    const char* name;
    int tiles[3];                 // tile index for top, sides, bottom
    std::optional<float> hardness; // seconds to break by hand (empty = unbreakable)
    std::optional<ItemId> drop;    // item dropped when broken (empty = nothing)
};

struct FoodInfo {
    int hunger;
    float saturation;
};

// Info for a real block (GRASS..BRICK); nullptr for AIR and anything else.
const BlockInfo* block_info(ItemId id);
const FoodInfo* food_info(ItemId id);
std::string item_name(ItemId id);

bool is_solid(BlockId id);  // can't walk through
bool is_block(ItemId id);   // placeable block (not food)
bool is_opaque(BlockId id); // hides the faces behind it
int icon_tile(ItemId id);   // tile used to draw an item in the hotbar and as a dropped item

// Every item that has a model or icon (all blocks, then the foods).
const std::vector<ItemId>& all_items();
// Items shown in the creative hotbar.
const std::vector<ItemId>& creative_items();
