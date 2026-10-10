// Items, recipes and moving stacks around the inventory screens.
#include <doctest/doctest.h>

#include <algorithm>
#include <set>

#include "container.h"
#include "crafting.h"
#include "items.h"

namespace {
// A grid from rows of item ids (0 = empty).
Grid grid_of(std::initializer_list<std::initializer_list<ItemId>> rows) {
    Grid g;
    for (const auto& row : rows)
        for (ItemId id : row) g.push_back(id ? std::optional<Stack>(Stack{id, 1}) : std::nullopt);
    return g;
}
}  // namespace

TEST_CASE("every item has a name, a sane stack size and a unique icon") {
    std::set<std::string> names;
    std::set<int> flat_tiles;
    for (ItemId id : all_items()) {
        const ItemInfo* info = item_info(id);
        REQUIRE(info);
        CHECK_FALSE(info->name.empty());
        INFO(info->name);
        if (id != CLAY_BALL) CHECK(names.insert(info->name).second);  // Beta calls the block and the ball "Clay"
        CHECK((info->max_stack == 1 || info->max_stack == 16 || info->max_stack == 64));
        if (info->max_damage > 0) CHECK(info->max_stack == 1);
        if (info->icon.shape == ItemIcon::Flat) CHECK(flat_tiles.insert(info->icon.top).second);
    }
    CHECK_FALSE(item_info(AIR));
    CHECK_FALSE(item_info(WATER));
    CHECK_FALSE(item_info(9999));
    // Stack limits from the plan: tools 1, snowballs/eggs/signs/buckets 16, food 64 except cake and stew.
    CHECK(max_stack(DIAMOND_PICKAXE) == 1);
    CHECK(max_stack(SNOWBALL) == 16);
    CHECK(max_stack(EGG) == 16);
    CHECK(max_stack(SIGN) == 16);
    CHECK(max_stack(BUCKET) == 16);
    CHECK(max_stack(BREAD) == 64);
    CHECK(max_stack(COOKED_PORKCHOP) == 64);
    CHECK(max_stack(CAKE) == 1);
    CHECK(max_stack(MUSHROOM_STEW) == 1);
    // Beta's durability.
    CHECK(max_damage(WOOD_PICKAXE) == 59);
    CHECK(max_damage(GOLD_SWORD) == 32);
    CHECK(max_damage(DIAMOND_AXE) == 1561);
    CHECK(max_damage(LEATHER_HELMET) == 33);
    CHECK(max_damage(DIAMOND_CHESTPLATE) == 384);
    CHECK(armor_slot(IRON_LEGGINGS) == 2);
    CHECK(armor_slot(IRON_INGOT) == -1);
    CHECK(tool(Material::Stone, 2) == STONE_PICKAXE);
    CHECK(armor(Material::Gold, ArmorPiece::Boots) == GOLD_BOOTS);
}

TEST_CASE("every recipe makes a real item from real items, and no two recipes clash") {
    for (const Recipe& r : recipes()) {
        CHECK(valid_item(r.result.item));
        CHECK(r.result.count >= 1);
        CHECK(r.result.count <= max_stack(r.result.item));
        for (const auto& [ch, ing] : r.key)
            for (ItemId id : ing) CHECK(valid_item(id));
        for (const auto& ing : r.ingredients)
            for (ItemId id : ing) CHECK(valid_item(id));
        // Lay the recipe out in a 3x3 grid with the first choice of each ingredient: it must make itself.
        Grid g(9);
        if (r.shape.empty()) {
            for (size_t i = 0; i < r.ingredients.size(); ++i) g[i] = Stack{r.ingredients[i][0], 1};
        } else {
            for (size_t y = 0; y < r.shape.size(); ++y)
                for (size_t x = 0; x < r.shape[y].size(); ++x)
                    for (const auto& [ch, ing] : r.key)
                        if (ch == r.shape[y][x]) g[y * 3 + x] = Stack{ing[0], 1};
        }
        const Recipe* found = match_recipe(g, 3);
        INFO("recipe for " << item_name(r.result.item));
        CHECK(found == &r);
    }
    CHECK(recipes().size() > 120);
}

