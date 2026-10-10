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
    // World generation (M4).
    GRAVEL, CLAY, SANDSTONE, COAL_ORE, IRON_ORE, GOLD_ORE, DIAMOND_ORE, REDSTONE_ORE, LAPIS_ORE,
    MOSSY_COBBLE, SPAWNER, LAVA, ICE, BIRCH_LOG, SPRUCE_LOG, BIRCH_LEAVES, SPRUCE_LEAVES,
    BLOCK_COUNT
};

// Food items live above the block id range.
enum Food : ItemId { APPLE = 100, BEEF = 101 };
// Materials dropped by ores and clay.
enum Material : ItemId { COAL = 110, DIAMOND = 111, REDSTONE = 112, LAPIS = 113, CLAY_BALL = 114 };

// Atlas tile indices (see textures.h).
enum Tile : int {
    T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE, T_SAND, T_LOG_SIDE, T_LOG_TOP,
    T_LEAVES, T_PLANKS, T_BEDROCK, T_WATER, T_BRICK,
    // Tiles for cows, dropped food and the block outline.
    T_COW_HIDE, T_WHITE, T_HOOF, T_COW_HEAD, T_SNOUT, T_BLACK, T_HORN, T_APPLE, T_BEEF,
    // Solid leaves for Fast graphics, then the player model.
    T_LEAVES_FAST, T_SKIN, T_HAIR, T_SHIRT, T_PANTS, T_SHOE, T_EYE,
    // World generation blocks.
    T_GRAVEL, T_CLAY, T_SANDSTONE_TOP, T_SANDSTONE_SIDE, T_SANDSTONE_BOTTOM, T_COAL_ORE, T_IRON_ORE, T_GOLD_ORE,
    T_DIAMOND_ORE, T_REDSTONE_ORE, T_LAPIS_ORE, T_MOSSY_COBBLE, T_SPAWNER, T_LAVA, T_ICE, T_BIRCH_LOG_SIDE,
    T_SPRUCE_LOG_SIDE, T_SPRUCE_LEAVES, T_SPRUCE_LEAVES_FAST,
    // Grass and leaves tiles are grey and tinted by biome in the world (see BlockTint). These are
    // the same tiles coloured in, for hotbar icons and dropped items.
    T_GRASS_ICON, T_LEAVES_ICON, T_BIRCH_LEAVES_ICON, T_SPRUCE_LEAVES_ICON,
    // Items.
    T_COAL, T_DIAMOND, T_REDSTONE, T_LAPIS, T_CLAY_BALL,
    TILE_COUNT
};

struct BlockInfo {
    const char* name;
    int tiles[3];                 // tile index for top, sides, bottom
    std::optional<float> hardness; // seconds to break by hand (empty = unbreakable)
    std::optional<ItemId> drop;    // item dropped when broken (empty = nothing)
    int drop_min = 1, drop_max = 1; // how many are dropped
};

// How a block's grey pixels are coloured in the world (only atlas pixels marked as tintable).
enum class BlockTint { NONE, GRASS, FOLIAGE, BIRCH, SPRUCE };
BlockTint block_tint(BlockId id);

struct FoodInfo {
    int hunger;
    float saturation;
};

// Info for a real block (GRASS..SPRUCE_LEAVES); nullptr for AIR and anything else.
const BlockInfo* block_info(ItemId id);
const FoodInfo* food_info(ItemId id);
std::string item_name(ItemId id);

bool is_solid(BlockId id);  // can't walk through
bool is_block(ItemId id);   // placeable block (not food or a material)
bool is_opaque(BlockId id); // hides the faces behind it
bool is_fluid(BlockId id);  // water or lava: you can't target or break it
bool is_leaves(BlockId id);
// Drawn see-through in the solid pass (alpha-tested), so neighbouring faces stay visible.
bool is_cutout(BlockId id);
int icon_tile(ItemId id);   // tile used to draw an item in the hotbar and as a dropped item

// Every item that has a model or icon (all blocks, then the foods and materials).
const std::vector<ItemId>& all_items();
// Items shown in the creative hotbar.
const std::vector<ItemId>& creative_items();
