// Every item: names, icons, stack limits, durability, food and armor.
//
// Item ids below 256 are blocks (see blocks.h), except the two foods that were made before
// this table existed (APPLE and BEEF keep their ids so old saves load). Other items start at 256.
// Things that Beta places as blocks but this game can't place yet (wool, torches, stairs...) are
// items for now, in their own section; the milestone that makes them placeable turns them into blocks.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "blocks.h"

enum Item : ItemId {
    // Materials
    STICK = 256, COAL, CHARCOAL, DIAMOND, IRON_INGOT, GOLD_INGOT, REDSTONE, GLOWSTONE_DUST,
    STRING, FEATHER, GUNPOWDER, FLINT, LEATHER, CLAY_BALL, CLAY_BRICK, PAPER, BOOK, SLIMEBALL, BONE,
    SUGAR, WHEAT, EGG, SNOWBALL, SUGAR_CANE,
    // Dyes (Beta's 16 colours, in Beta's order)
    INK_SAC, ROSE_RED, CACTUS_GREEN, COCOA_BEANS, LAPIS_LAZULI, PURPLE_DYE, CYAN_DYE, LIGHT_GRAY_DYE,
    GRAY_DYE, PINK_DYE, LIME_DYE, DANDELION_YELLOW, LIGHT_BLUE_DYE, MAGENTA_DYE, ORANGE_DYE, BONE_MEAL,
    // Tools and weapons: sword, shovel, pickaxe, axe, hoe in each material
    WOOD_SWORD, WOOD_SHOVEL, WOOD_PICKAXE, WOOD_AXE, WOOD_HOE,
    STONE_SWORD, STONE_SHOVEL, STONE_PICKAXE, STONE_AXE, STONE_HOE,
    IRON_SWORD, IRON_SHOVEL, IRON_PICKAXE, IRON_AXE, IRON_HOE,
    GOLD_SWORD, GOLD_SHOVEL, GOLD_PICKAXE, GOLD_AXE, GOLD_HOE,
    DIAMOND_SWORD, DIAMOND_SHOVEL, DIAMOND_PICKAXE, DIAMOND_AXE, DIAMOND_HOE,
    BOW, ARROW, FLINT_AND_STEEL, SHEARS, FISHING_ROD,
    // Armor: helmet, chestplate, leggings, boots in each material
    LEATHER_HELMET, LEATHER_CHESTPLATE, LEATHER_LEGGINGS, LEATHER_BOOTS,
    CHAIN_HELMET, CHAIN_CHESTPLATE, CHAIN_LEGGINGS, CHAIN_BOOTS,
    IRON_HELMET, IRON_CHESTPLATE, IRON_LEGGINGS, IRON_BOOTS,
    GOLD_HELMET, GOLD_CHESTPLATE, GOLD_LEGGINGS, GOLD_BOOTS,
    DIAMOND_HELMET, DIAMOND_CHESTPLATE, DIAMOND_LEGGINGS, DIAMOND_BOOTS,
    // Buckets, food and other things
    BUCKET, WATER_BUCKET, LAVA_BUCKET, MILK_BUCKET,
    BOWL, MUSHROOM_STEW, BREAD, COOKIE, CAKE, GOLDEN_APPLE,
    RAW_PORKCHOP, COOKED_PORKCHOP, RAW_FISH, COOKED_FISH,
    COMPASS, CLOCK, MAP, WOOD_DOOR, IRON_DOOR, SIGN, BED, PAINTING, BOAT,
    MINECART, STORAGE_MINECART, POWERED_MINECART, REPEATER, SADDLE,

    // Blocks that later milestones make placeable. Until then they are items.
    FUTURE_BLOCKS = 512,
    WHITE_WOOL = FUTURE_BLOCKS, ORANGE_WOOL, MAGENTA_WOOL, LIGHT_BLUE_WOOL, YELLOW_WOOL, LIME_WOOL, PINK_WOOL,
    GRAY_WOOL, LIGHT_GRAY_WOOL, CYAN_WOOL, PURPLE_WOOL, BLUE_WOOL, BROWN_WOOL, GREEN_WOOL, RED_WOOL, BLACK_WOOL,
    TORCH, REDSTONE_TORCH, GLOWSTONE, JACK_O_LANTERN, PUMPKIN,
    SNOW_BLOCK, GOLD_BLOCK, IRON_BLOCK, DIAMOND_BLOCK, LAPIS_BLOCK,
    BOOKSHELF, TNT, NOTE_BLOCK, JUKEBOX, DISPENSER, PISTON, STICKY_PISTON,
    STONE_SLAB, SANDSTONE_SLAB, WOOD_SLAB, COBBLE_SLAB, WOOD_STAIRS, COBBLE_STAIRS,
    FENCE, LADDER, TRAPDOOR, LEVER, STONE_BUTTON, STONE_PLATE, WOOD_PLATE,
    RAIL, POWERED_RAIL, DETECTOR_RAIL,
    YELLOW_FLOWER, RED_ROSE, BROWN_MUSHROOM, RED_MUSHROOM,
    ITEM_END
};

// Tool materials and armor materials, in the order of the enums above.
enum class Material { Wood, Stone, Iron, Gold, Diamond };
enum class ArmorPiece { Helmet, Chestplate, Leggings, Boots };

// How an item is drawn in the inventory: a flat icon, or a little 3D block.
struct ItemIcon {
    enum Shape { Flat, Cube, Slab, Stairs } shape = Flat;
    int top = 0, left = 0, right = 0;  // atlas tiles; a flat icon only uses `top`
};

struct ItemInfo {
    std::string name;
    int max_stack = 64;
    int max_damage = 0;            // uses before it breaks (0 = doesn't wear out)
    ItemIcon icon;
    std::optional<FoodInfo> food;  // can be eaten
    int armor_slot = -1;           // 0 helmet .. 3 boots, -1 isn't armor
};

const FoodInfo* food_info(ItemId id);
std::string item_name(ItemId id);
int icon_tile(ItemId id);  // the tile for a dropped item's model (a block's side)
// Every item, in the order the creative item picker shows them.
const std::vector<ItemId>& all_items();
// Items in the creative hotbar of a new world.
const std::vector<ItemId>& creative_items();

// Info for any item id, or nullptr for ids that aren't items (AIR, water, bedrock and unknown ids).
const ItemInfo* item_info(ItemId id);
inline bool valid_item(ItemId id) { return item_info(id) != nullptr; }
int max_stack(ItemId id);
int max_damage(ItemId id);
int armor_slot(ItemId id);
// Blocks that can't be placed yet (FUTURE_BLOCKS and up) are still drawn as blocks.
bool looks_like_block(ItemId id);

// Every tool (sword, shovel, pickaxe, axe, hoe) for one material, in that order.
ItemId tool(Material m, int kind);
// Armor uses Wood for leather and Stone for chain.
ItemId armor(Material m, ArmorPiece piece);