TEST_CASE("shaped recipes match anywhere in the grid and mirrored, shapeless in any cells") {
    // A log anywhere makes 4 planks.
    CHECK(craft_result(grid_of({{0, 0}, {0, LOG}}), 2) == Stack{PLANKS, 4});
    CHECK(craft_result(grid_of({{0, 0, 0}, {0, 0, 0}, {LOG, 0, 0}}), 3) == Stack{PLANKS, 4});
    // Sticks in either column of a 2x2 grid.
    CHECK(craft_result(grid_of({{PLANKS, 0}, {PLANKS, 0}}), 2) == Stack{STICK, 4});
    CHECK(craft_result(grid_of({{0, PLANKS}, {0, PLANKS}}), 2) == Stack{STICK, 4});
    // Axes work facing either way.
    CHECK(craft_result(grid_of({{COBBLE, COBBLE, 0}, {COBBLE, STICK, 0}, {0, STICK, 0}}), 3) == Stack{STONE_AXE, 1});
    CHECK(craft_result(grid_of({{0, COBBLE, COBBLE}, {0, STICK, COBBLE}, {0, STICK, 0}}), 3) == Stack{STONE_AXE, 1});
    // A pickaxe needs the 3x3 grid, and an extra item spoils it.
    CHECK(craft_result(grid_of({{IRON_INGOT, IRON_INGOT, IRON_INGOT}, {0, STICK, 0}, {0, STICK, 0}}), 3) ==
          Stack{IRON_PICKAXE, 1});
    CHECK_FALSE(craft_result(grid_of({{IRON_INGOT, IRON_INGOT, IRON_INGOT}, {0, STICK, 0}, {DIRT, STICK, 0}}), 3));
    // Torches take coal or charcoal.
    CHECK(craft_result(grid_of({{CHARCOAL, 0}, {STICK, 0}}), 2) == Stack{TORCH, 4});
    // Shapeless dyes in any cells.
    CHECK(craft_result(grid_of({{0, BONE_MEAL, 0}, {0, 0, 0}, {ROSE_RED, 0, 0}}), 3) == Stack{PINK_DYE, 2});
    CHECK(craft_result(grid_of({{INK_SAC, 0, 0}, {0, 0, 0}, {0, 0, WHITE_WOOL}}), 3) == Stack{BLACK_WOOL, 1});
    CHECK_FALSE(craft_result(grid_of({{ROSE_RED, BONE_MEAL}, {BONE_MEAL, 0}}), 2));
    // Beta's quantities.
    CHECK(craft_result(grid_of({{STICK, 0, STICK}, {STICK, STICK, STICK}, {STICK, 0, STICK}}), 3) == Stack{LADDER, 2});
    CHECK(craft_result(grid_of({{STICK, STICK, STICK}, {STICK, STICK, STICK}, {0, 0, 0}}), 3) == Stack{FENCE, 2});
    CHECK(craft_result(grid_of({{PLANKS, PLANKS, PLANKS}, {PLANKS, PLANKS, PLANKS}, {0, STICK, 0}}), 3) == Stack{SIGN, 1});
    CHECK(craft_result(grid_of({{PLANKS, PLANKS, 0}, {PLANKS, PLANKS, 0}, {PLANKS, PLANKS, 0}}), 3) == Stack{WOOD_DOOR, 1});
    // Nothing for an empty grid.
    CHECK_FALSE(craft_result(Grid(4), 2));
}

