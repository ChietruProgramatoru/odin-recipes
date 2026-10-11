// Finding the way and fishing (M15): the compass and clock needles and icons, maps filling in and
// saving, and the fishing bobber flying, floating, biting and landing.
#include <doctest/doctest.h>

#include <cmath>

#include "fishing.h"
#include "items.h"
#include "navigation.h"
#include "rng.h"
#include "save.h"
#include "world.h"

namespace {

constexpr double PI = 3.14159265358979323846;

// Stone up to y = 9 under grass, with a pool of water 3 deep (y = 7..9) at blocks 0..7 of chunk (0, 0).
std::unique_ptr<Chunk> pool_chunk(int cx, int cz, bool pool) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < CHUNK; ++z)
            for (int x = 0; x < CHUNK; ++x)
                c->blocks[Chunk::index(x, y, z)] = pool && y >= 7 && x < 8 && z < 8 ? WATER : y == 9 ? GRASS : STONE;
    c->max_y = 10;
    return c;
}

struct Pond {
    World world{1};
    explicit Pond(int radius = 2) {
        for (int cx = -radius; cx <= radius; ++cx)
            for (int cz = -radius; cz <= radius; ++cz) world.insert(pool_chunk(cx, cz, cx == 0 && cz == 0));
    }
};

// The middle column of the red half of the needle.
double needle_x(const Image& img) {
    double sum = 0;
    int n = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (img.at(x, y).r == 210 && img.at(x, y).g == 30) {
                sum += x;
                ++n;
            }
    return n ? sum / n : -1;
}

}  // namespace

TEST_CASE("the compass points to the spawn, turning with the player") {
    const glm::dvec3 spawn(0, 64, 0);
    // Facing +z (yaw 0) with the spawn straight ahead, behind, to the left (+x) and the right.
    CHECK(compass_target({0, 64, -10}, 0, spawn) == doctest::Approx(0));
    CHECK(std::abs(compass_target({0, 64, 10}, 0, spawn)) == doctest::Approx(PI));
    CHECK(compass_target({-10, 64, 0}, 0, spawn) == doctest::Approx(PI / 2));
    CHECK(compass_target({10, 64, 0}, 0, spawn) == doctest::Approx(-PI / 2));
    // Turning to face the spawn (yaw 90 looks along +x) brings the needle to straight ahead.
    CHECK(compass_target({-10, 64, 0}, 90, spawn) == doctest::Approx(0));
    // The needle on the icon: up when ahead, to the left when the spawn is to the left.
    CHECK(needle_x(compass_icon(0)) >= 7);
    CHECK(needle_x(compass_icon(0)) <= 8);
    CHECK(needle_x(compass_icon(PI / 2)) < 6);
    CHECK(needle_x(compass_icon(-PI / 2)) > 9);
}

TEST_CASE("needles spring towards their target and settle") {
    Needle n;
    bool overshot = false;
    for (int i = 0; i < 200; ++i) {
        n.tick(1.0);
        if (n.angle > 1.0) overshot = true;
    }
    CHECK(overshot);  // Beta's wobble
    CHECK(n.angle == doctest::Approx(1.0).epsilon(0.01));
    // It goes the short way round, across +-pi.
    Needle w;
    w.angle = 3.0;
    w.tick(-3.0);
    CHECK(w.angle > 3.0);
}

TEST_CASE("the clock's dial turns once a day: sun at the top at noon, moon at midnight") {
    CHECK(clock_target(6000) == doctest::Approx(0).epsilon(0.001));         // noon
    CHECK(clock_target(18000) == doctest::Approx(PI).epsilon(0.001));       // midnight
    const Image noon = clock_icon(clock_target(6000)), midnight = clock_icon(clock_target(18000));
    // Just under the pointer: yellow sun at noon, the dark night sky (or moon) at midnight.
    CHECK(noon.at(8, 4).r == 250);
    CHECK(midnight.at(8, 4).b < 240);
    CHECK(midnight.at(8, 4).r != 250);
}

TEST_CASE("map colours follow Beta's") {
    CHECK(map_color(GRASS) == MAP_GRASS);
    CHECK(map_color(WATER) == MAP_WATER);
    CHECK(map_color(SAND) == MAP_SAND);
    CHECK(map_color(LEAVES) == MAP_FOLIAGE);
    CHECK(map_color(TALL_GRASS) == MAP_FOLIAGE);
    CHECK(map_color(SNOW_LAYER) == MAP_SNOW);
    CHECK(map_color(PLANKS) == MAP_WOOD);
    CHECK(map_color(STONE) == MAP_STONE);
    CHECK(map_color(AIR) == MAP_NONE);
    CHECK(map_color(TORCH) == MAP_NONE);
    CHECK(map_color(RAIL) == MAP_NONE);
    CHECK(map_rgba(0).a == 0);
    CHECK(map_rgba(MAP_GRASS * MAP_SHADES + 2).g == 178);  // brightest shade is the colour itself
    CHECK(map_rgba(MAP_GRASS * MAP_SHADES + 0).g < 178);
}

