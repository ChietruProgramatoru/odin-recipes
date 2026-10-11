// Transport (M12): rails joining up, minecarts rolling along them, boats floating and crashing, the
// storage minecart's chest, and vehicles saved with their chunk.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "blockupdate.h"
#include "container.h"
#include "rails.h"
#include "rng.h"
#include "save.h"
#include "vehicles.h"
#include "world.h"

namespace {

// A stone floor at y = 0..9 (top at y = 10).
std::unique_ptr<Chunk> flat_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    return c;
}

constexpr int FLOOR = 10;  // the first air above the stone

struct Flat {
    World world{1};
    explicit Flat(int radius = 2) {
        for (int cx = -radius; cx <= radius; ++cx)
            for (int cz = -radius; cz <= radius; ++cz) world.insert(flat_chunk(cx, cz));
    }
    // Put a rail down the way the game does: set it, then join it up.
    void rail(int x, int y, int z, BlockId b = RAIL) {
        world.set_block(x, y, z, b, b == POWERED_RAIL ? RAIL_POWERED : 0);
        connect_rail(world, {x, y, z});
    }
    int shape(int x, int y, int z) { return rail_shape(world.get_block(x, y, z), world.get_data(x, y, z)); }
    void ticks(Vehicle& v, int n, const RideInput* in = nullptr) {
        for (int i = 0; i < n; ++i) v.tick(in);
    }
};

}  // namespace

TEST_CASE("rails are thin blocks that drop themselves, and old rail items load as rails") {
    CHECK(is_rail(RAIL));
    CHECK(is_rail(POWERED_RAIL));
    CHECK(is_rail(DETECTOR_RAIL));
    CHECK_FALSE(is_solid(RAIL));
    CHECK_FALSE(is_opaque(RAIL));
    CHECK(light_opacity(RAIL) == 0);
    CHECK(block_info(RAIL)->drop == std::optional<ItemId>(RAIL));
    CHECK(current_item_id(OLD_RAIL_ITEM) == RAIL);
    CHECK(current_item_id(OLD_POWERED_RAIL_ITEM) == POWERED_RAIL);
    CHECK(current_item_id(OLD_DETECTOR_RAIL_ITEM) == DETECTOR_RAIL);
    CHECK(item_name(RAIL) == "Rail");
}

TEST_CASE("a rail joins the rails next to it, curving round corners") {
    Flat f;
    f.rail(5, FLOOR, 5);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_NORTH_SOUTH);  // on its own
    f.rail(6, FLOOR, 5);
    CHECK(f.shape(6, FLOOR, 5) == RAIL_EAST_WEST);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_EAST_WEST);  // turned to meet it
    f.rail(5, FLOOR, 6);
    CHECK(f.shape(5, FLOOR, 6) == RAIL_NORTH_SOUTH);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_SOUTH_EAST);  // the corner curves
    // A rail already joined at both ends doesn't turn for a third.
    f.rail(4, FLOOR, 5);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_SOUTH_EAST);
    CHECK(f.shape(4, FLOOR, 5) == RAIL_NORTH_SOUTH);
}

TEST_CASE("powered rails don't curve") {
    Flat f;
    f.rail(5, FLOOR, 5, POWERED_RAIL);
    f.rail(6, FLOOR, 5);
    f.rail(5, FLOOR, 6);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_EAST_WEST);
    CHECK(rail_powered(POWERED_RAIL, f.world.get_data(5, FLOOR, 5)));  // still powered after turning
}

TEST_CASE("a rail next to a higher one slopes up to it") {
    Flat f;
    f.world.set_block(6, FLOOR, 5, STONE);
    f.rail(5, FLOOR, 5);
    f.rail(6, FLOOR + 1, 5);
    CHECK(f.shape(5, FLOOR, 5) == RAIL_ASCENDING_EAST);
    CHECK(f.shape(6, FLOOR + 1, 5) == RAIL_EAST_WEST);
    CHECK(rail_can_stay(f.world, {5, FLOOR, 5}));
    // Without the block on its high side, it couldn't stay sloped.
    f.world.set_block(6, FLOOR, 5, AIR);
    CHECK_FALSE(rail_can_stay(f.world, {5, FLOOR, 5}));
}

TEST_CASE("a rail pops off when the block under it goes") {
    Flat f;
    BlockUpdater blocks(f.world);
    Rng rng(1);
    BlockEvents ev;
    f.rail(5, FLOOR, 5);
    f.world.take_changes();
    f.world.set_block(5, FLOOR - 1, 5, AIR);
    blocks.tick(rng, {}, ev);
    CHECK(f.world.get_block(5, FLOOR, 5) == AIR);
    REQUIRE(ev.drops.size() == 1);
    CHECK(ev.drops[0].second == RAIL);
}

