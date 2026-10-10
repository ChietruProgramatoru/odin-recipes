#include "blocks.h"

#include "icons.h"

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
};

}  // namespace

const BlockInfo* block_info(ItemId id) {
    if (id <= AIR || id >= BLOCK_COUNT) return nullptr;
    return &BLOCKS[id];
}

bool is_solid(BlockId id) { return id != AIR && id != WATER; }
bool is_opaque(BlockId id) { return id != AIR && id != WATER; }
bool is_block(ItemId id) { return block_info(id) != nullptr; }
