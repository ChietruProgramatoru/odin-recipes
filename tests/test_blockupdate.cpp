// Blocks that change by themselves (M6): flowing water and lava, falling sand and gravel, fire,
// drowning, and the data values saved with chunks.
#include <doctest/doctest.h>

#include "blockupdate.h"
#include "rng.h"
#include "save.h"
#include "survival.h"
#include "tools.h"
#include "world.h"

namespace {

// A stone floor at y = 0..9 (top at y = 10), as far as `radius` chunks out.
std::unique_ptr<Chunk> flat_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    return c;
}

struct Flat {
    World world{1};
    BlockUpdater blocks{world};
    Rng rng{42};
    BlockEvents events;
    bool raining = false;

    explicit Flat(int radius = 2) {
        for (int cx = -radius; cx <= radius; ++cx)
            for (int cz = -radius; cz <= radius; ++cz) world.insert(flat_chunk(cx, cz));
    }
    BlockId at(int x, int y, int z) { return world.get_block(x, y, z); }
    int data(int x, int y, int z) { return world.get_data(x, y, z); }
    void run(int ticks) {
        RainCheck rain = [this](glm::ivec3) { return raining; };
        for (int i = 0; i < ticks; ++i) blocks.tick(rng, rain, events);
    }
};

constexpr int FLOOR = 10;  // the first air above the stone

}  // namespace

TEST_CASE("chunks keep a 4-bit data value per block, and save it") {
    Flat f;
    f.world.set_block(3, FLOOR, 4, WATER, 5);
    CHECK(f.data(3, FLOOR, 4) == 5);
    f.world.set_data(3, FLOOR, 4, 18);  // only 4 bits are kept
    CHECK(f.data(3, FLOOR, 4) == 2);
    f.world.set_block(3, FLOOR, 4, STONE);
    CHECK(f.data(3, FLOOR, 4) == 0);  // setting a block resets its data

    Chunk& c = f.world.chunk(0, 0);
    c.data[Chunk::index(1, 20, 2)] = 7;
    c.data[Chunk::index(2, 20, 2)] = 15;
    auto back = decode_chunk(encode_chunk(c), 0, 0);
    REQUIRE(back);
    CHECK(back->data == c.data);
}

TEST_CASE("a water source on flat ground spreads 7 blocks, getting lower as it goes") {
    Flat f;
    f.world.set_block(0, FLOOR, 0, WATER);
    f.run(200);
    CHECK(f.at(0, FLOOR, 0) == WATER);
    CHECK(f.data(0, FLOOR, 0) == 0);
    for (int d = 1; d <= 7; ++d) {
        CHECK(f.at(d, FLOOR, 0) == WATER);
        CHECK(f.data(d, FLOOR, 0) == d);
        CHECK(f.at(0, FLOOR, -d) == WATER);
    }
    CHECK(f.at(8, FLOOR, 0) == AIR);
    CHECK(f.at(0, FLOOR + 1, 0) == AIR);  // nothing flows up
    CHECK(f.blocks.pending() == 0);       // and it has settled

    // Taking the source away makes the rest dry up.
    f.world.set_block(0, FLOOR, 0, AIR);
    f.run(200);
    for (int d = 0; d <= 7; ++d) CHECK(f.at(d, FLOOR, 0) == AIR);
}

TEST_CASE("water falls down, then spreads from where it lands") {
    Flat f;
    // A 3-high ledge of stone at x <= 0, the water poured on top of it at its edge.
    for (int x = -4; x <= 0; ++x)
        for (int z = -4; z <= 4; ++z)
            for (int y = FLOOR; y < FLOOR + 3; ++y) f.world.set_block(x, y, z, STONE);
    f.world.set_block(0, FLOOR + 3, 0, WATER);
    f.run(300);
    // It runs off the edge, falls (data 8 and up) and spreads on the floor below.
    CHECK(f.at(1, FLOOR + 3, 0) == WATER);
    CHECK(f.at(1, FLOOR + 1, 0) == WATER);
    CHECK(f.data(1, FLOOR + 1, 0) >= 8);
    CHECK(f.at(1, FLOOR, 0) == WATER);
    CHECK(f.at(4, FLOOR, 0) == WATER);
}

