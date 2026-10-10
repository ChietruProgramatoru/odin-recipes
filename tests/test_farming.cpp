// Farming (M8): farmland, wheat, sugar cane, plants, cake, bone meal and food.
#include <doctest/doctest.h>

#include <algorithm>

#include "crafting.h"
#include "farming.h"
#include "items.h"
#include "survival.h"
#include "world.h"

namespace {

// Stone up to y = 8 with a layer of grass at y = 9, so the ground is at y = 10.
std::unique_ptr<Chunk> grass_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = y == 9 ? GRASS : STONE;
    c->max_y = 10;
    return c;
}

World grass_world() {
    World w(1);
    for (int cx = -1; cx <= 1; ++cx)
        for (int cz = -1; cz <= 1; ++cz) w.insert(grass_chunk(cx, cz));
    return w;
}

constexpr int G = 9;  // the grass layer
const glm::ivec3 SPOT{4, G, 4};

glm::ivec3 up(glm::ivec3 p) { return p + glm::ivec3(0, 1, 0); }

int count(const std::vector<ItemId>& items, ItemId item) {
    return static_cast<int>(std::count(items.begin(), items.end(), item));
}

}  // namespace

TEST_CASE("a hoe tills grass and dirt into dry farmland, but not under a block or on stone") {
    World w = grass_world();
    FarmEvents ev;
    CHECK(till(w, SPOT, ev));
    CHECK(w.get_block(4, G, 4) == FARMLAND);
    CHECK(w.get_data(4, G, 4) == 0);
    CHECK_FALSE(ev.rebuild.empty());
    w.set_block(6, G, 4, DIRT);
    CHECK(till(w, {6, G, 4}, ev));
    w.set_block(8, G + 1, 4, STONE);
    CHECK_FALSE(till(w, {8, G, 4}, ev));
    CHECK_FALSE(till(w, {8, G - 1, 4}, ev));  // stone
    CHECK(block_height(FARMLAND) == doctest::Approx(15.0 / 16));
}

TEST_CASE("farmland gets wet near water and dries out back to dirt without it") {
    World w = grass_world();
    FarmEvents ev;
    till(w, SPOT, ev);
    Rng rng(1);
    TickContext ctx;
    CHECK_FALSE(water_near_farmland(w, SPOT));
    // Dry with nothing on it: back to dirt.
    for (int i = 0; i < 200 && w.get_block(4, G, 4) == FARMLAND; ++i) random_tick(w, SPOT, rng, ctx, ev);
    CHECK(w.get_block(4, G, 4) == DIRT);

    // Water 4 blocks away wets it.
    till(w, SPOT, ev);
    w.set_block(8, G, 4, WATER);
    CHECK(water_near_farmland(w, SPOT));
    for (int i = 0; i < 200; ++i) random_tick(w, SPOT, rng, ctx, ev);
    CHECK(w.get_data(4, G, 4) == FARMLAND_WET);
    // 5 away is too far.
    w.set_block(8, G, 4, GRASS);
    w.set_block(9, G, 4, WATER);
    CHECK_FALSE(water_near_farmland(w, SPOT));
    // It dries one step at a time, but a crop keeps it from turning to dirt.
    w.set_block(4, G + 1, 4, WHEAT_CROP);
    for (int i = 0; i < 400; ++i) random_tick(w, SPOT, rng, ctx, ev);
    CHECK(w.get_block(4, G, 4) == FARMLAND);
    CHECK(w.get_data(4, G, 4) == 0);
}

TEST_CASE("rain wets farmland under open sky") {
    World w = grass_world();
    FarmEvents ev;
    till(w, SPOT, ev);
    Rng rng(2);
    TickContext ctx;
    ctx.raining = true;
    for (int i = 0; i < 200; ++i) random_tick(w, SPOT, rng, ctx, ev);
    CHECK(w.get_data(4, G, 4) == FARMLAND_WET);
}

TEST_CASE("wheat grows on farmland in light, faster on wet farmland, and not at night") {
    World w = grass_world();
    FarmEvents ev;
    till(w, SPOT, ev);
    w.set_block(4, G + 1, 4, WHEAT_CROP);
    CHECK(can_stay(w, up(SPOT), WHEAT_CROP));
    CHECK(wheat_growth_rate(w, up(SPOT)) == doctest::Approx(2.0f));  // 1 + dry farmland under it
    w.set_block(4, G, 4, FARMLAND, FARMLAND_WET);
    CHECK(wheat_growth_rate(w, up(SPOT)) == doctest::Approx(4.0f));
    // Wet farmland all around adds 3/4 for each.
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz) w.set_block(4 + dx, G, 4 + dz, FARMLAND, FARMLAND_WET);
    CHECK(wheat_growth_rate(w, up(SPOT)) == doctest::Approx(10.0f));
    // Wheat on two sides (a cross shape) halves it.
    w.set_block(3, G + 1, 4, WHEAT_CROP);
    w.set_block(4, G + 1, 3, WHEAT_CROP);
    CHECK(wheat_growth_rate(w, up(SPOT)) == doctest::Approx(5.0f));
    w.set_block(3, G + 1, 4, AIR);
    w.set_block(4, G + 1, 3, AIR);

    Rng rng(3);
    TickContext night;
    night.sky_darkness = 11;
    for (int i = 0; i < 2000; ++i) random_tick(w, up(SPOT), rng, night, ev);
    CHECK(w.get_data(4, G + 1, 4) == 0);  // too dark
    TickContext day;
    for (int i = 0; i < 2000; ++i) random_tick(w, up(SPOT), rng, day, ev);
    CHECK(w.get_data(4, G + 1, 4) == WHEAT_RIPE);
    CHECK(w.get_block(4, G + 1, 4) == WHEAT_CROP);
}

