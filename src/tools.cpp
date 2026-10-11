#include "tools.h"

#include <algorithm>

namespace {

enum ToolOrder { SWORD, SHOVEL, PICKAXE, AXE, HOE };  // the order of each material's tools in items.h

// Blocks each tool digs fast (Beta's lists, for the blocks this game has).
bool pickaxe_block(BlockId b) {
    switch (b) {
        case STONE: case COBBLE: case SANDSTONE: case MOSSY_COBBLE: case ICE: case OBSIDIAN:
        case COAL_ORE: case IRON_ORE: case GOLD_ORE: case DIAMOND_ORE: case LAPIS_ORE:
            return true;
        default: return false;
    }
}
bool shovel_block(BlockId b) {
    return b == GRASS || b == DIRT || b == SAND || b == GRAVEL || b == CLAY_BLOCK || b == FARMLAND || b == SNOW_LAYER;
}
bool axe_block(BlockId b) { return b == PLANKS || b == LOG || b == BIRCH_LOG || b == SPRUCE_LOG || b == CHEST; }

// Stone-like blocks drop nothing unless mined with a pickaxe of at least this level.
std::optional<int> pickaxe_needed(BlockId b) {
    if (is_furnace(b)) return 0;
    switch (b) {
        case STONE: case COBBLE: case BRICK: case SANDSTONE: case MOSSY_COBBLE: case SPAWNER:
        case COAL_ORE:
            return 0;
        case IRON_ORE: case LAPIS_ORE: return 1;
        case GOLD_ORE: case DIAMOND_ORE: case REDSTONE_ORE: return 2;
        case OBSIDIAN: return 3;
        default: return std::nullopt;
    }
}

}  // namespace

ToolKind tool_kind(ItemId item) {
    if (item < WOOD_SWORD || item > DIAMOND_HOE) return ToolKind::None;
    return static_cast<ToolKind>((item - WOOD_SWORD) % 5 + 1);
}

std::optional<Material> tool_material(ItemId item) {
    if (tool_kind(item) == ToolKind::None) return std::nullopt;
    return static_cast<Material>((item - WOOD_SWORD) / 5);
}

int harvest_level(Material m) {
    switch (m) {
        case Material::Stone: return 1;
        case Material::Iron: return 2;
        case Material::Diamond: return 3;
        default: return 0;  // wood and gold
    }
}

float dig_efficiency(Material m) {
    switch (m) {
        case Material::Wood: return 2;
        case Material::Stone: return 4;
        case Material::Iron: return 6;
        case Material::Gold: return 12;
        case Material::Diamond: return 8;
    }
    return 1;
}

bool can_harvest(ItemId held, BlockId block) {
    auto level = pickaxe_needed(block);
    if (!level) return true;
    return tool_kind(held) == ToolKind::Pickaxe && harvest_level(*tool_material(held)) >= *level;
}

float dig_speed(ItemId held, BlockId block) {
    if (held == SHEARS && (is_leaves(block) || block == COBWEB)) return 15;
    if (block == COBWEB && tool_kind(held) == ToolKind::Sword) return 15;  // Beta: swords cut through webs
    switch (tool_kind(held)) {
        case ToolKind::Sword: return 1.5f;
        case ToolKind::Pickaxe: return pickaxe_block(block) ? dig_efficiency(*tool_material(held)) : 1;
        case ToolKind::Shovel: return shovel_block(block) ? dig_efficiency(*tool_material(held)) : 1;
        case ToolKind::Axe: return axe_block(block) ? dig_efficiency(*tool_material(held)) : 1;
        default: return 1;
    }
}

std::optional<double> break_seconds(ItemId held, BlockId block) {
    const BlockInfo* info = block_info(block);
    if (!info || !info->hardness) return std::nullopt;
    const double hardness = *info->hardness;
    if (hardness <= 0) return 0.0;
    double ticks = can_harvest(held, block) ? hardness * 30 / dig_speed(held, block) : hardness * 100;
    return ticks / 20;
}

std::optional<ItemId> harvest_drop(ItemId held, BlockId block) {
    const BlockInfo* info = block_info(block);
    if (!info || !can_harvest(held, block)) return std::nullopt;
    if (held == SHEARS && is_leaves(block)) return block;
    return info->drop;
}

int attack_damage(ItemId held) {
    auto m = tool_material(held);
    if (!m) return 1;
    int bonus = 0;  // Beta's per-material damage bonus
    switch (*m) {
        case Material::Stone: bonus = 1; break;
        case Material::Iron: bonus = 2; break;
        case Material::Diamond: bonus = 3; break;
        default: break;
    }
    switch (tool_kind(held)) {
        case ToolKind::Sword: return 4 + bonus * 2;
        case ToolKind::Shovel: return 1 + bonus;
        case ToolKind::Pickaxe: return 2 + bonus;
        case ToolKind::Axe: return 3 + bonus;
        default: return 1;  // the hoe hits like a fist
    }
}

int wear_from_breaking(ItemId held, BlockId block) {
    if (held == SHEARS) return is_leaves(block) || block == COBWEB ? 1 : 0;
    switch (tool_kind(held)) {
        case ToolKind::Sword: return 2;
        case ToolKind::Shovel: case ToolKind::Pickaxe: case ToolKind::Axe: return 1;
        default: return 0;
    }
}

int wear_from_hit(ItemId held) {
    switch (tool_kind(held)) {
        case ToolKind::Sword: return 1;
        case ToolKind::Shovel: case ToolKind::Pickaxe: case ToolKind::Axe: return 2;
        default: return 0;
    }
}

bool wear_out(Stack& stack, int uses) {
    const int most = max_damage(stack.item);
    if (most <= 0 || uses <= 0) return false;
    stack.damage += uses;
    return stack.damage > most;
}

// --- Armor ------------------------------------------------------------------------------------

int armor_points(ItemId item) {
    static const int POINTS[4] = {3, 8, 6, 3};
    int slot = armor_slot(item);
    return slot < 0 ? 0 : POINTS[slot];
}

int armor_value(const Inventory& inv) {
    int points = 0, left = 0, total = 0;
    for (const auto& piece : inv.armor) {
        if (!piece || armor_slot(piece->item) < 0) continue;
        const int most = max_damage(piece->item);
        left += most - piece->damage;
        total += most;
        points += armor_points(piece->item);
    }
    if (total == 0) return 0;
    return (points - 1) * left / total + 1;
}

int absorb_with_armor(Inventory& inv, int damage, int& remainder) {
    if (damage <= 0) return 0;
    const int scaled = damage * (25 - armor_value(inv)) + remainder;
    for (auto& piece : inv.armor)
        if (piece && wear_out(*piece, damage)) piece.reset();
    remainder = scaled % 25;
    return scaled / 25;
}
