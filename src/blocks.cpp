#include "blocks.h"

#include "icons.h"
#include "items.h"

namespace {

const BlockInfo BLOCKS[BLOCK_COUNT] = {
    {"Air", {0, 0, 0}, std::nullopt, std::nullopt},
    {"Grass", {T_GRASS_TOP, T_GRASS_SIDE, T_DIRT}, 0.6f, DIRT},
    {"Dirt", {T_DIRT, T_DIRT, T_DIRT}, 0.5f, DIRT},
    {"Stone", {T_STONE, T_STONE, T_STONE}, 1.5f, COBBLE},
    {"Cobblestone", {T_COBBLE, T_COBBLE, T_COBBLE}, 1.6f, COBBLE},
    {"Sand", {T_SAND, T_SAND, T_SAND}, 0.5f, SAND},
    {"Oak Log", {T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP}, 1.5f, LOG},
    {"Leaves", {T_LEAVES, T_LEAVES, T_LEAVES}, 0.2f, std::nullopt},
    {"Oak Planks", {T_PLANKS, T_PLANKS, T_PLANKS}, 1.2f, PLANKS},
    {"Bedrock", {T_BEDROCK, T_BEDROCK, T_BEDROCK}, std::nullopt, std::nullopt},
    {"Water", {T_WATER, T_WATER, T_WATER}, std::nullopt, std::nullopt},
    {"Bricks", {T_BRICK, T_BRICK, T_BRICK}, 2.0f, BRICK},
    {"Crafting Table", {IT_CRAFT_TOP, IT_CRAFT_FRONT, T_PLANKS}, 1.8f, CRAFTING_TABLE},
    {"Chest", {IT_CHEST_TOP, IT_CHEST_FRONT, IT_CHEST_TOP}, 1.8f, CHEST},
    {"Furnace", {IT_FURNACE_TOP, IT_FURNACE_FRONT, IT_FURNACE_TOP}, 2.4f, FURNACE},
    {"Glowstone", {IT_GLOWSTONE, IT_GLOWSTONE, IT_GLOWSTONE}, 0.45f, GLOWSTONE},
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
    {"Birch Log", {T_LOG_TOP, T_BIRCH_LOG_SIDE, T_LOG_TOP}, 1.5f, BIRCH_LOG},
    {"Spruce Log", {T_LOG_TOP, T_SPRUCE_LOG_SIDE, T_LOG_TOP}, 1.5f, SPRUCE_LOG},
    {"Birch Leaves", {T_LEAVES, T_LEAVES, T_LEAVES}, 0.2f, std::nullopt},
    {"Spruce Leaves", {T_SPRUCE_LEAVES, T_SPRUCE_LEAVES, T_SPRUCE_LEAVES}, 0.2f, std::nullopt},
};

}  // namespace

const BlockInfo* block_info(ItemId id) {
    if (id <= AIR || id >= BLOCK_COUNT) return nullptr;
    return &BLOCKS[id];
}

bool is_fluid(BlockId id) { return id == WATER || id == LAVA; }
bool is_leaves(BlockId id) { return id == LEAVES || id == BIRCH_LEAVES || id == SPRUCE_LEAVES; }
bool is_cutout(BlockId id) { return id == SPAWNER; }
bool is_torch(BlockId id) { return id >= TORCH && id <= TORCH_WALL_SOUTH; }
bool is_solid(BlockId id) { return id != AIR && !is_fluid(id) && !is_torch(id); }
bool is_opaque(BlockId id) { return id != AIR && !is_fluid(id) && id != ICE && !is_cutout(id) && !is_torch(id); }
bool is_block(ItemId id) { return block_info(id) != nullptr; }

BlockTint block_tint(BlockId id) {
    switch (id) {
        case GRASS: return BlockTint::GRASS;
        case LEAVES: return BlockTint::FOLIAGE;
        case BIRCH_LEAVES: return BlockTint::BIRCH;
        case SPRUCE_LEAVES: return BlockTint::SPRUCE;
        default: return BlockTint::NONE;
    }
}

// Beta's values: leaves dim light by 1, water and ice by 3, solid blocks stop it.
int light_opacity(BlockId id) {
    if (id == AIR || is_torch(id) || id == SPAWNER) return 0;
    if (is_leaves(id)) return 1;
    if (id == WATER || id == ICE) return 3;
    return 15;
}

int light_emission(BlockId id) {
    if (id == GLOWSTONE || id == LAVA) return 15;
    if (is_torch(id)) return 14;
    return 0;
}
