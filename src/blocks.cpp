#include "blocks.h"

#include "icons.h"
#include "items.h"

namespace {

const BlockInfo BLOCKS[BLOCK_COUNT] = {
    {"Air", {0, 0, 0}, std::nullopt, std::nullopt},
    {"Grass", {T_GRASS_TOP, T_GRASS_SIDE, T_DIRT}, 0.6f, DIRT},
    {"Dirt", {T_DIRT, T_DIRT, T_DIRT}, 0.5f, DIRT},
    {"Stone", {T_STONE, T_STONE, T_STONE}, 1.5f, COBBLE},
    {"Cobblestone", {T_COBBLE, T_COBBLE, T_COBBLE}, 2.0f, COBBLE},
    {"Sand", {T_SAND, T_SAND, T_SAND}, 0.5f, SAND},
    {"Oak Log", {T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP}, 2.0f, LOG},
    {"Leaves", {T_LEAVES, T_LEAVES, T_LEAVES}, 0.2f, std::nullopt},
    {"Oak Planks", {T_PLANKS, T_PLANKS, T_PLANKS}, 2.0f, PLANKS},
    {"Bedrock", {T_BEDROCK, T_BEDROCK, T_BEDROCK}, std::nullopt, std::nullopt},
    {"Water", {T_WATER, T_WATER, T_WATER}, std::nullopt, std::nullopt},
    {"Bricks", {T_BRICK, T_BRICK, T_BRICK}, 2.0f, BRICK},
    {"Crafting Table", {IT_CRAFT_TOP, IT_CRAFT_FRONT, T_PLANKS}, 2.5f, CRAFTING_TABLE},
    {"Chest", {IT_CHEST_TOP, IT_CHEST_FRONT, IT_CHEST_TOP}, 2.5f, CHEST},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Glowstone", {IT_GLOWSTONE, IT_GLOWSTONE, IT_GLOWSTONE}, 0.3f, GLOWSTONE},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Gravel", {T_GRAVEL, T_GRAVEL, T_GRAVEL}, 0.6f, GRAVEL},
    {"Clay", {IT_CLAY, IT_CLAY, IT_CLAY}, 0.6f, CLAY_BALL, 4, 4},
    {"Sandstone", {IT_SANDSTONE_TOP, IT_SANDSTONE_SIDE, IT_SANDSTONE_TOP}, 0.8f, SANDSTONE},
    {"Coal Ore", {T_COAL_ORE, T_COAL_ORE, T_COAL_ORE}, 3.0f, COAL},
    {"Iron Ore", {T_IRON_ORE, T_IRON_ORE, T_IRON_ORE}, 3.0f, IRON_ORE},
    {"Gold Ore", {T_GOLD_ORE, T_GOLD_ORE, T_GOLD_ORE}, 3.0f, GOLD_ORE},
    {"Diamond Ore", {T_DIAMOND_ORE, T_DIAMOND_ORE, T_DIAMOND_ORE}, 3.0f, DIAMOND},
    {"Redstone Ore", {T_REDSTONE_ORE, T_REDSTONE_ORE, T_REDSTONE_ORE}, 3.0f, REDSTONE, 4, 5},
    {"Lapis Lazuli Ore", {T_LAPIS_ORE, T_LAPIS_ORE, T_LAPIS_ORE}, 3.0f, LAPIS_LAZULI, 4, 8},
    {"Mossy Cobblestone", {T_MOSSY_COBBLE, T_MOSSY_COBBLE, T_MOSSY_COBBLE}, 2.0f, MOSSY_COBBLE},
    {"Monster Spawner", {T_SPAWNER, T_SPAWNER, T_SPAWNER}, 5.0f, std::nullopt},
    {"Lava", {T_LAVA, T_LAVA, T_LAVA}, std::nullopt, std::nullopt},
    {"Ice", {T_ICE, T_ICE, T_ICE}, 0.5f, std::nullopt},
    {"Birch Log", {T_LOG_TOP, T_BIRCH_LOG_SIDE, T_LOG_TOP}, 2.0f, BIRCH_LOG},
    {"Spruce Log", {T_LOG_TOP, T_SPRUCE_LOG_SIDE, T_LOG_TOP}, 2.0f, SPRUCE_LOG},
    {"Birch Leaves", {T_LEAVES, T_LEAVES, T_LEAVES}, 0.2f, std::nullopt},
    {"Spruce Leaves", {T_SPRUCE_LEAVES, T_SPRUCE_LEAVES, T_SPRUCE_LEAVES}, 0.2f, std::nullopt},
    // Beds drop nothing themselves: Game::break_block drops one bed for the pair.
    {"Bed", {T_BED_TOP_FOOT, T_BED_SIDE_FOOT, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_FOOT, T_BED_SIDE_FOOT, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_FOOT, T_BED_SIDE_FOOT, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_FOOT, T_BED_SIDE_FOOT, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_HEAD, T_BED_SIDE_HEAD, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_HEAD, T_BED_SIDE_HEAD, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_HEAD, T_BED_SIDE_HEAD, T_PLANKS}, 0.2f, std::nullopt},
    {"Bed", {T_BED_TOP_HEAD, T_BED_SIDE_HEAD, T_PLANKS}, 0.2f, std::nullopt},
    {"Fire", {T_FIRE, T_FIRE, T_FIRE}, 0.0f, std::nullopt},
    // Breaking TNT lights it instead of dropping it (see Game::break_block).
    {"TNT", {IT_TNT_TOP, IT_TNT_SIDE, IT_TNT_TOP}, 0.0f, TNT},
    // The other furnace facings and the lit furnaces. The mesher draws the front on one side.
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_TOP}, 3.5f, FURNACE},
    // Glass drops nothing when broken (Beta).
    {"Glass", {IT_GLASS, IT_GLASS, IT_GLASS}, 0.3f, std::nullopt},
    {"Obsidian", {T_OBSIDIAN, T_OBSIDIAN, T_OBSIDIAN}, 10.0f, OBSIDIAN},
    // Farming. Crops, tall grass and cake work out their drops themselves (farming.h).
    {"Farmland", {T_FARMLAND_DRY, T_DIRT, T_DIRT}, 0.6f, DIRT},
    {"Crops", {T_WHEAT_0, T_WHEAT_0, T_WHEAT_0}, 0.0f, std::nullopt},
    {"Cake", {T_CAKE_TOP, T_CAKE_SIDE, T_CAKE_TOP}, 0.5f, std::nullopt},
    {"Sugar Canes", {T_SUGAR_CANE, T_SUGAR_CANE, T_SUGAR_CANE}, 0.0f, SUGAR_CANE},
    {"Grass", {T_TALL_GRASS, T_TALL_GRASS, T_TALL_GRASS}, 0.0f, std::nullopt},
    {"Fern", {T_FERN, T_FERN, T_FERN}, 0.0f, std::nullopt},
    {"Dandelion", {T_DANDELION, T_DANDELION, T_DANDELION}, 0.0f, YELLOW_FLOWER},
    {"Rose", {T_ROSE, T_ROSE, T_ROSE}, 0.0f, RED_ROSE},
    {"Brown Mushroom", {T_BROWN_MUSHROOM, T_BROWN_MUSHROOM, T_BROWN_MUSHROOM}, 0.0f, BROWN_MUSHROOM},
    {"Red Mushroom", {T_RED_MUSHROOM, T_RED_MUSHROOM, T_RED_MUSHROOM}, 0.0f, RED_MUSHROOM},
    // Nature (M6 part 2).
    {"Oak Sapling", {T_SAPLING, T_SAPLING, T_SAPLING}, 0.0f, SAPLING},
    {"Spruce Sapling", {T_SPRUCE_SAPLING, T_SPRUCE_SAPLING, T_SPRUCE_SAPLING}, 0.0f, SPRUCE_SAPLING},
    {"Birch Sapling", {T_BIRCH_SAPLING, T_BIRCH_SAPLING, T_BIRCH_SAPLING}, 0.0f, BIRCH_SAPLING},
    {"Cactus", {T_CACTUS_TOP, T_CACTUS_SIDE, T_CACTUS_BOTTOM}, 0.4f, CACTUS},
    {"Cobweb", {T_COBWEB, T_COBWEB, T_COBWEB}, 4.0f, STRING},
    {"Snow", {IT_SNOW, IT_SNOW, IT_SNOW}, 0.1f, SNOWBALL},
};

