// Redstone (M13): dust, torches, levers, buttons, plates, repeaters, doors, TNT, powered rails and
// pistons, run through the block ticks the way the game runs them.
#include <doctest/doctest.h>

#include "bed.h"
#include "blockupdate.h"
#include "building.h"
#include "rails.h"
#include "redstone.h"
#include "world.h"

namespace {

// Stone up to y = 9, so the ground is at y = 10.
std::unique_ptr<Chunk> stone_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    return c;
}

struct Bench {
    World w{1};
    BlockUpdater u{w};
    Rng rng{1};
    BlockEvents last;  // everything the ticks did
    Bench() {
        for (int cx = -2; cx <= 2; ++cx)
            for (int cz = -2; cz <= 2; ++cz) w.insert(stone_chunk(cx, cz));
        w.take_changes();
    }
    void set(glm::ivec3 p, BlockId b, int data = 0) { w.set_block(p.x, p.y, p.z, b, data); }
    BlockId at(glm::ivec3 p) { return w.get_block(p.x, p.y, p.z); }
    int data(glm::ivec3 p) { return w.get_data(p.x, p.y, p.z); }
    void run(int ticks) {
        RainCheck dry = [](glm::ivec3) { return false; };
        for (int i = 0; i < ticks; ++i) {
            BlockEvents ev;
            u.tick(rng, dry, ev);
            for (auto& t : ev.redstone.tnt) last.redstone.tnt.push_back(t);
            for (auto& t : ev.redstone.notes) last.redstone.notes.push_back(t);
            for (auto& t : ev.redstone.dispense) last.redstone.dispense.push_back(t);
            for (auto& t : ev.redstone.drops) last.redstone.drops.push_back(t);
            for (auto& t : ev.redstone.pistons) last.redstone.pistons.push_back(t);
        }
    }
    bool use(glm::ivec3 p) {
        std::vector<ChunkPos> dirty;
        RedstoneEvents ev;
        return u.redstone().use(p, u.now(), dirty, ev);
    }
};

constexpr int G = 10;  // the first air layer
const glm::ivec3 UP{0, 1, 0};

}  // namespace

TEST_CASE("a lever powers dust, which loses one a block and reaches 15 blocks") {
    Bench b;
    const glm::ivec3 lever{0, G, 0};
    b.set(lever, LEVER_BLOCK, 0);
    for (int x = 1; x <= 17; ++x) b.set({x, G, 0}, REDSTONE_WIRE);
    b.run(3);
    CHECK(b.data({1, G, 0}) == 0);
    CHECK(b.use(lever));
    b.run(3);
    CHECK(b.data({1, G, 0}) == 15);
    CHECK(b.data({2, G, 0}) == 14);
    CHECK(b.data({15, G, 0}) == 1);
    CHECK(b.data({16, G, 0}) == 0);
    CHECK(b.use(lever));
    b.run(3);
    for (int x = 1; x <= 17; ++x) CHECK(b.data({x, G, 0}) == 0);
}

TEST_CASE("dust joins the dust beside it and runs up and down steps") {
    Bench b;
    b.set({0, G, 0}, REDSTONE_WIRE);
    b.set({1, G, 0}, REDSTONE_WIRE);
    CHECK(wire_joins(b.w, {0, G, 0}, {1, 0, 0}));
    CHECK_FALSE(wire_joins(b.w, {0, G, 0}, {0, 0, 1}));
    // A line points along itself, not sideways.
    CHECK(wire_points(b.w, {0, G, 0}, {-1, 0, 0}));
    CHECK_FALSE(wire_points(b.w, {0, G, 0}, {0, 0, 1}));
    // Up a step onto a block, and down off it.
    b.set({2, G, 0}, STONE);
    b.set({2, G + 1, 0}, REDSTONE_WIRE);
    CHECK(wire_joins(b.w, {1, G, 0}, {1, 0, 0}));
    CHECK(wire_joins(b.w, {2, G + 1, 0}, {-1, 0, 0}));
    // A block on top of the lower dust cuts the step.
    b.set({1, G + 1, 0}, STONE);
    CHECK_FALSE(wire_joins(b.w, {1, G, 0}, {1, 0, 0}));
    // Power goes up the step too.
    b.set({1, G + 1, 0}, AIR);
    b.set({-1, G, 0}, LEVER_BLOCK, LEVER_ON);
    b.run(3);
    CHECK(b.data({0, G, 0}) == 15);
    CHECK(b.data({2, G + 1, 0}) == 13);
}