TEST_CASE("crafting a cake leaves the empty buckets in the grid") {
    Grid g = grid_of({{MILK_BUCKET, MILK_BUCKET, MILK_BUCKET}, {SUGAR, EGG, SUGAR}, {WHEAT, WHEAT, WHEAT}});
    CHECK(craft_result(g, 3) == Stack{CAKE, 1});
    consume_ingredients(g);
    CHECK(g[0] == Stack{BUCKET, 1});
    CHECK(g[2] == Stack{BUCKET, 1});
    CHECK_FALSE(g[4]);
    CHECK_FALSE(g[8]);
}

TEST_CASE("left click picks up and places, right click splits and places one") {
    Inventory inv;
    inv.slots[9] = Stack{DIRT, 5};
    CraftingMenu m(inv, 2);
    m.click({Slot::Inv, 9}, 1, false);  // right click takes the bigger half
    CHECK(m.cursor == Stack{DIRT, 3});
    CHECK(inv.slots[9] == Stack{DIRT, 2});
    m.click({Slot::Inv, 10}, 1, false);  // right click puts one down
    CHECK(inv.slots[10] == Stack{DIRT, 1});
    CHECK(m.cursor == Stack{DIRT, 2});
    m.click({Slot::Inv, 9}, 0, false);  // left click puts them all onto the same item
    CHECK(inv.slots[9] == Stack{DIRT, 4});
    CHECK_FALSE(m.cursor);
    m.click({Slot::Inv, 9}, 0, false);  // left click picks the whole stack up
    CHECK(m.cursor == Stack{DIRT, 4});
    CHECK_FALSE(inv.slots[9]);
    inv.slots[11] = Stack{SAND, 7};
    m.click({Slot::Inv, 11}, 0, false);  // different items swap
    CHECK(m.cursor == Stack{SAND, 7});
    CHECK(inv.slots[11] == Stack{DIRT, 4});
    // Stacks stop at the item's limit.
    inv.slots[12] = Stack{SNOWBALL, 15};
    m.cursor = Stack{SNOWBALL, 5};
    m.click({Slot::Inv, 12}, 0, false);
    CHECK(inv.slots[12] == Stack{SNOWBALL, 16});
    CHECK(m.cursor == Stack{SNOWBALL, 4});
}

TEST_CASE("armor slots only take the right piece, one at a time") {
    Inventory inv;
    CraftingMenu m(inv, 2);
    m.cursor = Stack{DIRT, 3};
    m.click({Slot::Armor, 0}, 0, false);
    CHECK_FALSE(inv.armor[0]);
    m.cursor = Stack{IRON_HELMET, 1};
    m.click({Slot::Armor, 1}, 0, false);  // a helmet isn't a chestplate
    CHECK_FALSE(inv.armor[1]);
    m.click({Slot::Armor, 0}, 0, false);
    CHECK(inv.armor[0] == Stack{IRON_HELMET, 1});
    CHECK_FALSE(m.cursor);
}

TEST_CASE("taking the crafted item uses up one of each ingredient") {
    Inventory inv;
    CraftingMenu m(inv, 2);
    m.cursor = Stack{PLANKS, 5};
    m.click({Slot::Grid, 0}, 1, false);
    m.click({Slot::Grid, 2}, 1, false);
    m.click({Slot::Grid, 0}, 1, false);
    m.click({Slot::Grid, 2}, 1, false);
    CHECK(m.result == Stack{STICK, 4});
    m.cursor.reset();
    m.click({Slot::Result, 0}, 0, false);
    CHECK(m.cursor == Stack{STICK, 4});
    CHECK(m.result == Stack{STICK, 4});  // one plank left in each cell
    m.click({Slot::Result, 0}, 0, false);
    CHECK(m.cursor == Stack{STICK, 8});
    CHECK_FALSE(m.result);
    CHECK_FALSE(m.grid[0]);
    // The result can't be taken onto a different item.
    m.grid[0] = Stack{LOG, 1};
    m.click({Slot::Grid, 1}, 0, false);  // any click on the grid refreshes the result; this one puts sticks down
    CHECK(inv.count(STICK) == 0);
    CHECK(m.grid[1] == Stack{STICK, 8});
    CHECK_FALSE(m.result);  // log + sticks isn't a recipe
}