TEST_CASE("a held map fills in around the player, a sixteenth of its columns a tick") {
    Pond p;
    MapData m = new_map(1, 0, 0);
    CHECK(m.to_map(4, 4).x == doctest::Approx(64.5));  // a pixel is 8 blocks: blocks 0..7 are pixel 64
    for (long t = 0; t < 16; ++t) update_map(m, p.world, 4, 4, t);
    CHECK(m.dirty);
    CHECK(m.version > 0);
    // The pond is the pixel at the middle; the grass is around it.
    const glm::dvec2 c = m.to_map(4, 4);
    CHECK(m.at(static_cast<int>(c.x), static_cast<int>(c.y)) / MAP_SHADES == MAP_WATER);
    CHECK(m.at(static_cast<int>(c.x) + 2, static_cast<int>(c.y)) / MAP_SHADES == MAP_GRASS);
    // Out past the loaded chunks (5 x 5 of them, 80 blocks) the map stays blank.
    CHECK(m.at(static_cast<int>(c.x) + 12, static_cast<int>(c.y)) == 0);
    // Nothing changes the second time round.
    bool changed = false;
    for (long t = 0; t < 16; ++t) changed |= update_map(m, p.world, 4, 4, t);
    CHECK_FALSE(changed);
}

TEST_CASE("maps save and load, and the next map number is in level.json") {
    MapData m = new_map(7, -300, 1234);
    m.colors[5] = MAP_SNOW * MAP_SHADES + 1;
    auto back = decode_map(encode_map(m));
    REQUIRE(back);
    CHECK(back->id == 7);
    CHECK(back->center_x == -300);
    CHECK(back->center_z == 1234);
    CHECK(back->scale == MAP_SCALE);
    CHECK(back->colors == m.colors);
    CHECK_FALSE(decode_map("not a map"));
    CHECK(map_path("w", 3).filename() == "map_3.dat");

    LevelData level;
    level.seed = 5;
    level.next_map = 4;
    PlayerData player;
    player.inventory.slots[0] = Stack{MAP, 1, 3};  // a map item's damage is its number
    level.player = player;
    auto loaded = level_from_json(level_to_json(level));
    REQUIRE(loaded);
    CHECK(loaded->next_map == 4);
    REQUIRE(loaded->player);
    CHECK(loaded->player->inventory.slots[0] == Stack{MAP, 1, 3});
    CHECK(max_stack(MAP) == 1);
}

TEST_CASE("a cast bobber flies, lands in water and floats at the surface") {
    Pond p;
    Rng rng(1);
    // Cast from the edge of the pool towards its middle.
    Bobber b(p.world, {4.5, 11.5, -2.5}, glm::normalize(glm::dvec3(0, 0.3, 1)) * 0.6);
    for (int i = 0; i < 200 && !b.in_ground; ++i) b.tick(rng, false, {});
    CHECK_FALSE(b.in_ground);
    CHECK(b.water_fraction() > 0);
    CHECK(b.water_fraction() < 1);
    CHECK(b.body.y > 9.4);  // bobbing near the top of the water (y 10)
    CHECK(b.body.y < 10.1);
    CHECK(b.body.z > 0);
}

TEST_CASE("fish bite now and then, and only in water") {
    Pond p;
    Rng rng(2);
    Bobber b(p.world, {4.5, 9.9, 4.5}, {0, 0, 0});
    int bites = 0;
    for (int i = 0; i < 20000; ++i)
        if (b.tick(rng, false, {}).bite) ++bites;
    // About one in 500 ticks, less the ticks a fish is already on.
    CHECK(bites > 20);
    CHECK(bites < 60);
    // Rain makes it more often.
    Bobber r(p.world, {4.5, 9.9, 4.5}, {0, 0, 0});
    int rain_bites = 0;
    for (int i = 0; i < 20000; ++i)
        if (r.tick(rng, true, {}).bite) ++rain_bites;
    CHECK(rain_bites > bites);
    // A bite: reeling in now catches a fish, at the cost of one use of the rod.
    while (b.bite_ticks == 0) b.tick(rng, false, {});
    CHECK(reel_result(b) == Reel::Fish);
    CHECK(reel_wear(Reel::Fish) == 1);
}

TEST_CASE("a bobber cast onto land lies there, and one cast at a mob hooks it") {
    Pond p;
    Rng rng(3);
    Bobber b(p.world, {-20.5, 11, -20.5}, {0.3, 0, 0});
    for (int i = 0; i < 100; ++i) b.tick(rng, false, {});
    CHECK(b.in_ground);
    CHECK(b.bite_ticks == 0);
    CHECK(reel_result(b) == Reel::Ground);
    CHECK(reel_wear(Reel::Ground) == 2);

    int cow = 0;
    ArrowTarget t{{-0.45f, 10, 3}, {0.45f, 11.3f, 4}, 0, 0, &cow};
    Bobber h(p.world, {0, 10.5, 0}, {0, 0, 1.5});
    bool hit = false;
    for (int i = 0; i < 10 && !hit; ++i) hit = h.tick(rng, false, {t}).hit.has_value();
    CHECK(hit);
    CHECK(h.hooked == &cow);
    CHECK(reel_result(h) == Reel::Mob);
    CHECK(reel_wear(Reel::Mob) == 3);
    CHECK(reel_wear(Reel::Nothing) == 0);
}

TEST_CASE("the fishing line runs from the rod to the bobber, sagging in the middle") {
    auto line = fishing_line({0, 10, 0}, {0, 10, 8}, 8);
    REQUIRE(line.size() == 8);
    for (const ModelPart& part : line) CHECK(part.model == MODEL_LINE);
    // The middle piece starts lower than the ends.
    CHECK(line[4].transform[3][1] < 10.0f);
    CHECK(line[0].transform[3][1] == doctest::Approx(10.0f));
}