TEST_CASE("two water sources next to a gap make a new source (infinite water)") {
    Flat f;
    // A 3x1 trench with sources at both ends.
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z) f.world.set_block(x, FLOOR, z, STONE);
    for (int x = -1; x <= 1; ++x) f.world.set_block(x, FLOOR, 0, AIR);
    f.world.set_block(-1, FLOOR, 0, WATER);
    f.world.set_block(1, FLOOR, 0, WATER);
    f.run(100);
    CHECK(f.at(0, FLOOR, 0) == WATER);
    CHECK(f.data(0, FLOOR, 0) == 0);
}

TEST_CASE("lava flows only 3 blocks and slowly") {
    Flat f;
    f.world.set_block(0, FLOOR, 0, LAVA);
    f.run(25);
    CHECK(f.at(1, FLOOR, 0) == AIR);  // lava waits 30 ticks between steps
    f.run(600);
    CHECK(f.at(1, FLOOR, 0) == LAVA);
    CHECK(f.at(3, FLOOR, 0) == LAVA);
    CHECK(f.data(3, FLOOR, 0) == 6);
    CHECK(f.at(4, FLOOR, 0) == AIR);
}

TEST_CASE("lava meeting water turns to obsidian (source) or cobblestone (flowing)") {
    Flat f;
    f.world.set_block(0, FLOOR, 0, LAVA);
    f.world.set_block(1, FLOOR, 0, WATER);
    f.run(1);
    CHECK(f.at(0, FLOOR, 0) == OBSIDIAN);
    CHECK(f.events.fizz.size() == 1);

    f.world.set_block(5, FLOOR, 5, LAVA, 2);  // flowing lava
    f.world.set_block(5, FLOOR + 1, 5, WATER);
    f.run(1);
    CHECK(f.at(5, FLOOR, 5) == COBBLE);

    // Lava pouring down onto water makes stone.
    f.world.set_block(-5, FLOOR, -5, WATER);
    f.world.set_block(-5, FLOOR + 1, -5, LAVA, 8);  // it would harden if water were beside it, not below
    f.world.set_block(-5, FLOOR + 2, -5, LAVA);
    f.run(100);
    CHECK(f.at(-5, FLOOR, -5) == STONE);
}

TEST_CASE("water washes torches away and they drop") {
    Flat f;
    f.world.set_block(2, FLOOR, 0, TORCH);
    f.world.set_block(0, FLOOR, 0, WATER);
    f.run(100);
    CHECK(f.at(2, FLOOR, 0) == WATER);
    bool dropped = false;
    for (const auto& [p, item] : f.events.drops) dropped |= p == glm::ivec3(2, FLOOR, 0) && item == TORCH;
    CHECK(dropped);
}

TEST_CASE("the flow vector points down the slope of the water") {
    Flat f;
    f.world.set_block(0, FLOOR, 0, WATER);
    f.run(200);
    const glm::dvec3 v = flow_vector(f.world, {3, FLOOR, 0});
    CHECK(v.x > 0.9);  // away from the source
    CHECK(flow_vector(f.world, {0, FLOOR - 3, 0}) == glm::dvec3(0));  // stone has no flow
    // A body standing in the stream is carried along it.
    Body b(f.world, 3.5, FLOOR, 0.5, 0.3, 1.8);
    CHECK(b.check_water());
    CHECK(b.current.x > 0.9);
}

TEST_CASE("sand and gravel fall when the block under them goes, and land as blocks") {
    Flat f;
    f.world.set_block(0, FLOOR + 5, 0, SAND);  // hanging in the air: nothing happens until a neighbour changes
    f.world.set_block(1, FLOOR + 5, 0, STONE);
    f.world.set_block(1, FLOOR + 5, 0, AIR);
    f.run(5);
    CHECK(f.at(0, FLOOR + 5, 0) == AIR);
    REQUIRE(f.events.falling.size() == 1);
    CHECK(f.events.falling[0].second == SAND);

    FallingBlock fb(f.world, SAND, {0, FLOOR + 5, 0});
    for (int i = 0; i < 200 && !fb.tick(0.05); ++i) {}
    CHECK(fb.cell() == glm::ivec3(0, FLOOR, 0));
    CHECK(fb.can_land(f.world));
    // On a torch it can't land; it drops as an item instead.
    f.world.set_block(4, FLOOR, 4, TORCH);
    FallingBlock on_torch(f.world, GRAVEL, {4, FLOOR + 3, 4});
    for (int i = 0; i < 200 && !on_torch.tick(0.05); ++i) {}
    CHECK_FALSE(on_torch.can_land(f.world));
}

