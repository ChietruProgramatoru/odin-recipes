#include "items.h"

#include <array>

#include "icons.h"

namespace {

const char* MATERIAL_NAMES[5] = {"Wooden", "Stone", "Iron", "Gold", "Diamond"};
const char* ARMOR_MATERIALS[5] = {"Leather", "Chain", "Iron", "Gold", "Diamond"};
const char* TOOL_NAMES[5] = {"Sword", "Shovel", "Pickaxe", "Axe", "Hoe"};
const char* ARMOR_NAMES[2][4] = {{"Cap", "Tunic", "Pants", "Boots"}, {"Helmet", "Chestplate", "Leggings", "Boots"}};
// Beta's tool uses per material, and armor: base uses per piece times a material factor.
const int TOOL_USES[5] = {59, 131, 250, 32, 1561};
const int ARMOR_BASE[4] = {11, 16, 15, 13};
const int ARMOR_FACTOR[5] = {3, 6, 12, 6, 24};
const char* COLOR_NAMES[16] = {"White", "Orange", "Magenta", "Light Blue", "Yellow", "Lime", "Pink", "Gray",
                               "Light Gray", "Cyan", "Purple", "Blue", "Brown", "Green", "Red", "Black"};

struct Table {
    std::vector<std::optional<ItemInfo>> items = std::vector<std::optional<ItemInfo>>(ITEM_END);
    std::vector<ItemId> order;  // every item, in the order the creative picker shows them
    int next_flat = IT_FLAT_BASE;

