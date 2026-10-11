// Nature (M6 part 2): leaf decay, saplings, ice and snow, cactus and cobwebs.
#include <doctest/doctest.h>

#include <algorithm>

#include "blockupdate.h"
#include "farming.h"
#include "furnace.h"
#include "items.h"
#include "nature.h"
#include "tools.h"
#include "world.h"

namespace {

// Stone up to y = 8 with `top` at y = 9, so the ground is at y = 10.
std::unique_ptr<Chunk> ground_chunk(int cx, int cz, BlockId top) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = y == 9 ? top : STONE;
    c->max_y = 10;
    // A warm, wet climate (the default all-zero climate is a cold one).
    c->climate.temperature.fill(230);
    c->climate.rainfall.fill(230);
    return c;
}

constexpr int GROUND = 10;  // the first air above the ground

struct Land {
    World world{1};
    BlockUpdater blocks{world};
    Rng rng{42};
    BlockEvents events;

    explicit Land(BlockId top = GRASS, bool cold = false) {
        for (int cx = -1; cx <= 1; ++cx)
            for (int cz = -1; cz <= 1; ++cz) {
                auto c = ground_chunk(cx, cz, top);
                if (cold) {
                    c->climate.temperature.fill(0);
                    c->climate.rainfall.fill(0);
                }
                world.insert(std::move(c));
            }
        tick();  // settle the changes made so far
        events = {};
    }
    BlockId at(glm::ivec3 p) { return world.get_block(p.x, p.y, p.z); }
    void set(glm::ivec3 p, BlockId b, int data = 0) { world.set_block(p.x, p.y, p.z, b, data); }
    void tick(int n = 1) {
        RainCheck dry = [](glm::ivec3) { return false; };
        for (int i = 0; i < n; ++i) blocks.tick(rng, dry, events);
    }
    // Random ticks for the middle chunk, with block ticks between them.
    void random(int n) {
        RainCheck dry = [](glm::ivec3) { return false; };
        for (int i = 0; i < n; ++i) {
            blocks.tick(rng, dry, events);
            blocks.random_ticks({0, 0}, 0, rng, dry, events);
        }
    }
    bool dropped(ItemId item) const {
        return std::any_of(events.drops.begin(), events.drops.end(), [&](const auto& d) { return d.second == item; });
    }
};

// A trunk of 4 logs on (4, 10, 4) with a 5 x 5 layer of leaves at its top and a 3 x 3 layer above.
void small_tree(Land& l, BlockId leaves = LEAVES) {
    for (int y = GROUND; y < GROUND + 4; ++y) l.set({4, y, 4}, LOG);
    for (int dx = -2; dx <= 2; ++dx)
        for (int dz = -2; dz <= 2; ++dz) {
            if (dx || dz) l.set({4 + dx, GROUND + 3, 4 + dz}, leaves);
            if (std::abs(dx) <= 1 && std::abs(dz) <= 1) l.set({4 + dx, GROUND + 4, 4 + dz}, leaves);
        }
    l.tick();
}

int count_leaves(Land& l) {
    int n = 0;
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = -3; dz <= 3; ++dz)
            for (int y = GROUND; y < GROUND + 8; ++y) n += is_leaves(l.at({4 + dx, y, 4 + dz}));
    return n;
}

}  // namespace

TEST_CASE("leaves look for a log within 4 leaves") {
    Land l;
    l.set({0, GROUND, 0}, LOG);
    for (int x = 1; x <= 6; ++x) l.set({x, GROUND, 0}, LEAVES);
    CHECK(log_near(l.world, {1, GROUND, 0}));
    CHECK(log_near(l.world, {4, GROUND, 0}));   // 4 steps along the leaves
    CHECK_FALSE(log_near(l.world, {5, GROUND, 0}));
    l.set({3, GROUND, 0}, AIR);  // the path through the leaves is cut
    CHECK_FALSE(log_near(l.world, {4, GROUND, 0}));
}

