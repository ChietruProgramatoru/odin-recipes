// Furnaces: smelting recipes, fuel, burning and cooking, the furnace screen's slots, the
// furnace blocks and saving what is inside.
#include <doctest/doctest.h>

#include "bed.h"
#include "container.h"
#include "furnace.h"
#include "save.h"
#include "world.h"

namespace {
// Run a furnace for n ticks.
void run(Furnace& f, int n) {
    for (int i = 0; i < n; ++i) f.tick();
}
}  // namespace

TEST_CASE("Beta's smelting recipes, plus steak") {
    CHECK(smelt_result(IRON_ORE) == Stack{IRON_INGOT, 1});
    CHECK(smelt_result(GOLD_ORE) == Stack{GOLD_INGOT, 1});
    CHECK(smelt_result(DIAMOND_ORE) == Stack{DIAMOND, 1});
    CHECK(smelt_result(SAND) == Stack{GLASS, 1});
    CHECK(smelt_result(COBBLE) == Stack{STONE, 1});
    CHECK(smelt_result(CLAY_BALL) == Stack{CLAY_BRICK, 1});
    for (ItemId log : std::initializer_list<ItemId>{LOG, BIRCH_LOG, SPRUCE_LOG}) CHECK(smelt_result(log) == Stack{CHARCOAL, 1});
    CHECK(smelt_result(RAW_PORKCHOP) == Stack{COOKED_PORKCHOP, 1});
    CHECK(smelt_result(RAW_FISH) == Stack{COOKED_FISH, 1});
    CHECK(smelt_result(BEEF) == Stack{STEAK, 1});
    CHECK_FALSE(smelt_result(DIRT));
    CHECK_FALSE(smelt_result(IRON_INGOT));
    CHECK(food_info(STEAK)->hunger == 8);
}

TEST_CASE("fuel burns for Beta's times") {
    CHECK(fuel_ticks(COAL) == 1600);
    CHECK(fuel_ticks(CHARCOAL) == 1600);
    for (ItemId wood : std::initializer_list<ItemId>{LOG, BIRCH_LOG, PLANKS, CRAFTING_TABLE, CHEST, WOOD_STAIRS, FENCE}) CHECK(fuel_ticks(wood) == 300);
    CHECK(fuel_ticks(STICK) == 100);
    CHECK(fuel_ticks(LAVA_BUCKET) == 20000);
    CHECK(fuel_ticks(COBBLE) == 0);
    CHECK(fuel_ticks(WOOD_SLAB) == 0);  // a stone-like block in Beta
    CHECK(fuel_ticks(BUCKET) == 0);
}

TEST_CASE("a furnace smelts one item every 10 seconds while it has fuel") {
    Furnace f;
    f.input = Stack{IRON_ORE, 3};
    f.fuel = Stack{PLANKS, 1};  // 300 ticks: one and a half items
    f.tick();
    CHECK(f.burning());
    CHECK_FALSE(f.fuel);
    CHECK(f.burn_total == 300);
    CHECK(f.cook == 1);
    run(f, SMELT_TICKS - 1);
    CHECK(f.output == Stack{IRON_INGOT, 1});
    CHECK(f.input == Stack{IRON_ORE, 2});
    CHECK(f.cook == 0);
    run(f, 101);  // the fire goes out halfway through the second ore
    CHECK_FALSE(f.burning());
    CHECK(f.output == Stack{IRON_INGOT, 1});
    f.tick();
    CHECK(f.cook == 0);  // the half-cooked ore starts over, like Beta
    f.fuel = Stack{COAL, 1};
    run(f, SMELT_TICKS * 2);
    CHECK(f.output == Stack{IRON_INGOT, 3});
    CHECK_FALSE(f.input);
    CHECK(f.burning());  // the coal burns on with nothing to cook
    run(f, 2000);
    CHECK_FALSE(f.burning());
}

TEST_CASE("no fuel is used when nothing can be smelted") {
    Furnace f;
    f.fuel = Stack{COAL, 2};
    run(f, 10);
    CHECK_FALSE(f.burning());
    CHECK(f.fuel == Stack{COAL, 2});
    f.input = Stack{DIRT, 1};  // not smeltable
    run(f, 10);
    CHECK(f.fuel == Stack{COAL, 2});
    // A full output slot, or a different item in it, stops the smelting.
    f.input = Stack{SAND, 1};
    f.output = Stack{STONE, 1};
    run(f, 10);
    CHECK(f.fuel == Stack{COAL, 2});
    f.output = Stack{GLASS, 64};
    run(f, 10);
    CHECK(f.fuel == Stack{COAL, 2});
    f.output = Stack{GLASS, 63};
    run(f, SMELT_TICKS);
    CHECK(f.output == Stack{GLASS, 64});
    CHECK(f.fuel == Stack{COAL, 1});
}