TEST_CASE("a redstone torch goes out when its block is powered, and back on when it isn't") {
    Bench b;
    const glm::ivec3 block{0, G, 0}, torch{1, G, 0};  // a torch on the block's east side
    b.set(block, STONE);
    b.set(torch, REDSTONE_TORCH_ON, 1 + 1);  // the wall is to its west (facing 1)
    CHECK(redstone_support(REDSTONE_TORCH_ON, 2) == glm::ivec3(-1, 0, 0));
    const glm::ivec3 lever = block + UP;  // a lever on top of the block powers it
    b.set(lever, LEVER_BLOCK, 0);
    // Dust after the torch shows its power.
    b.set({2, G, 0}, REDSTONE_WIRE);
    b.run(4);
    CHECK(b.at(torch) == REDSTONE_TORCH_ON);
    CHECK(b.data({2, G, 0}) == 15);
    b.use(lever);
    b.run(1);
    CHECK(b.at(torch) == REDSTONE_TORCH_ON);  // a torch takes two ticks to turn
    b.run(4);
    CHECK(b.at(torch) == REDSTONE_TORCH_OFF);
    CHECK(b.data({2, G, 0}) == 0);
    b.use(lever);
    b.run(5);
    CHECK(b.at(torch) == REDSTONE_TORCH_ON);
    CHECK(b.data({2, G, 0}) == 15);
}

TEST_CASE("a torch powers the block above it, which powers the dust beside that") {
    Bench b;
    b.set({0, G, 0}, REDSTONE_TORCH_ON, 0);
    b.set({0, G + 1, 0}, STONE);
    b.set({1, G + 1, 0}, STONE);
    b.set({1, G + 2, 0}, REDSTONE_WIRE);  // on the next block: not touching the torch's block
    b.set({0, G + 2, 0}, REDSTONE_WIRE);  // on top of the torch's block
    b.run(3);
    CHECK(strong_power(b.w, {0, G + 1, 0}, true) == 15);
    CHECK(b.data({0, G + 2, 0}) == 15);
    CHECK(b.data({1, G + 2, 0}) == 14);
    // The torch doesn't power the block it stands on.
    CHECK(strong_power(b.w, {0, G - 1, 0}, true) == 0);
}

TEST_CASE("a repeater waits its delay and sends full power on") {
    Bench b;
    const glm::ivec3 lever{0, G, 0}, rep{3, G, 0};
    b.set(lever, LEVER_BLOCK, 0);
    b.set({1, G, 0}, REDSTONE_WIRE);
    b.set({2, G, 0}, REDSTONE_WIRE);
    b.set(rep, REPEATER_OFF, 3 | (2 << 2));  // facing east, delay 3
    CHECK(repeater_delay(b.data(rep)) == 3);
    for (int x = 4; x <= 6; ++x) b.set({x, G, 0}, REDSTONE_WIRE);
    b.run(3);
    b.use(lever);
    b.run(2);  // the dust carries it to the repeater
    CHECK(b.at(rep) == REPEATER_OFF);
    b.run(6);
    CHECK(b.at(rep) == REPEATER_ON);
    b.run(2);
    CHECK(b.data({4, G, 0}) == 15);
    CHECK(b.data({6, G, 0}) == 13);
    // Power only goes out of the front: the dust behind it keeps the lever's power.
    CHECK(b.data({2, G, 0}) == 14);
    // Right-clicking steps the delay round: 3, 4, 1.
    b.use(rep);
    CHECK(repeater_delay(b.data(rep)) == 4);
    b.use(rep);
    CHECK(repeater_delay(b.data(rep)) == 1);
}