TEST_CASE("leaves decay once their tree is chopped down, and only then") {
    Land l;
    small_tree(l);
    const int leaves = count_leaves(l);
    REQUIRE(leaves == 24 + 9);
    l.random(3000);
    CHECK(count_leaves(l) == leaves);  // a standing tree keeps its leaves

    for (int y = GROUND; y < GROUND + 4; ++y) l.set({4, y, 4}, AIR);
    l.tick();
    CHECK((l.world.get_data(5, GROUND + 3, 4) & LEAVES_CHECK) != 0);
    l.random(8000);
    CHECK(count_leaves(l) == 0);
}

TEST_CASE("leaves a player placed never decay") {
    Land l;
    l.set({4, GROUND, 4}, LOG);
    l.set({4, GROUND + 1, 4}, LEAVES, LEAVES_PLAYER);
    l.set({5, GROUND + 1, 4}, LEAVES);
    l.tick();
    l.set({4, GROUND, 4}, AIR);
    l.random(6000);
    CHECK(l.at({4, GROUND + 1, 4}) == LEAVES);
    CHECK(l.at({5, GROUND + 1, 4}) == AIR);
}

TEST_CASE("leaves drop a sapling 1 time in 20, of their own tree, and oak leaves apples") {
    Rng rng(5);
    int saplings = 0, apples = 0, spruce = 0;
    for (int i = 0; i < 4000; ++i) {
        for (ItemId item : leaf_drops(LEAVES, rng)) {
            saplings += item == SAPLING;
            apples += item == APPLE;
        }
        for (ItemId item : leaf_drops(SPRUCE_LEAVES, rng)) {
            CHECK(item == SPRUCE_SAPLING);
            ++spruce;
        }
    }
    CHECK(saplings > 140);
    CHECK(saplings < 260);
    CHECK(apples > 300);
    CHECK(apples < 500);
    CHECK(spruce > 140);
    CHECK(sapling_for(BIRCH_LEAVES) == BIRCH_SAPLING);
}

TEST_CASE("saplings grow into their tree with light, or at once with bone meal") {
    Land l;
    const glm::ivec3 oak{4, GROUND, 4}, spruce{-6, GROUND, 5};
    CHECK(can_stay(l.world, oak, SAPLING));
    CHECK_FALSE(can_stay(l.world, {4, GROUND - 3, 4}, SAPLING));  // not on stone
    CHECK(planted_block(BIRCH_SAPLING) == BIRCH_SAPLING);

    FarmEvents ev;
    l.set(spruce, SPRUCE_SAPLING);
    CHECK(bone_meal(l.world, spruce, l.rng, ev));
    CHECK(l.at(spruce) == SPRUCE_LOG);
    CHECK(l.at(spruce - glm::ivec3(0, 1, 0)) == DIRT);
    CHECK(!ev.rebuild.empty());

    l.set(oak, SAPLING);
    const TickContext day{0, false};
    int ticks = 0;
    while (l.at(oak) == SAPLING && ticks < 5000) {
        random_tick(l.world, oak, l.rng, day, ev);
        ++ticks;
    }
    CHECK(l.at(oak) == LOG);
    CHECK(ticks > 2);  // 1 in 30 to get ready, 1 in 30 more to grow

    // No room under a roof: the sapling stays (bone meal is still used up, like Beta).
    const glm::ivec3 roofed{-4, GROUND, -4};
    l.set(roofed, BIRCH_SAPLING);
    l.set(roofed + glm::ivec3(0, 3, 0), STONE);
    CHECK(bone_meal(l.world, roofed, l.rng, ev));
    CHECK(l.at(roofed) == BIRCH_SAPLING);
}

TEST_CASE("saplings and their drops: fuel, and knocked off with their block") {
    CHECK(fuel_ticks(SAPLING) == 100);
    CHECK(fuel_ticks(SPRUCE_SAPLING) == 100);
    Rng rng(1);
    CHECK(farm_drops(BIRCH_SAPLING, 0, rng) == std::vector<ItemId>{BIRCH_SAPLING});
    CHECK(item_info(SAPLING)->name == "Oak Sapling");
    CHECK(is_plant(SAPLING));
    CHECK_FALSE(is_solid(SAPLING));
}

