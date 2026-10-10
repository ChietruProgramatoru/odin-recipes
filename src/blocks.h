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
    CRAFTING_TABLE, CHEST, FURNACE,
    GLOWSTONE, TORCH,
    // Torches on a wall, named by the side of the torch's cell the wall is on. They drop and
    // show as TORCH; placing a torch on a wall picks one of these.
    TORCH_WALL_WEST, TORCH_WALL_EAST, TORCH_WALL_NORTH, TORCH_WALL_SOUTH,
    // World generation (M4).
    GRAVEL, CLAY_BLOCK, SANDSTONE, COAL_ORE, IRON_ORE, GOLD_ORE, DIAMOND_ORE, REDSTONE_ORE, LAPIS_ORE,
    MOSSY_COBBLE, SPAWNER, LAVA, ICE, BIRCH_LOG, SPRUCE_LOG, BIRCH_LEAVES, SPRUCE_LEAVES,
    // Beds (M5): the foot and head halves, named by the way the head points from the foot.
    // Placed from the BED item (see items.h); breaking either half removes both and drops one bed.
    BED_FOOT_SOUTH, BED_FOOT_WEST, BED_FOOT_NORTH, BED_FOOT_EAST,
    BED_HEAD_SOUTH, BED_HEAD_WEST, BED_HEAD_NORTH, BED_HEAD_EAST,
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
    // The torch in the world (its inventory icon is in icons.h).
    T_TORCH,
    // World generation blocks.
    // (Sandstone and clay use the item table's tiles, see icons.h.)
    T_GRAVEL, T_COAL_ORE, T_IRON_ORE, T_GOLD_ORE,
    T_DIAMOND_ORE, T_REDSTONE_ORE, T_LAPIS_ORE, T_MOSSY_COBBLE, T_SPAWNER, T_LAVA, T_ICE, T_BIRCH_LOG_SIDE,
    T_SPRUCE_LOG_SIDE, T_SPRUCE_LEAVES, T_SPRUCE_LEAVES_FAST,
    // Grass and leaves tiles are grey and tinted by biome in the world (see BlockTint). These are
    // the same tiles coloured in, for hotbar icons and dropped items.
    T_GRASS_TOP_ICON, T_GRASS_ICON, T_LEAVES_ICON, T_BIRCH_LEAVES_ICON, T_SPRUCE_LEAVES_ICON,
    // Beds: the top of each half (the head end of the picture points to the head), the long sides
    // and the two ends. The bottom 3 rows of the side and end tiles are see-through except the legs.
    T_BED_TOP_FOOT, T_BED_TOP_HEAD, T_BED_SIDE_FOOT, T_BED_SIDE_HEAD, T_BED_END_FOOT, T_BED_END_HEAD,
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

// Info for a real block (GRASS and up); nullptr for AIR and anything else.
const BlockInfo* block_info(ItemId id);

bool is_solid(BlockId id);  // can't walk through
bool is_block(ItemId id);   // placeable block
bool is_opaque(BlockId id); // hides the faces behind it
bool is_fluid(BlockId id);  // water or lava: you can't target or break it
bool is_leaves(BlockId id);
// Drawn see-through in the solid pass (alpha-tested), so neighbouring faces stay visible.
bool is_cutout(BlockId id);
bool is_torch(BlockId id);  // floor or wall torch (drawn as a thin stick, not a cube)
bool is_bed(BlockId id);    // either half of a bed
// How tall a solid block is for walking on it: 1, or 9/16 for a bed.
float block_height(BlockId id);
// Light (0-15): how much a block dims light passing into it, and how much it gives off.
int light_opacity(BlockId id);
int light_emission(BlockId id);

// Item names, icons, stack sizes and the rest are in items.h.
