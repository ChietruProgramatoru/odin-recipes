// Tools, weapons and armor: Beta's rules for mining speed, which tool a block needs,
// attack damage, wear, and how much damage armor takes off. Pure logic, no graphics.
#pragma once
#include <optional>

#include "survival.h"

enum class ToolKind { None, Sword, Shovel, Pickaxe, Axe, Hoe };

// What kind of tool an item is (None for the empty hand, AIR, and anything that isn't a tool).
ToolKind tool_kind(ItemId item);
// The material of a sword, shovel, pickaxe, axe or hoe.
std::optional<Material> tool_material(ItemId item);

// Beta's tool materials: how strong a block they can harvest (wood and gold 0, stone 1,
// iron 2, diamond 3) and how fast they dig the blocks they're made for.
int harvest_level(Material m);
float dig_efficiency(Material m);

// Does this block drop anything when broken with `held` (AIR = bare hand)?
// Stone and ores need a pickaxe; iron and lapis ore need stone or better, gold, diamond and
// redstone ore need iron or better (and obsidian a diamond pickaxe).
bool can_harvest(ItemId held, BlockId block);
// Beta's dig speed: the tool's efficiency on blocks it's made for, otherwise 1.
float dig_speed(ItemId held, BlockId block);
// Seconds to break a block while holding `held`: hardness x 1.5 / speed when the block can be
// harvested, hardness x 5 when it can't (Beta's 30 and 100 ticks per point of hardness).
// 0 breaks at once; nothing for unbreakable blocks.
std::optional<double> break_seconds(ItemId held, BlockId block);
// The item that breaking `block` with `held` drops, if any (shears drop leaves themselves).
std::optional<ItemId> harvest_drop(ItemId held, BlockId block);

// Half-hearts of damage a hit does: 1 by hand, swords 4/6/8/10 (gold 4), other tools a bit more than a fist.
int attack_damage(ItemId held);
// Uses worn off the held item by breaking a block, and by hitting a mob.
int wear_from_breaking(ItemId held, BlockId block);
int wear_from_hit(ItemId held);

// Wear a tool or armor piece by `uses`. Returns true when it breaks (Beta: once its damage
// goes past its durability); the caller removes it.
bool wear_out(Stack& stack, int uses);

// --- Armor ------------------------------------------------------------------------------------

constexpr int ARROW_DAMAGE = 4;

// Armor points of one piece: Beta gives every material the same 3/8/6/3 (helmet..boots),
// only durability differs.
int armor_points(ItemId item);
// The armor bar (0-20) for what's worn: the pieces' points, scaled down as they wear.
int armor_value(const Inventory& inv);

// Damage after armor: each armor point takes 4% off. Wears every worn piece by the full damage,
// removing pieces that break. `remainder` carries the fraction of a point to the next hit, like Beta.
int absorb_with_armor(Inventory& inv, int damage, int& remainder);