    ItemInfo& add(ItemId id, std::string name, ItemIcon icon, int stack = 64) {
        auto& slot = items[id];
        if (!slot) order.push_back(id);
        slot = ItemInfo{std::move(name), stack, 0, icon, std::nullopt, -1};
        return *slot;
    }
    ItemInfo& flat(ItemId id, std::string name, int stack = 64) {
        return add(id, std::move(name), {ItemIcon::Flat, next_flat++, 0, 0}, stack);
    }
    ItemInfo& cube(ItemId id, std::string name, int top, int left, int right, ItemIcon::Shape shape = ItemIcon::Cube) {
        return add(id, std::move(name), {shape, top, left, right});
    }
    ItemInfo& food(ItemId id, std::string name, int hunger, float saturation, int stack = 64) {
        ItemInfo& info = flat(id, std::move(name), stack);
        info.food = FoodInfo{hunger, saturation};
        return info;
    }
};

Table build() {
    Table t;
    // Real blocks first (not water or lava). The crafting table, chest and furnace get their own icons
    // below; grass and leaves use coloured-in copies of their grey (biome-tinted) tiles.
    for (int b = GRASS; b < BLOCK_COUNT; ++b) {
        // The torch is a flat icon, added below; beds are placed from the BED item.
        // Farmland, crops, sugar cane and cake come from other items; flowers are flat icons below.
        if (b == WATER || b == LAVA || b == FIRE || is_torch(b) || is_bed(b) || (is_furnace(b) && b != FURNACE) ||
            is_plant(b) || b == FARMLAND || b == CAKE_BLOCK || is_rail(b) || b == SNOW_LAYER)
            continue;
        const BlockInfo* info = block_info(b);
        t.cube(b, info->name, info->tiles[0], info->tiles[1], info->tiles[1]);
    }
    t.cube(GRASS, "Grass", T_GRASS_TOP_ICON, T_GRASS_ICON, T_GRASS_ICON);
    t.cube(LEAVES, "Leaves", T_LEAVES_ICON, T_LEAVES_ICON, T_LEAVES_ICON);
    t.cube(BIRCH_LEAVES, "Birch Leaves", T_BIRCH_LEAVES_ICON, T_BIRCH_LEAVES_ICON, T_BIRCH_LEAVES_ICON);
    t.cube(SPRUCE_LEAVES, "Spruce Leaves", T_SPRUCE_LEAVES_ICON, T_SPRUCE_LEAVES_ICON, T_SPRUCE_LEAVES_ICON);
    t.cube(CRAFTING_TABLE, "Crafting Table", IT_CRAFT_TOP, IT_CRAFT_FRONT, IT_CRAFT_SIDE);
    t.cube(CHEST, "Chest", IT_CHEST_TOP, IT_CHEST_FRONT, IT_CHEST_SIDE);
    t.cube(FURNACE, "Furnace", IT_FURNACE_TOP, IT_FURNACE_FRONT, IT_FURNACE_SIDE);

    // Blocks that can't be placed yet.
    for (int c = 0; c < 16; ++c)
        t.cube(WHITE_WOOL + c, std::string(c == 0 ? "" : COLOR_NAMES[c]) + (c == 0 ? "Wool" : " Wool"), IT_WOOL + c,
               IT_WOOL + c, IT_WOOL + c);
    t.cube(PUMPKIN, "Pumpkin", IT_PUMPKIN_TOP, IT_PUMPKIN_FACE, IT_PUMPKIN_SIDE);
    t.cube(JACK_O_LANTERN, "Jack 'o' Lantern", IT_PUMPKIN_TOP, IT_JACK_FACE, IT_PUMPKIN_SIDE);
    t.cube(SNOW_BLOCK, "Snow", IT_SNOW, IT_SNOW, IT_SNOW);
    t.cube(GOLD_BLOCK, "Block of Gold", IT_GOLD_BLOCK, IT_GOLD_BLOCK, IT_GOLD_BLOCK);
    t.cube(IRON_BLOCK, "Block of Iron", IT_IRON_BLOCK, IT_IRON_BLOCK, IT_IRON_BLOCK);
    t.cube(DIAMOND_BLOCK, "Block of Diamond", IT_DIAMOND_BLOCK, IT_DIAMOND_BLOCK, IT_DIAMOND_BLOCK);
    t.cube(LAPIS_BLOCK, "Lapis Lazuli Block", IT_LAPIS_BLOCK, IT_LAPIS_BLOCK, IT_LAPIS_BLOCK);
    t.cube(BOOKSHELF, "Bookshelf", T_PLANKS, IT_BOOKSHELF, IT_BOOKSHELF);
    t.cube(NOTE_BLOCK, "Note Block", IT_NOTE_BLOCK, IT_NOTE_BLOCK, IT_NOTE_BLOCK);
    t.cube(JUKEBOX, "Jukebox", IT_JUKEBOX_TOP, IT_NOTE_BLOCK, IT_NOTE_BLOCK);
    t.cube(DISPENSER, "Dispenser", IT_FURNACE_TOP, IT_DISPENSER_FRONT, IT_FURNACE_SIDE);
    t.cube(PISTON, "Piston", IT_PISTON_TOP, IT_PISTON_SIDE, IT_PISTON_SIDE);
    t.cube(STICKY_PISTON, "Sticky Piston", IT_STICKY_TOP, IT_PISTON_SIDE, IT_PISTON_SIDE);
    t.cube(STONE_SLAB, "Stone Slab", IT_SLAB_TOP, IT_SLAB_SIDE, IT_SLAB_SIDE, ItemIcon::Slab);
    t.cube(SANDSTONE_SLAB, "Sandstone Slab", IT_SANDSTONE_TOP, IT_SANDSTONE_SIDE, IT_SANDSTONE_SIDE, ItemIcon::Slab);
    t.cube(WOOD_SLAB, "Wooden Slab", T_PLANKS, T_PLANKS, T_PLANKS, ItemIcon::Slab);
    t.cube(COBBLE_SLAB, "Cobblestone Slab", T_COBBLE, T_COBBLE, T_COBBLE, ItemIcon::Slab);
    t.cube(WOOD_STAIRS, "Wooden Stairs", T_PLANKS, T_PLANKS, T_PLANKS, ItemIcon::Stairs);
    t.cube(COBBLE_STAIRS, "Cobblestone Stairs", T_COBBLE, T_COBBLE, T_COBBLE, ItemIcon::Stairs);
    t.flat(TORCH, "Torch");
    t.flat(REDSTONE_TORCH, "Redstone Torch");
    t.flat(FENCE, "Fence");
    t.flat(LADDER, "Ladder");
    t.flat(TRAPDOOR, "Trapdoor");
    t.flat(LEVER, "Lever");
    t.flat(STONE_BUTTON, "Button");
    t.flat(STONE_PLATE, "Stone Pressure Plate");
    t.flat(WOOD_PLATE, "Wooden Pressure Plate");
    t.flat(RAIL, "Rail");
    t.flat(POWERED_RAIL, "Powered Rail");
    t.flat(DETECTOR_RAIL, "Detector Rail");
    t.flat(YELLOW_FLOWER, "Dandelion");
    t.flat(RED_ROSE, "Rose");
    t.flat(BROWN_MUSHROOM, "Brown Mushroom");
    t.flat(RED_MUSHROOM, "Red Mushroom");
    // Saplings and cobwebs show the picture they have in the world.
    for (BlockId b : {SAPLING, SPRUCE_SAPLING, BIRCH_SAPLING, COBWEB}) t.add(b, block_info(b)->name, {ItemIcon::Flat, block_info(b)->tiles[1], 0, 0});

    // Tools, weapons and armor.
    for (int m = 0; m < 5; ++m)
        for (int k = 0; k < 5; ++k)
            t.flat(WOOD_SWORD + m * 5 + k, std::string(MATERIAL_NAMES[m]) + " " + TOOL_NAMES[k], 1).max_damage = TOOL_USES[m];
    t.flat(BOW, "Bow", 1).max_damage = 384;
    t.flat(ARROW, "Arrow");
    t.flat(FLINT_AND_STEEL, "Flint and Steel", 1).max_damage = 64;
    t.flat(SHEARS, "Shears", 1).max_damage = 238;
    t.flat(FISHING_ROD, "Fishing Rod", 1).max_damage = 64;
    for (int m = 0; m < 5; ++m)
        for (int p = 0; p < 4; ++p) {
            ItemInfo& a = t.flat(LEATHER_HELMET + m * 4 + p,
                                 std::string(ARMOR_MATERIALS[m]) + " " + ARMOR_NAMES[m == 0 ? 0 : 1][p], 1);
            a.max_damage = ARMOR_BASE[p] * ARMOR_FACTOR[m];
            a.armor_slot = p;
        }

    // Materials.
    t.flat(STICK, "Stick");
    t.flat(COAL, "Coal");
    t.flat(CHARCOAL, "Charcoal");
    t.flat(DIAMOND, "Diamond");
    t.flat(IRON_INGOT, "Iron Ingot");
    t.flat(GOLD_INGOT, "Gold Ingot");
    t.flat(REDSTONE, "Redstone");
    t.flat(GLOWSTONE_DUST, "Glowstone Dust");
    t.flat(STRING, "String");
    t.flat(FEATHER, "Feather");
    t.flat(GUNPOWDER, "Gunpowder");
    t.flat(FLINT, "Flint");
    t.flat(LEATHER, "Leather");
    t.flat(CLAY_BALL, "Clay");
    t.flat(CLAY_BRICK, "Brick");
    t.flat(PAPER, "Paper");
    t.flat(BOOK, "Book");
    t.flat(SLIMEBALL, "Slimeball");
    t.flat(BONE, "Bone");
    t.flat(SUGAR, "Sugar");
    t.flat(WHEAT, "Wheat");
    t.flat(EGG, "Egg", 16);
    t.flat(SNOWBALL, "Snowball", 16);
    t.flat(SUGAR_CANE, "Sugar Canes");
    const char* dyes[16] = {"Ink Sac", "Rose Red", "Cactus Green", "Cocoa Beans", "Lapis Lazuli", "Purple Dye",
                            "Cyan Dye", "Light Gray Dye", "Gray Dye", "Pink Dye", "Lime Dye", "Dandelion Yellow",
                            "Light Blue Dye", "Magenta Dye", "Orange Dye", "Bone Meal"};
    for (int d = 0; d < 16; ++d) t.flat(INK_SAC + d, dyes[d]);

    // Food.
    t.food(APPLE, "Apple", 4, 2.4f);
    t.food(BEEF, "Beef", 6, 4.0f);
    t.food(STEAK, "Steak", 8, 12.8f);
    t.food(BREAD, "Bread", 5, 6.0f);
    t.food(COOKIE, "Cookie", 2, 0.4f);
    t.food(GOLDEN_APPLE, "Golden Apple", 4, 9.6f);
    t.food(MUSHROOM_STEW, "Mushroom Stew", 6, 7.2f, 1);
    t.food(RAW_PORKCHOP, "Raw Porkchop", 3, 1.8f);
    t.food(COOKED_PORKCHOP, "Cooked Porkchop", 8, 12.8f);
    t.food(RAW_FISH, "Raw Fish", 2, 0.4f);
    t.food(COOKED_FISH, "Cooked Fish", 5, 6.0f);
    t.flat(CAKE, "Cake", 1);
    t.flat(BOWL, "Bowl");

    // Everything else.
    t.flat(BUCKET, "Bucket", 16);
    t.flat(WATER_BUCKET, "Water Bucket", 1);
    t.flat(LAVA_BUCKET, "Lava Bucket", 1);
    t.flat(MILK_BUCKET, "Milk", 1);
    t.flat(COMPASS, "Compass");
    t.flat(CLOCK, "Clock");
    t.flat(MAP, "Map", 1);
    t.flat(WOOD_DOOR, "Wooden Door", 1);
    t.flat(IRON_DOOR, "Iron Door", 1);
    t.flat(SIGN, "Sign", 16);
    t.flat(BED, "Bed", 1);
    t.flat(PAINTING, "Painting");
    t.flat(BOAT, "Boat", 1);
    t.flat(MINECART, "Minecart", 1);
    t.flat(STORAGE_MINECART, "Minecart with Chest", 1);
    t.flat(POWERED_MINECART, "Minecart with Furnace", 1);
    t.flat(REPEATER, "Redstone Repeater");
    t.flat(SADDLE, "Saddle", 1);
    t.flat(SEEDS, "Seeds");
    return t;
}

const Table& table() {
    static const Table t = build();
    return t;
}

}  // namespace