TEST_CASE("buttons spring back and plates rise again once nothing is on them") {
    Bench b;
    const glm::ivec3 button{1, G, 0}, plate{0, G, 3};
    b.set({0, G, 0}, STONE);
    b.set(button, BUTTON_BLOCK, 1);  // its wall is to the west
    b.set({2, G, 0}, REDSTONE_WIRE);
    CHECK(b.use(button));
    b.run(2);
    CHECK(b.data({2, G, 0}) == 15);
    b.run(BUTTON_TICKS);
    CHECK((b.data(button) & BUTTON_PRESSED) == 0);
    CHECK(b.data({2, G, 0}) == 0);

    bool standing = true;
    b.u.set_plate_sensor([&](glm::ivec3, BlockId) { return standing; });
    b.set(plate, STONE_PLATE_BLOCK);
    b.set(plate + glm::ivec3(1, 0, 0), REDSTONE_WIRE);
    std::vector<ChunkPos> dirty;
    RedstoneEvents ev;
    b.u.redstone().press_plate(plate, b.u.now(), dirty, ev);
    b.run(PLATE_TICKS * 2);
    CHECK(b.data(plate) == PLATE_PRESSED);
    CHECK(b.data(plate + glm::ivec3(1, 0, 0)) == 15);
    standing = false;
    b.run(PLATE_TICKS + 2);
    CHECK(b.data(plate) == 0);
    CHECK(b.data(plate + glm::ivec3(1, 0, 0)) == 0);
}

TEST_CASE("power opens doors and trapdoors and shuts them again; a door opened by hand stays open") {
    Bench b;
    const glm::ivec3 door{0, G, 0}, lever{1, G, 0}, hand{4, G, 0};
    b.set(door, IRON_DOOR_BLOCK, 0);
    b.set(door + UP, IRON_DOOR_BLOCK, DOOR_UPPER);
    b.set(lever, LEVER_BLOCK, 0);
    b.run(3);
    CHECK_FALSE(door_open(b.data(door)));
    b.use(lever);
    b.run(3);
    CHECK(door_open(b.data(door)));
    CHECK(door_open(b.data(door + UP)));
    b.use(lever);
    b.run(3);
    CHECK_FALSE(door_open(b.data(door)));
    // A wooden door opened by hand isn't shut by something else changing nearby.
    b.set(hand, WOOD_DOOR_BLOCK, DOOR_OPEN);
    b.set(hand + UP, WOOD_DOOR_BLOCK, DOOR_OPEN | DOOR_UPPER);
    b.run(2);
    b.set(hand + glm::ivec3(1, 0, 0), COBBLE);
    b.run(3);
    CHECK(door_open(b.data(hand)));
    // Trapdoors too.
    const glm::ivec3 trap{0, G, 4};
    b.set(trap, TRAPDOOR, 0);
    b.set(trap + glm::ivec3(1, 0, 0), LEVER_BLOCK, LEVER_ON);
    b.run(3);
    CHECK((b.data(trap) & TRAPDOOR_OPEN) != 0);
}

TEST_CASE("powered TNT is lit, a note block plays once each time it is powered, a dispenser fires") {
    Bench b;
    b.set({0, G, 0}, TNT);
    b.set({1, G, 0}, LEVER_BLOCK, 0);
    b.set({2, G, 0}, NOTE_BLOCK);
    b.set({1, G, 2}, DISPENSER_BLOCK, 0);
    b.set({1, G, 1}, LEVER_BLOCK, 0);
    b.run(3);
    b.use({1, G, 0});
    b.use({1, G, 1});
    b.run(DISPENSER_TICKS + 3);
    CHECK(b.at({0, G, 0}) == AIR);
    CHECK(b.last.redstone.tnt.size() == 1);
    CHECK(b.last.redstone.notes.size() == 1);
    CHECK(b.last.redstone.dispense.size() == 1);
    // Staying powered doesn't play again.
    b.run(10);
    CHECK(b.last.redstone.notes.size() == 1);
    b.use({1, G, 0});
    b.run(3);
    b.use({1, G, 0});
    b.run(3);
    CHECK(b.last.redstone.notes.size() == 2);
}