TEST_CASE("a sand tower falls one block after another") {
    Flat f;
    for (int y = FLOOR + 1; y < FLOOR + 4; ++y) f.world.set_block(0, y, 0, SAND);
    f.world.set_block(0, FLOOR, 0, AIR);  // nothing there anyway; the change wakes the sand above
    f.run(10);
    CHECK(f.events.falling.size() == 3);
}

TEST_CASE("fire spreads to planks and burns them away, and dies on stone") {
    Flat f;
    // A plank floor at FLOOR (on the stone), with a fire on it.
    for (int x = -3; x <= 3; ++x)
        for (int z = -3; z <= 3; ++z) f.world.set_block(x, FLOOR, z, PLANKS);
    f.world.set_block(0, FLOOR + 1, 0, FIRE);
    f.run(20 * 120);  // two minutes
    int planks = 0;
    for (int x = -3; x <= 3; ++x)
        for (int z = -3; z <= 3; ++z) planks += f.at(x, FLOOR, z) == PLANKS;
    CHECK(planks < 49);  // some burned

    // Fire on bare stone with nothing to burn goes out within a few ticks of fire.
    Flat g;
    g.world.set_block(0, FLOOR, 0, FIRE);
    g.run(20 * 120);
    CHECK(g.at(0, FLOOR, 0) == AIR);
}

TEST_CASE("fire can't float: it needs ground or something burnable beside it") {
    Flat f;
    CHECK(fire_can_stay(f.world, {0, FLOOR, 0}));
    CHECK_FALSE(fire_can_stay(f.world, {0, FLOOR + 3, 0}));
    f.world.set_block(1, FLOOR + 3, 0, LOG);
    CHECK(fire_can_stay(f.world, {0, FLOOR + 3, 0}));
    // A fire whose ground goes away goes out.
    f.world.set_block(5, FLOOR, 5, FIRE);
    f.run(1);
    f.world.set_block(5, FLOOR - 1, 5, AIR);
    f.run(1);
    CHECK(f.at(5, FLOOR, 5) == AIR);
}

TEST_CASE("rain puts fires out") {
    Flat f;
    for (int x = -2; x <= 2; ++x) f.world.set_block(x, FLOOR, 0, PLANKS);
    f.world.set_block(0, FLOOR + 1, 0, FIRE);
    f.raining = true;
    f.run(FIRE_TICKS + 2);
    CHECK(f.at(0, FLOOR + 1, 0) == AIR);
}

TEST_CASE("fire sets TNT off") {
    Flat f;
    f.world.set_block(1, FLOOR, 0, TNT);
    f.world.set_block(0, FLOOR, 0, FIRE);
    f.run(20 * 60);
    CHECK(f.at(1, FLOOR, 0) != TNT);
    CHECK(f.events.tnt.size() == 1);
}

TEST_CASE("drowning: 15 seconds of air, then 2 damage a second") {
    Vitals v;
    int damage = 0, first = -1;
    for (int t = 0; t < 400; ++t) {
        int d = v.breathe(true);
        if (d && first < 0) first = t;
        damage += d;
    }
    CHECK(first == MAX_AIR + 19);  // air runs out at 300 ticks, the first hurt comes a second later
    CHECK(damage == DROWN_DAMAGE * 5);
    CHECK(v.bubbles() == 0);
    v.breathe(false);
    CHECK(v.air == MAX_AIR);
    CHECK(v.bubbles() == 10);
}

TEST_CASE("obsidian needs a diamond pickaxe, gravel and sand are dug with shovels") {
    CHECK_FALSE(harvest_drop(IRON_PICKAXE, OBSIDIAN));
    CHECK(harvest_drop(DIAMOND_PICKAXE, OBSIDIAN) == OBSIDIAN);
    CHECK(falls(SAND));
    CHECK(falls(GRAVEL));
    CHECK_FALSE(falls(DIRT));
}