const ItemInfo* item_info(ItemId id) {
    if (id < 0 || id >= ITEM_END) return nullptr;
    const auto& slot = table().items[id];
    return slot ? &*slot : nullptr;
}

int max_stack(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info ? info->max_stack : 64;
}

int max_damage(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info ? info->max_damage : 0;
}

int armor_slot(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info ? info->armor_slot : -1;
}

bool looks_like_block(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info && info->icon.shape != ItemIcon::Flat;
}

ItemId tool(Material m, int kind) { return WOOD_SWORD + static_cast<int>(m) * 5 + kind; }
ItemId armor(Material m, ArmorPiece piece) { return LEATHER_HELMET + static_cast<int>(m) * 4 + static_cast<int>(piece); }

const FoodInfo* food_info(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info && info->food ? &*info->food : nullptr;
}

std::string item_name(ItemId id) {
    const ItemInfo* info = item_info(id);
    return info ? info->name : "";
}

ItemId current_item_id(ItemId id) {
    if (id == OLD_TORCH_ITEM) return TORCH;
    if (id == OLD_GLOWSTONE_ITEM) return GLOWSTONE;
    if (id == OLD_SANDSTONE_ITEM) return SANDSTONE;
    if (id == OLD_CLAY_ITEM) return CLAY_BLOCK;
    if (id == OLD_TNT_ITEM) return TNT;
    if (id == OLD_YELLOW_FLOWER_ITEM) return YELLOW_FLOWER;
    if (id == OLD_RED_ROSE_ITEM) return RED_ROSE;
    if (id == OLD_BROWN_MUSHROOM_ITEM) return BROWN_MUSHROOM;
    if (id == OLD_RED_MUSHROOM_ITEM) return RED_MUSHROOM;
    if (id == OLD_RAIL_ITEM) return RAIL;
    if (id == OLD_POWERED_RAIL_ITEM) return POWERED_RAIL;
    if (id == OLD_DETECTOR_RAIL_ITEM) return DETECTOR_RAIL;
    return id;
}

int icon_tile(ItemId id) {
    const ItemInfo* info = item_info(id);
    if (!info) return T_BLACK;
    return info->icon.shape == ItemIcon::Flat ? info->icon.top : info->icon.right;
}

const std::vector<ItemId>& all_items() { return table().order; }

const std::vector<ItemId>& creative_items() {
    static const std::vector<ItemId> items = {GRASS, DIRT, STONE, COBBLE, SAND, LOG, PLANKS, BRICK, LEAVES};
    return items;
}
