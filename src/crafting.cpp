#include "crafting.h"

#include <algorithm>

#include "items.h"

namespace {

const Ingredient PLANKS_ANY = {PLANKS};
const Ingredient LOG_ANY = {LOG};
const Ingredient COAL_ANY = {COAL, CHARCOAL};
const Ingredient WOOL_ANY = [] {
    Ingredient v;
    for (int c = 0; c < 16; ++c) v.push_back(WHITE_WOOL + c);
    return v;
}();

struct Builder {
    std::vector<Recipe> list;

    void shaped(std::vector<std::string> shape, std::vector<std::pair<char, Ingredient>> key, ItemId result, int count = 1) {
        list.push_back({std::move(shape), std::move(key), {}, Stack{result, count}});
    }
    void shapeless(std::vector<Ingredient> ingredients, ItemId result, int count = 1) {
        list.push_back({{}, {}, std::move(ingredients), Stack{result, count}});
    }
    // 2x2 of one item -> a block, like wool from string.
    void square(Ingredient item, ItemId result, int count = 1) { shaped({"##", "##"}, {{'#', std::move(item)}}, result, count); }
    // 9 items <-> a storage block.
    void storage(ItemId item, ItemId block) {
        shaped({"###", "###", "###"}, {{'#', {item}}}, block);
        shapeless({{block}}, item, 9);
    }
};

std::vector<Recipe> build() {
    Builder b;
    const Ingredient stick = {STICK};

    // Basics
    b.shapeless({LOG_ANY}, PLANKS, 4);
    b.shaped({"#", "#"}, {{'#', PLANKS_ANY}}, STICK, 4);
    b.shaped({"X", "#"}, {{'X', COAL_ANY}, {'#', stick}}, TORCH, 4);
    b.square(PLANKS_ANY, CRAFTING_TABLE);
    b.shaped({"###", "# #", "###"}, {{'#', {COBBLE}}}, FURNACE);
    b.shaped({"###", "# #", "###"}, {{'#', PLANKS_ANY}}, CHEST);

    // Tools and weapons
    const Ingredient tool_materials[5] = {PLANKS_ANY, {COBBLE}, {IRON_INGOT}, {GOLD_INGOT}, {DIAMOND}};
    const std::vector<std::string> tool_shapes[5] = {
        {"X", "X", "#"}, {"X", "#", "#"}, {"XXX", " # ", " # "}, {"XX", "X#", " #"}, {"XX", " #", " #"}};
    for (int m = 0; m < 5; ++m)
        for (int k = 0; k < 5; ++k)
            b.shaped(tool_shapes[k], {{'X', tool_materials[m]}, {'#', stick}}, tool(static_cast<Material>(m), k));
    b.shaped({" #X", "# X", " #X"}, {{'#', stick}, {'X', {STRING}}}, BOW);
    b.shaped({"X", "#", "Y"}, {{'X', {FLINT}}, {'#', stick}, {'Y', {FEATHER}}}, ARROW, 4);
    b.shaped({"A ", " B"}, {{'A', {IRON_INGOT}}, {'B', {FLINT}}}, FLINT_AND_STEEL);
    b.shaped({" #", "# "}, {{'#', {IRON_INGOT}}}, SHEARS);
    b.shaped({"  #", " #X", "# X"}, {{'#', stick}, {'X', {STRING}}}, FISHING_ROD);

    // Armor (chain armor can't be crafted in Beta)
    const std::pair<Material, ItemId> armor_materials[4] = {
        {Material::Wood, LEATHER}, {Material::Iron, IRON_INGOT}, {Material::Gold, GOLD_INGOT}, {Material::Diamond, DIAMOND}};
    const std::vector<std::string> armor_shapes[4] = {{"XXX", "X X"}, {"X X", "XXX", "XXX"}, {"XXX", "X X", "X X"}, {"X X", "X X"}};
    for (const auto& [m, item] : armor_materials)
        for (int p = 0; p < 4; ++p)
            b.shaped(armor_shapes[p], {{'X', {item}}}, armor(m, static_cast<ArmorPiece>(p)));

    // Storage blocks and other blocks
    b.storage(GOLD_INGOT, GOLD_BLOCK);
    b.storage(IRON_INGOT, IRON_BLOCK);
    b.storage(DIAMOND, DIAMOND_BLOCK);
    b.storage(LAPIS_LAZULI, LAPIS_BLOCK);
    b.square({GLOWSTONE_DUST}, GLOWSTONE);
    b.square({STRING}, WHITE_WOOL);
    b.square({SNOWBALL}, SNOW_BLOCK);
    b.square({CLAY_BALL}, CLAY_BLOCK);
    b.square({CLAY_BRICK}, BRICK);
    b.square({SAND}, SANDSTONE);
    b.shaped({"X#X", "#X#", "X#X"}, {{'X', {GUNPOWDER}}, {'#', {SAND}}}, TNT);
    b.shaped({"###", "XXX", "###"}, {{'#', PLANKS_ANY}, {'X', {BOOK}}}, BOOKSHELF);
    b.shaped({"A", "B"}, {{'A', {PUMPKIN}}, {'B', {TORCH}}}, JACK_O_LANTERN);
    b.shaped({"###"}, {{'#', {STONE}}}, STONE_SLAB, 3);
    b.shaped({"###"}, {{'#', {SANDSTONE}}}, SANDSTONE_SLAB, 3);
    b.shaped({"###"}, {{'#', PLANKS_ANY}}, WOOD_SLAB, 3);
    b.shaped({"###"}, {{'#', {COBBLE}}}, COBBLE_SLAB, 3);
    b.shaped({"#  ", "## ", "###"}, {{'#', PLANKS_ANY}}, WOOD_STAIRS, 4);
    b.shaped({"#  ", "## ", "###"}, {{'#', {COBBLE}}}, COBBLE_STAIRS, 4);
    b.shaped({"###", "###"}, {{'#', stick}}, FENCE, 2);
    b.shaped({"# #", "###", "# #"}, {{'#', stick}}, LADDER, 2);
    b.shaped({"###", "###"}, {{'#', PLANKS_ANY}}, TRAPDOOR, 2);
    b.shaped({"##", "##", "##"}, {{'#', PLANKS_ANY}}, WOOD_DOOR);
    b.shaped({"##", "##", "##"}, {{'#', {IRON_INGOT}}}, IRON_DOOR);
    b.shaped({"###", "###", " X "}, {{'#', PLANKS_ANY}, {'X', stick}}, SIGN);
    b.shaped({"###", "XXX"}, {{'#', WOOL_ANY}, {'X', PLANKS_ANY}}, BED);
    b.shaped({"###", "#X#", "###"}, {{'#', stick}, {'X', WOOL_ANY}}, PAINTING);

    // Redstone
    b.shaped({"X", "#"}, {{'X', {REDSTONE}}, {'#', stick}}, REDSTONE_TORCH);
    b.shaped({"X", "#"}, {{'X', stick}, {'#', {COBBLE}}}, LEVER);
    b.shaped({"#", "#"}, {{'#', {STONE}}}, STONE_BUTTON);
    b.shaped({"##"}, {{'#', {STONE}}}, STONE_PLATE);
    b.shaped({"##"}, {{'#', PLANKS_ANY}}, WOOD_PLATE);
    b.shaped({"###", "#X#", "###"}, {{'#', PLANKS_ANY}, {'X', {REDSTONE}}}, NOTE_BLOCK);
    b.shaped({"###", "#X#", "###"}, {{'#', PLANKS_ANY}, {'X', {DIAMOND}}}, JUKEBOX);
    b.shaped({"###", "#X#", "#R#"}, {{'#', {COBBLE}}, {'X', {BOW}}, {'R', {REDSTONE}}}, DISPENSER);
    b.shaped({"TTT", "#X#", "#R#"}, {{'T', PLANKS_ANY}, {'#', {COBBLE}}, {'X', {IRON_INGOT}}, {'R', {REDSTONE}}}, PISTON);
    b.shaped({"S", "P"}, {{'S', {SLIMEBALL}}, {'P', {PISTON}}}, STICKY_PISTON);
    b.shaped({"#X#", "III"}, {{'#', {REDSTONE_TORCH}}, {'X', {REDSTONE}}, {'I', {STONE}}}, REPEATER);

    // Transport
    b.shaped({"X X", "X#X", "X X"}, {{'X', {IRON_INGOT}}, {'#', stick}}, RAIL, 16);
    b.shaped({"X X", "X#X", "XRX"}, {{'X', {GOLD_INGOT}}, {'#', stick}, {'R', {REDSTONE}}}, POWERED_RAIL, 6);
    b.shaped({"X X", "X#X", "XRX"}, {{'X', {IRON_INGOT}}, {'#', {STONE_PLATE}}, {'R', {REDSTONE}}}, DETECTOR_RAIL, 6);
    b.shaped({"# #", "###"}, {{'#', {IRON_INGOT}}}, MINECART);
    b.shaped({"A", "B"}, {{'A', {CHEST}}, {'B', {MINECART}}}, STORAGE_MINECART);
    b.shaped({"A", "B"}, {{'A', {FURNACE}}, {'B', {MINECART}}}, POWERED_MINECART);
    b.shaped({"# #", "###"}, {{'#', PLANKS_ANY}}, BOAT);

    // Items and food
    b.shaped({"# #", " # "}, {{'#', {IRON_INGOT}}}, BUCKET);
    b.shaped({" # ", "#X#", " # "}, {{'#', {IRON_INGOT}}, {'X', {REDSTONE}}}, COMPASS);
    b.shaped({" # ", "#X#", " # "}, {{'#', {GOLD_INGOT}}, {'X', {REDSTONE}}}, CLOCK);
    b.shaped({"###", "#X#", "###"}, {{'#', {PAPER}}, {'X', {COMPASS}}}, MAP);
    b.shaped({"###"}, {{'#', {SUGAR_CANE}}}, PAPER, 3);
    b.shaped({"#", "#", "#"}, {{'#', {PAPER}}}, BOOK);
    b.shapeless({{SUGAR_CANE}}, SUGAR);
    b.shaped({"# #", " # "}, {{'#', PLANKS_ANY}}, BOWL, 4);
    b.shaped({"Y", "X", "#"}, {{'Y', {BROWN_MUSHROOM}}, {'X', {RED_MUSHROOM}}, {'#', {BOWL}}}, MUSHROOM_STEW);
    b.shaped({"Y", "X", "#"}, {{'Y', {RED_MUSHROOM}}, {'X', {BROWN_MUSHROOM}}, {'#', {BOWL}}}, MUSHROOM_STEW);
    b.shaped({"###"}, {{'#', {WHEAT}}}, BREAD);
    b.shaped({"#X#"}, {{'#', {WHEAT}}, {'X', {COCOA_BEANS}}}, COOKIE, 8);
    b.shaped({"AAA", "BEB", "CCC"}, {{'A', {MILK_BUCKET}}, {'B', {SUGAR}}, {'E', {EGG}}, {'C', {WHEAT}}}, CAKE);
    b.shaped({"###", "#X#", "###"}, {{'#', {GOLD_BLOCK}}, {'X', {APPLE}}}, GOLDEN_APPLE);

    // Dyes
    b.shapeless({{BONE}}, BONE_MEAL, 3);
    b.shapeless({{RED_ROSE}}, ROSE_RED, 2);
    b.shapeless({{YELLOW_FLOWER}}, DANDELION_YELLOW, 2);
    b.shapeless({{ROSE_RED}, {DANDELION_YELLOW}}, ORANGE_DYE, 2);
    b.shapeless({{LAPIS_LAZULI}, {BONE_MEAL}}, LIGHT_BLUE_DYE, 2);
    b.shapeless({{ROSE_RED}, {LAPIS_LAZULI}}, PURPLE_DYE, 2);
    b.shapeless({{ROSE_RED}, {BONE_MEAL}}, PINK_DYE, 2);
    b.shapeless({{CACTUS_GREEN}, {BONE_MEAL}}, LIME_DYE, 2);
    b.shapeless({{INK_SAC}, {BONE_MEAL}}, GRAY_DYE, 2);
    b.shapeless({{GRAY_DYE}, {BONE_MEAL}}, LIGHT_GRAY_DYE, 2);
    b.shapeless({{INK_SAC}, {BONE_MEAL}, {BONE_MEAL}}, LIGHT_GRAY_DYE, 3);
    b.shapeless({{LAPIS_LAZULI}, {CACTUS_GREEN}}, CYAN_DYE, 2);
    b.shapeless({{PURPLE_DYE}, {PINK_DYE}}, MAGENTA_DYE, 2);
    b.shapeless({{LAPIS_LAZULI}, {ROSE_RED}, {PINK_DYE}}, MAGENTA_DYE, 3);
    b.shapeless({{LAPIS_LAZULI}, {ROSE_RED}, {ROSE_RED}, {BONE_MEAL}}, MAGENTA_DYE, 4);
    // Dyeing white wool: wool colour c takes dye 15 - c.
    for (int c = 1; c < 16; ++c) b.shapeless({{INK_SAC + 15 - c}, {WHITE_WOOL}}, WHITE_WOOL + c);
    return b.list;
}

bool accepts(const Ingredient& ing, const std::optional<Stack>& cell) {
    return cell && std::find(ing.begin(), ing.end(), cell->item) != ing.end();
}

const Ingredient* key_of(const Recipe& r, char ch) {
    for (const auto& [k, ing] : r.key)
        if (k == ch) return &ing;
    return nullptr;
}

bool matches_shaped(const Recipe& r, const Grid& grid, int size) {
    // The filled part of the grid must be exactly the recipe's size.
    int x0 = size, y0 = size, x1 = -1, y1 = -1;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (grid[y * size + x]) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
    const int h = static_cast<int>(r.shape.size()), w = static_cast<int>(r.shape[0].size());
    if (x1 < 0 || x1 - x0 + 1 != w || y1 - y0 + 1 != h) return false;
    for (int mirror = 0; mirror < 2; ++mirror) {
        bool ok = true;
        for (int y = 0; y < h && ok; ++y)
            for (int x = 0; x < w && ok; ++x) {
                char ch = r.shape[y][mirror ? w - 1 - x : x];
                const auto& cell = grid[(y0 + y) * size + x0 + x];
                if (ch == ' ')
                    ok = !cell;
                else
                    ok = accepts(*key_of(r, ch), cell);
            }
        if (ok) return true;
    }
    return false;
}

bool matches_shapeless(const Recipe& r, const Grid& grid) {
    std::vector<bool> used(r.ingredients.size(), false);
    for (const auto& cell : grid) {
        if (!cell) continue;
        bool found = false;
        for (size_t i = 0; i < r.ingredients.size() && !found; ++i)
            if (!used[i] && accepts(r.ingredients[i], cell)) used[i] = found = true;
        if (!found) return false;
    }
    return std::all_of(used.begin(), used.end(), [](bool u) { return u; });
}

}  // namespace

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> list = build();
    return list;
}

const Recipe* match_recipe(const Grid& grid, int size) {
    for (const Recipe& r : recipes()) {
        if (r.shape.empty() ? matches_shapeless(r, grid) : matches_shaped(r, grid, size)) return &r;
    }
    return nullptr;
}

std::optional<Stack> craft_result(const Grid& grid, int size) {
    const Recipe* r = match_recipe(grid, size);
    if (!r) return std::nullopt;
    return r->result;
}

void consume_ingredients(Grid& grid) {
    for (auto& cell : grid) {
        if (!cell) continue;
        if (cell->item == MILK_BUCKET) {
            cell = Stack{BUCKET, 1};
            continue;
        }
        if (--cell->count <= 0) cell.reset();
    }
}