TEST_CASE("wheat drops wheat and seeds when ripe, only a chance of seeds before") {
    Rng rng(4);
    int seeds_ripe = 0, seeds_young = 0, wheat_young = 0;
    for (int i = 0; i < 300; ++i) {
        auto ripe = farm_drops(WHEAT_CROP, WHEAT_RIPE, rng);
        CHECK(count(ripe, WHEAT) == 1);
        seeds_ripe += count(ripe, SEEDS);
        auto young = farm_drops(WHEAT_CROP, 0, rng);
        seeds_young += count(young, SEEDS);
        wheat_young += count(young, WHEAT);
    }
    CHECK(wheat_young == 0);
    CHECK(seeds_ripe > 300);   // 3 tries at 8/15 each: about 1.6 a harvest
    CHECK(seeds_young < 150);  // 3 tries at 1/15 each: about 0.2
    // Tall grass gives seeds 1 time in 8.
    int grass_seeds = 0;
    for (int i = 0; i < 800; ++i) grass_seeds += count(farm_drops(TALL_GRASS, 0, rng), SEEDS);
    CHECK(grass_seeds > 60);
    CHECK(grass_seeds < 150);
    CHECK(farm_drops(CAKE_BLOCK, 2, rng).empty());
}

TEST_CASE("plants pop off when the block under them goes, all the way up a sugar cane") {
    World w = grass_world();
    FarmEvents ev;
    Rng rng(5);
    w.set_block(4, G + 1, 4, YELLOW_FLOWER);
    w.set_block(4, G, 4, AIR);
    break_unsupported(w, SPOT, rng, ev);
    CHECK(w.get_block(4, G + 1, 4) == AIR);
    REQUIRE(ev.drops.size() == 1);
    CHECK(ev.drops[0].second == YELLOW_FLOWER);

    // Sugar cane by water, three tall.
    w.set_block(6, G, 5, WATER);
    const glm::ivec3 base{6, G + 1, 4};
    CHECK(can_stay(w, base, SUGAR_CANE_PLANT));
    CHECK_FALSE(can_stay(w, {2, G + 1, 2}, SUGAR_CANE_PLANT));  // no water next to it
    for (int k = 0; k < 3; ++k) w.set_block(6, G + 1 + k, 4, SUGAR_CANE_PLANT);
    w.set_block(6, G + 1, 4, AIR);  // break the bottom one
    ev = {};
    break_unsupported(w, base, rng, ev);
    CHECK(w.get_block(6, G + 2, 4) == AIR);
    CHECK(w.get_block(6, G + 3, 4) == AIR);
    CHECK(ev.drops.size() == 2);
}

TEST_CASE("sugar cane grows to three tall") {
    World w = grass_world();
    FarmEvents ev;
    Rng rng(6);
    w.set_block(6, G, 5, WATER);
    w.set_block(6, G + 1, 4, SUGAR_CANE_PLANT);
    TickContext ctx;
    for (int i = 0; i < 200; ++i)
        for (int k = 0; k < 4; ++k) random_tick(w, {6, G + 1 + k, 4}, rng, ctx, ev);
    CHECK(w.get_block(6, G + 2, 4) == SUGAR_CANE_PLANT);
    CHECK(w.get_block(6, G + 3, 4) == SUGAR_CANE_PLANT);
    CHECK(w.get_block(6, G + 4, 4) == AIR);
}

TEST_CASE("flowers and tall grass need grass or dirt, mushrooms need the dark") {
    World w = grass_world();
    CHECK(can_stay(w, up(SPOT), YELLOW_FLOWER));
    CHECK(can_stay(w, up(SPOT), TALL_GRASS));
    CHECK_FALSE(can_stay(w, {4, G, 4}, RED_ROSE));  // on stone
    CHECK_FALSE(can_stay(w, up(SPOT), BROWN_MUSHROOM));  // in full daylight
    // Under a wide roof it is dark enough.
    for (int x = -8; x <= 16; ++x)
        for (int z = -8; z <= 16; ++z) w.set_block(x, G + 3, z, STONE);
    CHECK(can_stay(w, up(SPOT), BROWN_MUSHROOM));
    CHECK(can_stay(w, up(SPOT), RED_MUSHROOM));
}

