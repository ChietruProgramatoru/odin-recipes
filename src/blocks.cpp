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
    {"Gravel", {T_GRAVEL, T_GRAVEL, T_GRAVEL}, 0.6f, GRAVEL},
    {"Clay", {T_CLAY, T_CLAY, T_CLAY}, 0.6f, CLAY_BALL, 4, 4},
    {"Sandstone", {T_SANDSTONE_TOP, T_SANDSTONE_SIDE, T_SANDSTONE_BOTTOM}, 0.8f, SANDSTONE},
    {"Coal Ore", {T_COAL_ORE, T_COAL_ORE, T_COAL_ORE}, 3.0f, COAL},
    {"Iron Ore", {T_IRON_ORE, T_IRON_ORE, T_IRON_ORE}, 3.0f, IRON_ORE},
    {"Gold Ore", {T_GOLD_ORE, T_GOLD_ORE, T_GOLD_ORE}, 3.0f, GOLD_ORE},
    {"Diamond Ore", {T_DIAMOND_ORE, T_DIAMOND_ORE, T_DIAMOND_ORE}, 3.0f, DIAMOND},
    {"Redstone Ore", {T_REDSTONE_ORE, T_REDSTONE_ORE, T_REDSTONE_ORE}, 3.0f, REDSTONE, 4, 5},
    {"Lapis Lazuli Ore", {T_LAPIS_ORE, T_LAPIS_ORE, T_LAPIS_ORE}, 3.0f, LAPIS, 4, 8},
    {"Mossy Cobblestone", {T_MOSSY_COBBLE, T_MOSSY_COBBLE, T_MOSSY_COBBLE}, 2.0f, MOSSY_COBBLE},
    {"Monster Spawner", {T_SPAWNER, T_SPAWNER, T_SPAWNER}, 5.0f, std::nullopt},
    {"Lava", {T_LAVA, T_LAVA, T_LAVA}, std::nullopt, std::nullopt},
    {"Ice", {T_ICE, T_ICE, T_ICE}, 0.5f, std::nullopt},
    {"Birch Log", {T_LOG_TOP, T_BIRCH_LOG_SIDE, T_LOG_TOP}, 1.5f, BIRCH_LOG},
    {"Spruce Log", {T_LOG_TOP, T_SPRUCE_LOG_SIDE, T_LOG_TOP}, 1.5f, SPRUCE_LOG},
    {"Birch Leaves", {T_LEAVES, T_LEAVES, T_LEAVES}, 0.2f, std::nullopt},
    {"Spruce Leaves", {T_SPRUCE_LEAVES, T_SPRUCE_LEAVES, T_SPRUCE_LEAVES}, 0.2f, std::nullopt},
};

struct MaterialInfo {
    ItemId id;
    const char* name;
    int tile;
};
const MaterialInfo MATERIALS[] = {
    {COAL, "Coal", T_COAL}, {DIAMOND, "Diamond", T_DIAMOND}, {REDSTONE, "Redstone", T_REDSTONE},
    {LAPIS, "Lapis Lazuli", T_LAPIS}, {CLAY_BALL, "Clay", T_CLAY_BALL},
};

const MaterialInfo* material_info(ItemId id) {
    for (const MaterialInfo& m : MATERIALS)
        if (m.id == id) return &m;
    return nullptr;
}

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
    if (const MaterialInfo* m = material_info(id)) return m->name;
    if (const BlockInfo* b = block_info(id)) return b->name;
    return "";
}

bool is_fluid(BlockId id) { return id == WATER || id == LAVA; }
bool is_leaves(BlockId id) { return id == LEAVES || id == BIRCH_LEAVES || id == SPRUCE_LEAVES; }
bool is_cutout(BlockId id) { return id == SPAWNER; }
bool is_solid(BlockId id) { return id != AIR && !is_fluid(id); }
bool is_opaque(BlockId id) { return id != AIR && !is_fluid(id) && id != ICE && !is_cutout(id); }
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

int icon_tile(ItemId id) {
    if (id == APPLE) return T_APPLE;
    if (id == BEEF) return T_BEEF;
    if (const MaterialInfo* m = material_info(id)) return m->tile;
    switch (id) {
        case GRASS: return T_GRASS_ICON;
        case LEAVES: return T_LEAVES_ICON;
        case BIRCH_LEAVES: return T_BIRCH_LEAVES_ICON;
        case SPRUCE_LEAVES: return T_SPRUCE_LEAVES_ICON;
        default: return block_info(id)->tiles[1];
    }
}

const std::vector<ItemId>& all_items() {
    static const std::vector<ItemId> items = [] {
        std::vector<ItemId> v;
        for (int b = GRASS; b < BLOCK_COUNT; ++b) v.push_back(b);
        v.push_back(APPLE);
        v.push_back(BEEF);
        for (const MaterialInfo& m : MATERIALS) v.push_back(m.id);
        return v;
    }();
    return items;
}

const std::vector<ItemId>& creative_items() {
    static const std::vector<ItemId> items = {GRASS, DIRT, STONE, COBBLE, SAND, LOG, PLANKS, BRICK, LEAVES};
    return items;
}