TEST_CASE("the point on a rail follows its slope") {
    Flat f;
    f.world.set_block(6, FLOOR, 5, STONE);
    f.rail(5, FLOOR, 5);
    f.rail(6, FLOOR + 1, 5);
    auto low = point_on_rail(f.world, {5.1, FLOOR, 5.7});
    auto high = point_on_rail(f.world, {5.9, FLOOR, 5.2});
    REQUIRE(low);
    REQUIRE(high);
    CHECK(low->y == doctest::Approx(FLOOR + 0.1));
    CHECK(high->y == doctest::Approx(FLOOR + 0.9));
    CHECK(low->z == doctest::Approx(5.5));  // pulled onto the middle of the rail
    CHECK_FALSE(point_on_rail(f.world, {9.5, FLOOR, 9.5}));
}

TEST_CASE("a minecart rolls along a track, round a curve, and stops at the end") {
    Flat f;
    // East along z = 5 from x = 0 to 8, then a curve south and on to z = 12, with a wall after it.
    for (int x = 0; x <= 8; ++x) f.rail(x, FLOOR, 5);
    for (int z = 6; z <= 12; ++z) f.rail(8, FLOOR, z);
    f.world.set_block(8, FLOOR, 13, STONE);
    CHECK(f.shape(8, FLOOR, 5) == RAIL_SOUTH_WEST);
    // With someone in it, it hardly slows down (empty carts stop much sooner).
    Vehicle cart(f.world, VehicleKind::Minecart, 0.5, FLOOR, 5.5);
    cart.ridden = true;
    cart.motion.x = 0.4;
    f.ticks(cart, 40);
    CHECK(cart.on_rail);
    CHECK(cart.body.x == doctest::Approx(8.5));  // came round the corner
    CHECK(cart.body.z > 9);
    CHECK(cart.body.y == doctest::Approx(FLOOR));
    f.ticks(cart, 200);
    CHECK(cart.body.z < 13);  // stopped by the wall
    CHECK(std::hypot(cart.motion.x, cart.motion.z) < 0.01);
}

TEST_CASE("a minecart rolls down a slope and speeds up") {
    Flat f;
    // A slope rising to the east, then flat track going west at the bottom.
    for (int x = 0; x <= 5; ++x) f.rail(x, FLOOR, 5);
    f.world.set_block(7, FLOOR, 5, STONE);
    f.rail(7, FLOOR + 1, 5);
    f.world.set_block(6, FLOOR, 5, STONE);
    f.world.set_block(6, FLOOR, 5, AIR);
    f.world.set_block(6, FLOOR - 1, 5, STONE);
    f.rail(6, FLOOR, 5);
    CHECK(f.shape(6, FLOOR, 5) == RAIL_ASCENDING_EAST);
    Vehicle cart(f.world, VehicleKind::Minecart, 7.5, FLOOR + 1, 5.5);
    cart.motion.x = -0.05;
    f.ticks(cart, 30);
    CHECK(cart.body.x < 6);
    CHECK(cart.body.y == doctest::Approx(FLOOR));
    CHECK(std::abs(cart.motion.x) > 0.05);  // faster at the bottom than at the top
}

TEST_CASE("powered rails push minecarts, and unpowered ones stop them") {
    Flat f;
    for (int x = 0; x <= 20; ++x) f.rail(x, FLOOR, 5, x == 2 ? POWERED_RAIL : RAIL);
    Vehicle cart(f.world, VehicleKind::Minecart, 1.5, FLOOR, 5.5);
    cart.motion.x = 0.05;
    double fastest = 0;
    for (int i = 0; i < 30; ++i) {
        cart.tick(nullptr);
        fastest = std::max(fastest, cart.motion.x);
    }
    CHECK(fastest > 0.15);
    CHECK(cart.body.x > 5);
    // Switched off, the same rail is a brake.
    set_rail_powered(f.world, {2, FLOOR, 5}, false);
    Vehicle slow(f.world, VehicleKind::Minecart, 1.5, FLOOR, 5.5);
    slow.motion.x = 0.05;
    f.ticks(slow, 40);
    CHECK(slow.body.x < 3.5);
    CHECK(std::abs(slow.motion.x) < 0.01);
}

TEST_CASE("a powered minecart runs on coal") {
    Flat f;
    for (int x = 0; x <= 60; ++x) f.rail(x, FLOOR, 5);
    Vehicle cart(f.world, VehicleKind::PoweredMinecart, 2.5, FLOOR, 5.5);
    CHECK_FALSE(cart.add_fuel(DIRT, 1.5, 5.5));
    CHECK(cart.add_fuel(COAL, 1.5, 5.5));  // pushed away from the player, to the east
    CHECK(cart.fuel == COAL_FUEL_TICKS);
    f.ticks(cart, 100);
    CHECK(cart.body.x > 10);
    CHECK(cart.fuel == COAL_FUEL_TICKS - 100);
}