TEST_CASE("ice melts into water next to a light, and snow layers go") {
    Land l;
    const glm::ivec3 ice{2, GROUND, 2}, snow{12, GROUND, 12}, dark_ice{-6, GROUND, -6};  // out of reach of the meltwater
    l.set(ice, ICE);
    l.set(snow, SNOW_LAYER);
    l.set(dark_ice, ICE);
    l.random(3000);
    CHECK(l.at(ice) == ICE);  // daylight doesn't melt them
    CHECK(l.at(snow) == SNOW_LAYER);
    l.set(ice + glm::ivec3(1, 0, 0), GLOWSTONE);
    l.set(snow + glm::ivec3(0, 1, 0), GLOWSTONE);
    CHECK(melts(l.world, ice));
    CHECK(melts(l.world, snow));
    l.random(8000);
    CHECK(l.at(ice) == WATER);
    CHECK(l.world.get_data(ice.x, ice.y, ice.z) == 0);  // a source
    CHECK(l.at(snow) == AIR);
    CHECK(l.at(dark_ice) == ICE);
}

TEST_CASE("snow layers need something to lie on, and are walked through") {
    Land l;
    const glm::ivec3 p{3, GROUND, 3};
    CHECK(snow_can_stay(l.world, p));
    CHECK_FALSE(snow_can_stay(l.world, p + glm::ivec3(0, 1, 0)));  // on air
    l.set(p - glm::ivec3(0, 1, 0), ICE);
    CHECK_FALSE(snow_can_stay(l.world, p));
    l.set(p - glm::ivec3(0, 1, 0), GRASS);
    l.set(p, SNOW_LAYER);
    CHECK_FALSE(is_solid(SNOW_LAYER));
    CHECK(harvest_drop(WOOD_SHOVEL, SNOW_LAYER) == SNOWBALL);
    l.tick();
    l.set(p - glm::ivec3(0, 1, 0), AIR);  // the ground goes from under it
    l.tick();
    CHECK(l.at(p) == AIR);
}

TEST_CASE("in cold biomes still water freezes, and snow settles while it snows") {
    Land l(GRASS, true);
    const glm::ivec3 pond{5, GROUND - 1, 5};
    l.set(pond, WATER);
    l.tick();
    for (int i = 0; i < 40000; ++i) l.blocks.snow_ticks({0, 0}, 1, false, l.events);
    CHECK(l.at(pond) == ICE);
    int snow = 0;
    for (int x = -16; x < 32; ++x) snow += l.at({x, GROUND, 3}) == SNOW_LAYER;
    CHECK(snow == 0);  // no snow without snowfall

    for (int i = 0; i < 20000; ++i) l.blocks.snow_ticks({0, 0}, 1, true, l.events);
    for (int x = -16; x < 32; ++x) snow += l.at({x, GROUND, 3}) == SNOW_LAYER;
    CHECK(snow > 10);
    CHECK(l.at(pond + glm::ivec3(0, 1, 0)) == AIR);  // not on ice

    Land warm;
    for (int i = 0; i < 20000; ++i) warm.blocks.snow_ticks({0, 0}, 1, true, warm.events);
    int warm_snow = 0;
    for (int x = -16; x < 32; ++x) warm_snow += warm.at({x, GROUND, 3}) == SNOW_LAYER;
    CHECK(warm_snow == 0);
}

TEST_CASE("cactus stands on sand with nothing beside it, and grows to three tall") {
    Land l(SAND);
    const glm::ivec3 p{4, GROUND, 4};
    CHECK(cactus_can_stay(l.world, p));
    CHECK(planted_block(CACTUS) == CACTUS);
    l.set(p, CACTUS);
    l.tick();
    l.random(30000);
    CHECK(l.at(p + glm::ivec3(0, 1, 0)) == CACTUS);
    CHECK(l.at(p + glm::ivec3(0, 2, 0)) == CACTUS);
    CHECK(l.at(p + glm::ivec3(0, 3, 0)) == AIR);

    // A block put next to the top piece knocks it off, and only it.
    l.set(p + glm::ivec3(1, 2, 0), STONE);
    l.tick();
    CHECK(l.at(p + glm::ivec3(0, 2, 0)) == AIR);
    CHECK(l.dropped(CACTUS));
    CHECK(l.at(p + glm::ivec3(0, 1, 0)) == CACTUS);
    // The sand going takes the whole stalk.
    l.events = {};
    l.set(p - glm::ivec3(0, 1, 0), STONE);
    l.tick(3);
    CHECK(l.at(p) == AIR);
    CHECK(l.at(p + glm::ivec3(0, 1, 0)) == AIR);

    Land grass;
    CHECK_FALSE(cactus_can_stay(grass.world, p));
    CHECK(smelt_result(CACTUS)->item == CACTUS_GREEN);
}

