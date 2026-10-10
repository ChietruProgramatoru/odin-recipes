#include "blocks.h"

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
    {"Glowstone", {T_GLOWSTONE, T_GLOWSTONE, T_GLOWSTONE}, 0.45f, GLOWSTONE},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
    {"Torch", {T_TORCH, T_TORCH, T_TORCH}, 0.0f, TORCH},
};

const FoodInfo APPLE_FOOD{4, 2.4f};
const FoodInfo BEEF_FOOD{6, 4.0f};

}  // namespace

const BlockInfo* block_info(ItemId id) {
    if (id <= AIR || id >= BLOCK_COUNT) return nullptr;
    return &BLOCKS[id];
}

const FoodInfo* food_info(ItemId id) {
    if (id == APPLE) return &APPLE_FOOD;
    if (id == BEEF) return &BEEF_FOOD;
    return nullptr;
}

std::string item_name(ItemId id) {
    if (id == APPLE) return "Apple";
    if (id == BEEF) return "Beef";
    if (const BlockInfo* b = block_info(id)) return b->name;
    return "";
}

bool is_torch(BlockId id) { return id >= TORCH && id <= TORCH_WALL_SOUTH; }
bool is_solid(BlockId id) { return id != AIR && id != WATER && !is_torch(id); }
bool is_opaque(BlockId id) { return id != AIR && id != WATER && !is_torch(id); }

// Beta's values: leaves dim light by 1, water by 3, solid blocks stop it.
int light_opacity(BlockId id) {
    if (id == AIR || is_torch(id)) return 0;
    if (id == LEAVES) return 1;
    if (id == WATER) return 3;
    return 15;
}

int light_emission(BlockId id) {
    if (id == GLOWSTONE) return 15;
    if (is_torch(id)) return 14;
    return 0;
}
bool is_block(ItemId id) { return block_info(id) != nullptr; }

int icon_tile(ItemId id) {
    if (id == APPLE) return T_APPLE;
    if (id == BEEF) return T_BEEF;
    return block_info(id)->tiles[1];
}

const std::vector<ItemId>& all_items() {
    static const std::vector<ItemId> items = [] {
        std::vector<ItemId> v;
        for (int b = GRASS; b <= TORCH; ++b) v.push_back(b);
        v.push_back(APPLE);
        v.push_back(BEEF);
        return v;
    }();
    return items;
}

const std::vector<ItemId>& creative_items() {
    static const std::vector<ItemId> items = {GRASS, DIRT, STONE, COBBLE, SAND, LOG, PLANKS, TORCH, GLOWSTONE};
    return items;
}