TEST_CASE("a boat floats, goes where its rider steers, and breaks up when it crashes") {
    Flat f;
    // A pool: water at y = 9 (the top layer of the floor) from x = 0 to 30, with a wall at x = 31.
    for (int x = 0; x <= 30; ++x)
        for (int z = 0; z <= 10; ++z) f.world.set_block(x, FLOOR - 1, z, WATER);
    for (int z = 0; z <= 10; ++z) f.world.set_block(31, FLOOR, z, STONE), f.world.set_block(31, FLOOR - 1, z, STONE);
    Vehicle boat(f.world, VehicleKind::Boat, 5.5, FLOOR + 0.7, 5.5);
    f.ticks(boat, 100);
    CHECK(boat.body.y > FLOOR - 1);  // floats on the surface
    CHECK(boat.body.y < FLOOR + 0.2);
    CHECK(boat.body.x == doctest::Approx(5.5));
    // Forward, looking east (yaw 90 faces +x).
    RideInput east{1, 0, 90};
    bool crashed = false;
    for (int i = 0; i < 200 && !crashed; ++i) crashed = boat.tick(&east);
    CHECK(crashed);
    CHECK(boat.body.x > 25);
    const auto left = boat.drops();
    REQUIRE(left.size() == 2);
    CHECK(left[0] == Stack{PLANKS, 3});
    CHECK(left[1] == Stack{STICK, 2});
    // Drifting gently into the wall doesn't break it.
    Vehicle slow(f.world, VehicleKind::Boat, 29.5, FLOOR - 0.3, 5.5);
    slow.motion.x = 0.05;
    bool broke = false;
    for (int i = 0; i < 100; ++i) broke |= slow.tick(nullptr);
    CHECK_FALSE(broke);
}

TEST_CASE("hits wear a vehicle down until it breaks, and a minecart drops what it is made of") {
    Flat f;
    Vehicle cart(f.world, VehicleKind::StorageMinecart, 2.5, FLOOR, 5.5);
    CHECK_FALSE(cart.hit(1));
    CHECK_FALSE(cart.hit(1));
    CHECK_FALSE(cart.hit(1));
    CHECK_FALSE(cart.hit(1));
    CHECK(cart.hit(1));  // 50 > 40
    cart.chest[3] = Stack{DIAMOND, 5};
    const auto left = cart.drops();
    REQUIRE(left.size() == 3);
    CHECK(left[0] == Stack{MINECART, 1});
    CHECK(left[1] == Stack{CHEST, 1});
    CHECK(left[2] == Stack{DIAMOND, 5});
}

TEST_CASE("vehicles are saved with their chunk") {
    Flat f;
    Vehicle cart(f.world, VehicleKind::StorageMinecart, 2.5, FLOOR, 5.5);
    cart.yaw = 90;
    cart.chest[0] = Stack{COBBLE, 64};
    cart.chest[26] = Stack{IRON_PICKAXE, 1, 17};
    Vehicle furnace(f.world, VehicleKind::PoweredMinecart, 4.5, FLOOR, 5.5);
    furnace.fuel = 900;
    furnace.push_x = -1;
    Vehicle boat(f.world, VehicleKind::Boat, 6.5, FLOOR, 5.5);
    Chunk& c = f.world.chunk(0, 0);
    c.entities = {cart.record(), furnace.record(), boat.record()};
    auto back = decode_chunk(encode_chunk(c), 0, 0);
    REQUIRE(back);
    REQUIRE(back->entities.size() == 3);
    CHECK(back->entities == c.entities);
    auto again = Vehicle::from_record(f.world, back->entities[0]);
    REQUIRE(again);
    CHECK(again->kind == VehicleKind::StorageMinecart);
    CHECK(again->yaw == doctest::Approx(90));
    CHECK(again->chest == cart.chest);
    auto fur = Vehicle::from_record(f.world, back->entities[1]);
    CHECK(fur->fuel == 900);
    CHECK(fur->push_x == doctest::Approx(-1));
    CHECK(Vehicle::from_record(f.world, back->entities[2])->kind == VehicleKind::Boat);
}

TEST_CASE("a chest screen moves stacks between the chest and the inventory") {
    Inventory inv;
    ChestSlots chest{};
    chest[0] = Stack{COBBLE, 10};
    inv.slots[0] = Stack{DIRT, 5};
    CraftingMenu menu(inv, chest);
    menu.click({Slot::Chest, 0}, 0, true);  // shift: chest -> inventory
    CHECK_FALSE(chest[0]);
    CHECK(inv.count(COBBLE) == 10);
    menu.click({Slot::Inv, 0}, 0, true);  // shift: inventory -> chest
    CHECK(chest[0] == Stack{DIRT, 5});
    menu.click({Slot::Chest, 0}, 0, false);  // pick it up
    CHECK(menu.cursor == Stack{DIRT, 5});
    menu.click({Slot::Chest, 26}, 0, false);
    CHECK(chest[26] == Stack{DIRT, 5});
}