TEST_CASE("shift-click moves between the hotbar and storage, and crafts as many as fit") {
    Inventory inv;
    inv.slots[0] = Stack{DIRT, 10};
    inv.slots[20] = Stack{DIRT, 60};
    CraftingMenu m(inv, 2);
    m.click({Slot::Inv, 0}, 0, true);  // hotbar -> storage: fills the dirt stack first
    CHECK(inv.slots[20] == Stack{DIRT, 64});
    CHECK(inv.slots[9] == Stack{DIRT, 6});
    CHECK_FALSE(inv.slots[0]);
    m.click({Slot::Inv, 9}, 0, true);  // storage -> hotbar
    CHECK(inv.slots[0] == Stack{DIRT, 6});

    m.grid[0] = Stack{LOG, 3};
    m.click({Slot::Grid, 1}, 0, false);  // refresh the result
    CHECK(m.result == Stack{PLANKS, 4});
    m.click({Slot::Result, 0}, 0, true);
    CHECK(inv.count(PLANKS) == 12);
    CHECK(inv.slots[8] == Stack{PLANKS, 12});  // the hotbar fills from the right
    CHECK_FALSE(m.grid[0]);
    CHECK_FALSE(m.result);
}

TEST_CASE("closing the screen puts the grid and the held stack back, dropping what doesn't fit") {
    Inventory inv;
    for (int i = 1; i < Inventory::SIZE; ++i) inv.slots[i] = Stack{STONE, 64};
    CraftingMenu m(inv, 3);
    m.grid[4] = Stack{PLANKS, 70 - 64};
    m.cursor = Stack{DIRT, 64};
    m.grid[0] = Stack{SAND, 2};
    auto dropped = m.close();
    CHECK(inv.slots[0] == Stack{DIRT, 64});  // the held stack went first into the one free slot
    REQUIRE(dropped.size() == 2);
    CHECK(dropped[0] == Stack{SAND, 2});
    CHECK(dropped[1] == Stack{PLANKS, 6});
    CHECK_FALSE(m.cursor);
}

TEST_CASE("throwing items from the screens") {
    Inventory inv;
    inv.slots[3] = Stack{COBBLE, 10};
    CraftingMenu m(inv, 2);
    CHECK(m.drop_one({Slot::Inv, 3}) == Stack{COBBLE, 1});
    CHECK(inv.slots[3] == Stack{COBBLE, 9});
    m.click({Slot::Inv, 3}, 0, false);
    CHECK(m.click_outside(1) == Stack{COBBLE, 1});
    CHECK(m.cursor == Stack{COBBLE, 8});
    CHECK(m.click_outside(0) == Stack{COBBLE, 8});
    CHECK_FALSE(m.cursor);
}

TEST_CASE("the creative picker hands out full stacks") {
    Inventory hotbar;
    CreativeMenu c(hotbar);
    const auto& items = all_items();
    int sword = static_cast<int>(std::find(items.begin(), items.end(), DIAMOND_SWORD) - items.begin());
    int dirt = static_cast<int>(std::find(items.begin(), items.end(), DIRT) - items.begin());
    c.click_item(dirt, 0, false);
    CHECK(c.cursor == Stack{DIRT, 64});
    c.click_hotbar(2, 0, false);
    CHECK(hotbar.slots[2] == Stack{DIRT, 64});
    c.click_item(sword, 0, true);  // shift puts it straight in the hotbar
    CHECK(hotbar.slots[0] == Stack{DIAMOND_SWORD, 1});
    c.click_hotbar(2, 0, false);
    c.click_item(sword, 0, false);  // clicking the picker while holding something deletes it
    CHECK_FALSE(c.cursor);
    c.click_hotbar(0, 0, true);
    CHECK_FALSE(hotbar.slots[0]);
}
