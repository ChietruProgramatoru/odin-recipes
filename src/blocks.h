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
    // Tools (M3): fire lit with flint and steel. It burns out by itself; spreading comes in M6.
    FIRE,
    // Monsters (M10): TNT, lit by breaking it, flint and steel or another explosion.
    TNT,
    // Smelting (M7): FURNACE above faces south (its front towards +z); these are the other
    // facings, then lit furnaces in all four. They all drop FURNACE. Glass comes out of a furnace.
    FURNACE_WEST, FURNACE_NORTH, FURNACE_EAST,
    LIT_FURNACE_SOUTH, LIT_FURNACE_WEST, LIT_FURNACE_NORTH, LIT_FURNACE_EAST,
    GLASS,
    // Fluids (M6): where flowing lava meets water. Needs a diamond pickaxe.
    OBSIDIAN,
    // Farming (M8). The data value (see World::get_data) holds farmland wetness (0 dry .. 7 wet),
    // the wheat stage (0 .. 7 ripe), how many slices of a cake are eaten (0 .. 5) and the age of
    // the top of a sugar cane (0 .. 15). Plants are drawn as crossed sheets (is_plant).
    FARMLAND, WHEAT_CROP, CAKE_BLOCK, SUGAR_CANE_PLANT, TALL_GRASS, FERN,
    YELLOW_FLOWER, RED_ROSE, BROWN_MUSHROOM, RED_MUSHROOM,
    // Nature (M6 part 2). Saplings are plants that grow into their tree (the data value's 8 bit says
    // the next growth tick grows it). Cactus grows on sand like sugar cane (data: age 0 .. 15).
    // Cobwebs slow anything in them. Snow layers lie on the ground in cold biomes and melt in light.
    // Leaves use their data value too: LEAVES_PLAYER (never decay), LEAVES_CHECK (see nature.h).
    SAPLING, SPRUCE_SAPLING, BIRCH_SAPLING, CACTUS, COBWEB, SNOW_LAYER,
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
    // Fire, then armor worn on the player model: leather, chain, iron, gold, diamond.
    T_FIRE, T_ARMOR_LEATHER, T_ARMOR_CHAIN, T_ARMOR_IRON, T_ARMOR_GOLD, T_ARMOR_DIAMOND,
    // Monsters (M10) and the smoke of explosions.
    T_ZOMBIE_SKIN, T_ZOMBIE_SHIRT, T_ZOMBIE_PANTS, T_BONE, T_CREEPER, T_SPIDER, T_SPIDER_EYE, T_SLIME, T_SMOKE,
    // Fluids (M6).
    T_OBSIDIAN,
    // Farming (M8): farmland, wheat in its 8 stages, plants (drawn as crossed sheets) and cake.
    T_FARMLAND_DRY, T_FARMLAND_WET, T_WHEAT_0, T_WHEAT_7 = T_WHEAT_0 + 7, T_SUGAR_CANE, T_TALL_GRASS, T_FERN,
    T_DANDELION, T_ROSE, T_BROWN_MUSHROOM, T_RED_MUSHROOM, T_CAKE_TOP, T_CAKE_SIDE, T_CAKE_INSIDE,
    // Animals (M9). Sheep's wool uses the wool item tiles (see icons.h).
    T_PIG, T_PIG_SNOUT, T_SADDLE, T_SHEEP_SKIN, T_SHEEP_FACE, T_BEAK, T_WATTLE, T_SQUID, T_WOLF, T_WOLF_DARK,
    // Nature (M6 part 2). The snow layer uses the snow block's tile (IT_SNOW, icons.h).
    T_SAPLING, T_SPRUCE_SAPLING, T_BIRCH_SAPLING, T_CACTUS_TOP, T_CACTUS_SIDE, T_CACTUS_BOTTOM, T_COBWEB,
    T_GRASS_SNOW_SIDE,
    TILE_COUNT
};

struct BlockInfo {
    const char* name;
    int tiles[3];                 // tile index for top, sides, bottom
    std::optional<float> hardness; // Beta's hardness; break time also depends on the tool, see tools.h (empty = unbreakable)
    std::optional<ItemId> drop;    // item dropped when broken with the right tool (empty = nothing)
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
// A fluid's data value: 0 is a source, 1-7 how far it has flowed from one, 8 and up falling
// (Beta). How much of the cell it fills, from 1/9 to 1 (Beta's BlockFluid.getFluidHeightPercent
// is the part that is air).
inline float fluid_height_percent(int data) { return (data >= 8 ? 1 : data + 1) / 9.0f; }
inline float fluid_fill(int data) { return 1.0f - fluid_height_percent(data); }
bool is_leaves(BlockId id);
// Drawn see-through in the solid pass (alpha-tested), so neighbouring faces stay visible.
bool is_cutout(BlockId id);
bool is_torch(BlockId id);  // floor or wall torch (drawn as a thin stick, not a cube)
// Not a cube and nothing to bump into: torches and fire. Faces next to them are drawn.
bool is_thin(BlockId id);
bool is_bed(BlockId id);    // either half of a bed
// Drawn as crossed sheets and nothing to bump into: wheat, sugar cane, tall grass, ferns, flowers,
// mushrooms, saplings and cobwebs.
bool is_plant(BlockId id);
bool is_sapling(BlockId id);
// The sapling a kind of leaves drops, and the leaves (and log) a sapling grows.
BlockId sapling_for(BlockId leaves);
// Furnaces: any facing, lit or not. The facing is Beta's (0 south, 1 west, 2 north, 3 east, see
// FACING_DIRS in bed.h) and says which way the front points.
bool is_furnace(BlockId id);
bool is_lit_furnace(BlockId id);
int furnace_dir(BlockId furnace);
BlockId furnace_block(int dir, bool lit);
bool falls(BlockId id);     // sand and gravel fall when nothing holds them up
// How tall a solid block is for walking on it: 1, 9/16 for a bed, 15/16 for farmland and cactus, 1/2 for cake.
float block_height(BlockId id);
// Light (0-15): how much a block dims light passing into it, and how much it gives off.
int light_opacity(BlockId id);
int light_emission(BlockId id);

// Item names, icons, stack sizes and the rest are in items.h.