TEST_CASE("bone meal ripens wheat at once and sprouts grass and flowers around grass") {
    World w = grass_world();
    FarmEvents ev;
    Rng rng(7);
    till(w, SPOT, ev);
    w.set_block(4, G + 1, 4, WHEAT_CROP);
    CHECK(bone_meal(w, up(SPOT), rng, ev));
    CHECK(w.get_data(4, G + 1, 4) == WHEAT_RIPE);
    CHECK_FALSE(bone_meal(w, up(SPOT), rng, ev));  // already ripe: not used up
    CHECK_FALSE(bone_meal(w, {4, G - 1, 4}, rng, ev));  // stone

    CHECK(bone_meal(w, {10, G, 10}, rng, ev));
    int grass = 0, flowers = 0;
    for (int x = 0; x < 20; ++x)
        for (int z = 0; z < 20; ++z) {
            BlockId b = w.get_block(x, G + 1, z);
            grass += b == TALL_GRASS;
            flowers += b == YELLOW_FLOWER || b == RED_ROSE;
        }
    CHECK(grass > 10);
    CHECK(grass > flowers);
}

TEST_CASE("landing on farmland can trample it; a small step can't") {
    World w = grass_world();
    FarmEvents ev;
    Rng rng(8);
    till(w, SPOT, ev);
    w.set_block(4, G + 1, 4, WHEAT_CROP);
    CHECK_FALSE(trample(w, SPOT, 0.4, rng, ev));  // stepping down less than half a block
    CHECK(w.get_block(4, G, 4) == FARMLAND);
    CHECK(trample(w, SPOT, 3.0, rng, ev));  // a real fall always does
    CHECK(w.get_block(4, G, 4) == DIRT);
    CHECK(w.get_block(4, G + 1, 4) == AIR);  // the crop popped off
}

TEST_CASE("items plant the right blocks, and old flower items load as the blocks") {
    CHECK(planted_block(SEEDS) == WHEAT_CROP);
    CHECK(planted_block(SUGAR_CANE) == SUGAR_CANE_PLANT);
    CHECK(planted_block(CAKE) == CAKE_BLOCK);
    CHECK(planted_block(RED_ROSE) == RED_ROSE);
    CHECK(planted_block(WHEAT) == AIR);
    CHECK(current_item_id(OLD_YELLOW_FLOWER_ITEM) == YELLOW_FLOWER);
    CHECK(current_item_id(OLD_RED_MUSHROOM_ITEM) == RED_MUSHROOM);
    CHECK(valid_item(SEEDS));
    CHECK(valid_item(YELLOW_FLOWER));
    CHECK(is_plant(WHEAT_CROP));
    CHECK_FALSE(is_solid(TALL_GRASS));
    CHECK(is_solid(CAKE_BLOCK));
    CHECK(block_height(CAKE_BLOCK) == doctest::Approx(0.5));
}

TEST_CASE("every food fills hunger, food stacks to 64 but stew and cake to 1, and a cake slice fills 2") {
    for (ItemId f : std::initializer_list<ItemId>{APPLE, GOLDEN_APPLE, BREAD, RAW_PORKCHOP, COOKED_PORKCHOP, RAW_FISH, COOKED_FISH, COOKIE, MUSHROOM_STEW})
        CHECK(food_info(f) != nullptr);
    CHECK(max_stack(BREAD) == 64);
    CHECK(max_stack(MUSHROOM_STEW) == 1);
    CHECK(max_stack(CAKE) == 1);
    Vitals v;
    v.hunger = 10;
    v.saturation = 0;
    CHECK(v.eat(CAKE_SLICE));
    CHECK(v.hunger == 12);
    v.hunger = MAX_HUNGER;
    CHECK_FALSE(v.eat(CAKE_SLICE));
}

TEST_CASE("bread from three wheat, mushroom stew from the wild mushrooms, sugar from cane") {
    auto s = [](ItemId i) { return std::optional<Stack>(Stack{i, 1}); };
    Grid bread(9);
    bread[3] = bread[4] = bread[5] = s(WHEAT);
    auto r = craft_result(bread, 3);
    REQUIRE(r);
    CHECK(r->item == BREAD);
    Grid stew(9);
    stew[1] = s(BROWN_MUSHROOM);
    stew[4] = s(RED_MUSHROOM);
    stew[7] = s(BOWL);
    r = craft_result(stew, 3);
    REQUIRE(r);
    CHECK(r->item == MUSHROOM_STEW);
    Grid sugar(4);
    sugar[0] = s(SUGAR_CANE);
    r = craft_result(sugar, 2);
    REQUIRE(r);
    CHECK(r->item == SUGAR);
}