TEST_CASE("a lava bucket leaves its bucket behind") {
    Furnace f;
    f.input = Stack{COBBLE, 1};
    f.fuel = Stack{LAVA_BUCKET, 1};
    f.tick();
    CHECK(f.burn_left == 20000);
    CHECK(f.fuel == Stack{BUCKET, 1});
}

TEST_CASE("the furnace screen's slots") {
    Inventory inv;
    Furnace f;
    CraftingMenu menu(inv, f);
    const Slot input{Slot::Furnace, FURNACE_INPUT}, fuel{Slot::Furnace, FURNACE_FUEL}, output{Slot::Furnace, FURNACE_OUTPUT};
    inv.slots[0] = Stack{IRON_ORE, 5};
    inv.slots[1] = Stack{COAL, 3};
    menu.click({Slot::Inv, 0}, 0, false);
    menu.click(input, 0, false);
    menu.click({Slot::Inv, 1}, 0, false);
    menu.click(output, 0, false);  // the output takes nothing
    CHECK(menu.cursor == Stack{COAL, 3});
    CHECK_FALSE(f.output);
    menu.click(fuel, 0, false);
    CHECK(f.input == Stack{IRON_ORE, 5});
    CHECK(f.fuel == Stack{COAL, 3});
    run(f, SMELT_TICKS * 2);
    CHECK(f.output == Stack{IRON_INGOT, 2});
    menu.click(output, 1, false);  // right click: half
    CHECK(menu.cursor == Stack{IRON_INGOT, 1});
    run(f, SMELT_TICKS);
    menu.click(output, 0, false);  // joins the ingot on the mouse
    CHECK(menu.cursor == Stack{IRON_INGOT, 3});
    CHECK_FALSE(f.output);
    menu.click({Slot::Inv, 5}, 0, false);
    // Shift-click moves furnace slots into the hotbar from the right.
    menu.click(input, 0, true);
    CHECK(inv.slots[8] == Stack{IRON_ORE, 2});
    CHECK_FALSE(f.input);
    // Closing the screen leaves the furnace's contents in the furnace.
    CHECK(menu.close().empty());
    CHECK(f.fuel);
}

TEST_CASE("furnace blocks face four ways, lit or not, and drop a furnace") {
    for (int dir = 0; dir < 4; ++dir)
        for (bool lit : {false, true}) {
            BlockId b = furnace_block(dir, lit);
            CHECK(is_furnace(b));
            CHECK(is_lit_furnace(b) == lit);
            CHECK(furnace_dir(b) == dir);
            CHECK(block_info(b)->drop == FURNACE);
            CHECK(light_emission(b) == (lit ? 13 : 0));
            CHECK(is_opaque(b));
            CHECK((b == FURNACE || !valid_item(b)));  // only the one furnace item
        }
    CHECK(furnace_block(0, false) == FURNACE);
    CHECK_FALSE(is_furnace(CHEST));
    CHECK_FALSE(is_opaque(GLASS));
    CHECK(is_solid(GLASS));
    CHECK(light_opacity(GLASS) == 0);
    CHECK_FALSE(block_info(GLASS)->drop);
}

TEST_CASE("a furnace's contents are saved with its chunk") {
    World world(5);
    Chunk& c = world.chunk(0, 0);
    const glm::ivec3 pos{3, 70, 5};
    world.set_block(pos.x, pos.y, pos.z, furnace_block(2, true));
    Furnace& f = world.furnace(pos);
    f.input = Stack{SAND, 10};
    f.fuel = Stack{LAVA_BUCKET, 1};
    f.output = Stack{GLASS, 4};
    f.burn_left = 1234;
    f.burn_total = 1600;
    f.cook = 77;
    // A furnace record where the furnace is gone is dropped on load.
    c.furnaces[Chunk::index(1, 1, 1)] = Furnace{};
    auto back = decode_chunk(encode_chunk(c), 0, 0);
    REQUIRE(back);
    CHECK(back->furnaces.size() == 1);
    CHECK(back->furnaces[Chunk::index(3, 70, 5)] == f);
    CHECK(world.take_furnace(pos));
    CHECK_FALSE(world.furnace_at(pos));
}