TEST_CASE("a powered rail takes power from beside it and passes it 8 rails along") {
    Bench b;
    for (int x = 0; x <= 10; ++x) b.set({x, G, 0}, POWERED_RAIL, RAIL_EAST_WEST);
    b.set({0, G, 1}, REDSTONE_TORCH_ON, 0);
    b.run(4);
    CHECK(rail_powered(POWERED_RAIL, b.data({0, G, 0})));
    CHECK(rail_powered(POWERED_RAIL, b.data({8, G, 0})));
    CHECK_FALSE(rail_powered(POWERED_RAIL, b.data({9, G, 0})));
    b.set({0, G, 1}, AIR);
    b.run(12);
    for (int x = 0; x <= 10; ++x) CHECK_FALSE(rail_powered(POWERED_RAIL, b.data({x, G, 0})));
}

TEST_CASE("a detector rail pressed by a cart powers what is next to it") {
    Bench b;
    b.set({0, G, 0}, DETECTOR_RAIL, RAIL_NORTH_SOUTH);
    b.set({1, G, 0}, REDSTONE_WIRE);
    b.run(2);
    CHECK(b.data({1, G, 0}) == 0);
    set_rail_powered(b.w, {0, G, 0}, true);
    b.run(2);
    CHECK(b.data({1, G, 0}) == 15);
}

TEST_CASE("a piston pushes up to 12 blocks, and a sticky piston pulls one back") {
    Bench b;
    const glm::ivec3 piston{0, G, 0}, lever{0, G, -1};
    b.set(piston, PISTON_BASE, 5);  // pushes east
    for (int x = 1; x <= 12; ++x) b.set({x, G, 0}, x % 2 ? COBBLE : PLANKS);
    b.set(lever, LEVER_BLOCK, 0);
    b.run(2);
    b.use(lever);
    b.run(2);
    CHECK((b.data(piston) & PISTON_EXTENDED) != 0);
    CHECK(b.at({1, G, 0}) == PISTON_HEAD);
    CHECK(b.at({2, G, 0}) == COBBLE);
    CHECK(b.at({13, G, 0}) == PLANKS);
    REQUIRE(b.last.redstone.pistons.size() == 1);
    CHECK(b.last.redstone.pistons[0].moved_to.size() == 13);
    // Pulling back leaves the blocks where they went.
    b.use(lever);
    b.run(2);
    CHECK((b.data(piston) & PISTON_EXTENDED) == 0);
    CHECK(b.at({1, G, 0}) == AIR);
    CHECK(b.at({2, G, 0}) == COBBLE);
    // Thirteen in a row is too many.
    b.set({1, G, 0}, COBBLE);
    b.use(lever);
    b.run(2);
    CHECK((b.data(piston) & PISTON_EXTENDED) == 0);
    CHECK(b.at({1, G, 0}) == COBBLE);
    b.use(lever);
    b.run(2);

    // Sticky: pushes one, then pulls it back. Obsidian doesn't move; dust in the way breaks off.
    const glm::ivec3 sticky{0, G, 5}, slever{0, G, 4};
    b.set(sticky, STICKY_PISTON_BASE, 5);
    b.set({1, G, 5}, SAND);
    b.set({2, G, 5}, REDSTONE_WIRE);
    b.set(slever, LEVER_BLOCK, 0);
    b.run(2);
    b.use(slever);
    b.run(2);
    CHECK(b.at({1, G, 5}) == PISTON_HEAD);
    CHECK((b.data({1, G, 5}) & PISTON_HEAD_STICKY) != 0);
    CHECK(b.at({2, G, 5}) == SAND);
    bool dust_dropped = false;
    for (const PistonMove& m : b.last.redstone.pistons)
        for (const auto& d : m.drops) dust_dropped |= d.second == REDSTONE;
    CHECK(dust_dropped);
    b.use(slever);
    b.run(2);
    CHECK(b.at({1, G, 5}) == SAND);
    CHECK(b.at({2, G, 5}) == AIR);
    b.set({1, G, 5}, OBSIDIAN);
    b.use(slever);
    b.run(2);
    CHECK((b.data(sticky) & PISTON_EXTENDED) == 0);
}