static_assert(TILE_COUNT <= ITEM_TILE_BASE, "world tiles run into the item tiles");

}  // namespace

const BlockInfo* block_info(ItemId id) {
    if (id <= AIR || id >= BLOCK_COUNT) return nullptr;
    return &BLOCKS[id];
}

bool is_fluid(BlockId id) { return id == WATER || id == LAVA; }
bool is_leaves(BlockId id) { return id == LEAVES || id == BIRCH_LEAVES || id == SPRUCE_LEAVES; }
bool is_cutout(BlockId id) { return id == SPAWNER || id == GLASS; }
bool is_torch(BlockId id) { return id >= TORCH && id <= TORCH_WALL_SOUTH; }
bool is_bed(BlockId id) { return id >= BED_FOOT_SOUTH && id <= BED_HEAD_EAST; }
bool is_plant(BlockId id) {
    return id == WHEAT_CROP || (id >= SUGAR_CANE_PLANT && id <= RED_MUSHROOM) || is_sapling(id) || id == COBWEB;
}
bool is_sapling(BlockId id) { return id >= SAPLING && id <= BIRCH_SAPLING; }
BlockId sapling_for(BlockId leaves) {
    if (leaves == SPRUCE_LEAVES) return SPRUCE_SAPLING;
    if (leaves == BIRCH_LEAVES) return BIRCH_SAPLING;
    return SAPLING;
}
bool is_furnace(BlockId id) { return id == FURNACE || (id >= FURNACE_WEST && id <= LIT_FURNACE_EAST); }
bool is_lit_furnace(BlockId id) { return id >= LIT_FURNACE_SOUTH && id <= LIT_FURNACE_EAST; }
int furnace_dir(BlockId id) {
    if (id == FURNACE) return 0;
    if (is_lit_furnace(id)) return id - LIT_FURNACE_SOUTH;
    return id - FURNACE_WEST + 1;
}
BlockId furnace_block(int dir, bool lit) {
    dir &= 3;
    if (lit) return static_cast<BlockId>(LIT_FURNACE_SOUTH + dir);
    return dir == 0 ? static_cast<BlockId>(FURNACE) : static_cast<BlockId>(FURNACE_WEST + dir - 1);
}
float block_height(BlockId id) {
    if (is_bed(id)) return 9.0f / 16;
    if (id == FARMLAND || id == CACTUS) return 15.0f / 16;  // Beta's cactus box is 1/16 in from every side but the bottom
    if (id == CAKE_BLOCK) return 0.5f;
    return 1.0f;
}
bool falls(BlockId id) { return id == SAND || id == GRAVEL; }
bool is_thin(BlockId id) { return is_torch(id) || id == FIRE || is_plant(id) || id == SNOW_LAYER; }
bool is_solid(BlockId id) { return id != AIR && !is_fluid(id) && !is_thin(id); }
bool is_opaque(BlockId id) {
    return id != AIR && !is_fluid(id) && id != ICE && !is_cutout(id) && !is_thin(id) && !is_bed(id) && id != CAKE_BLOCK &&
           id != FARMLAND && id != CACTUS;
}
bool is_block(ItemId id) { return block_info(id) != nullptr; }

BlockTint block_tint(BlockId id) {
    switch (id) {
        case GRASS: case TALL_GRASS: case FERN: return BlockTint::GRASS;
        case LEAVES: return BlockTint::FOLIAGE;
        case BIRCH_LEAVES: return BlockTint::BIRCH;
        case SPRUCE_LEAVES: return BlockTint::SPRUCE;
        default: return BlockTint::NONE;
    }
}

// Beta's values: leaves dim light by 1, water and ice by 3, solid blocks stop it.
int light_opacity(BlockId id) {
    if (id == AIR || is_thin(id) || is_bed(id) || id == SPAWNER || id == GLASS || id == CAKE_BLOCK || id == CACTUS) return 0;
    if (is_leaves(id)) return 1;
    if (id == WATER || id == ICE) return 3;
    return 15;
}

int light_emission(BlockId id) {
    if (id == GLOWSTONE || id == LAVA || id == FIRE) return 15;
    if (is_torch(id)) return 14;
    if (id == BROWN_MUSHROOM) return 1;  // Beta's brown mushrooms glow very faintly
    if (is_lit_furnace(id)) return 13;
    return 0;
}