TEST_CASE("cactus hurts what touches it, from the side or on top") {
    Land l(SAND);
    l.set({4, GROUND, 4}, CACTUS);
    Body beside(l.world, 5.3 + 1e-9, GROUND, 4.5, 0.3, 1.8);  // pressed against its +x side
    CHECK(touches_cactus(l.world, beside));
    Body away(l.world, 5.5, GROUND, 4.5, 0.3, 1.8);
    CHECK_FALSE(touches_cactus(l.world, away));
    Body on_top(l.world, 4.5, GROUND + 15.0 / 16, 4.5, 0.3, 1.8);
    CHECK(touches_cactus(l.world, on_top));
    CHECK(block_height(CACTUS) == doctest::Approx(15.0 / 16));
    CHECK(is_solid(CACTUS));
    CHECK_FALSE(is_opaque(CACTUS));
}

TEST_CASE("cobwebs slow everything in them and stop falls; swords and shears cut them") {
    Land l;
    l.set({4, GROUND, 4}, COBWEB);
    Body b(l.world, 4.5, GROUND, 4.5, 0.3, 1.8);
    b.move(0.4, 0, 0);
    CHECK(b.in_web);
    CHECK(b.x == doctest::Approx(4.6));
    Body falling(l.world, 4.5, GROUND + 0.5, 4.5, 0.3, 1.8);
    falling.vy = -20;
    falling.move(0, -0.4, 0);
    CHECK(falling.y == doctest::Approx(GROUND + 0.48));
    CHECK(falling.vy >= -1.6);
    Body free(l.world, 8.5, GROUND, 8.5, 0.3, 1.8);
    free.move(0.4, 0, 0);
    CHECK_FALSE(free.in_web);
    CHECK(free.x == doctest::Approx(8.9));

    CHECK(harvest_drop(AIR, COBWEB) == STRING);
    CHECK(*break_seconds(WOOD_SWORD, COBWEB) < *break_seconds(AIR, COBWEB) / 10);
    CHECK(*break_seconds(SHEARS, COBWEB) < *break_seconds(AIR, COBWEB) / 10);
}

TEST_CASE("world generation lays snow in cold biomes and grows cactus in deserts") {
    TerrainGenerator gen(7);
    int snowy = 0, cold_tops = 0, cacti = 0;
    for (int cx = -40; cx < 40; cx += 5)
        for (int cz = -40; cz < 40; cz += 5) {
            Biome b = gen.biome_at(cx * CHUNK + 8, cz * CHUNK + 8);
            if (!is_cold(b) && b != Biome::DESERT) continue;
            auto c = gen.generate(cx, cz);
            for (int i = 0; i < CHUNK * CHUNK; ++i) {
                int x = i & 15, z = i >> 4;
                int top = HEIGHT - 1;
                while (top > 0 && c->blocks[Chunk::index(x, top, z)] == AIR) --top;
                const BlockId t = c->blocks[Chunk::index(x, top, z)];
                if (is_cold(c->climate.biome((z << 4) | x)) && c->blocks[Chunk::index(x, top - 1, z)] != ICE && t != ICE) {
                    ++cold_tops;
                    snowy += t == SNOW_LAYER;
                }
                for (int y = 0; y < HEIGHT; ++y) cacti += c->blocks[Chunk::index(x, y, z)] == CACTUS;
            }
        }
    REQUIRE(cold_tops > 0);
    CHECK(snowy > cold_tops * 3 / 4);
    CHECK(cacti > 0);
}