TEST_CASE("pistons push up, and a piston and its head break together") {
    Bench b;
    const glm::ivec3 piston{0, G, 0};
    b.set(piston, PISTON_BASE, 1);  // up
    b.set({0, G + 1, 0}, DIRT);
    b.set({1, G, 0}, REDSTONE_TORCH_ON, 0);
    b.run(3);
    CHECK(b.at({0, G + 1, 0}) == PISTON_HEAD);
    CHECK(b.at({0, G + 2, 0}) == DIRT);
    CHECK(piston_cells(b.w, piston).size() == 2);
    CHECK(piston_cells(b.w, {0, G + 1, 0}).size() == 2);
    CHECK(joined_cells(b.w, {0, G + 1, 0}).size() == 2);
    CHECK(data_drops(PISTON_HEAD, b.data({0, G + 1, 0})) == std::vector<ItemId>{PISTON});
    // The head goes by itself if its piston does.
    b.set(piston, AIR);
    b.run(2);
    CHECK(b.at({0, G + 1, 0}) == AIR);
}

TEST_CASE("dust, torches and levers drop off when their block goes") {
    Bench b;
    b.set({0, G, 0}, STONE);
    b.set({0, G + 1, 0}, REDSTONE_WIRE);
    b.set({1, G, 0}, LEVER_BLOCK, 1 + 1);  // on the east side of the block (wall to the west)
    b.run(2);
    b.set({0, G, 0}, AIR);
    b.run(2);
    CHECK(b.at({0, G + 1, 0}) == AIR);
    CHECK(b.at({1, G, 0}) == AIR);
    CHECK(b.last.redstone.drops.size() == 2);
}

TEST_CASE("redstone placement: dust on the floor, torches on walls, pistons facing the player") {
    Bench b;
    const glm::ivec3 floor{4, G - 1, 4};
    auto dust = redstone_placement(b.w, REDSTONE, floor, UP, 0, 0);
    REQUIRE(dust);
    CHECK(dust->block == REDSTONE_WIRE);
    CHECK(dust->pos == floor + UP);
    b.set({4, G, 4}, STONE);
    auto torch = redstone_placement(b.w, REDSTONE_TORCH, {4, G, 4}, {1, 0, 0}, 0, 0);
    REQUIRE(torch);
    CHECK(torch->block == REDSTONE_TORCH_ON);
    CHECK(redstone_support(torch->block, torch->data) == glm::ivec3(-1, 0, 0));
    CHECK_FALSE(redstone_placement(b.w, STONE_BUTTON, {4, G, 4}, UP, 0, 0));
    // Looking south (yaw 0) the piston's front faces north, back at the player.
    b.set({4, G, 4}, AIR);
    auto piston = redstone_placement(b.w, PISTON, floor, UP, 0, 0);
    REQUIRE(piston);
    CHECK(dir6(piston->data & 7) == glm::ivec3(0, 0, -1));
    auto looking_down = redstone_placement(b.w, STICKY_PISTON, floor, UP, 0, 80);
    REQUIRE(looking_down);
    CHECK(looking_down->block == STICKY_PISTON_BASE);
    CHECK(dir6(looking_down->data & 7) == UP);
    auto rep = redstone_placement(b.w, REPEATER, floor, UP, 90, 0);
    REQUIRE(rep);
    CHECK(facing_dir(repeater_facing(rep->data)) == facing_dir(facing_from_yaw(90)));
    CHECK(is_redstone_item(LEVER));
    CHECK(redstone_drop(REDSTONE_TORCH_OFF, 0) == REDSTONE_TORCH);
}

TEST_CASE("a dispenser keeps its items when its data changes") {
    Bench b;
    const glm::ivec3 p{0, G, 0};
    b.set(p, DISPENSER_BLOCK, 0);
    Chunk* c = b.w.chunk_at(p);
    c->chests[cell_index(p)][0] = Stack{ARROW, 5};
    b.set(p, DISPENSER_BLOCK, 2);
    CHECK(c->chests.count(cell_index(p)) == 1);
    b.set(p, AIR);
    CHECK(c->chests.count(cell_index(p)) == 0);
}
