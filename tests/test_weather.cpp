// Weather (rain, snow, thunder, lightning) and beds.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "bed.h"
#include "physics.h"
#include "rng.h"
#include "save.h"
#include "sky.h"
#include "textures.h"
#include "weather.h"
#include "world.h"

namespace {

// A stone floor at y = 0..9 under open sky, with one climate everywhere.
std::unique_ptr<Chunk> flat_chunk(int cx, int cz, double temperature, double rainfall) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    c->climate.temperature.fill(static_cast<uint8_t>(temperature * 255));
    c->climate.rainfall.fill(static_cast<uint8_t>(rainfall * 255));
    return c;
}

void load_flat(World& w, int radius, double temperature = 0.7, double rainfall = 0.6) {
    for (int cx = -radius; cx <= radius; ++cx)
        for (int cz = -radius; cz <= radius; ++cz) w.insert(flat_chunk(cx, cz, temperature, rainfall));
}

}  // namespace

TEST_CASE("rain and thunder switch on Beta's timers and fade in over 100 ticks") {
    Rng rng(5);
    Weather w;
    w.tick(rng);  // the first tick picks the timers
    CHECK_FALSE(w.state.raining);
    CHECK(w.state.rain_time >= 12000);
    CHECK(w.state.rain_time < 180000);
    CHECK(w.state.thunder_time >= 12000);

    w.state.rain_time = 1;
    w.tick(rng);
    CHECK(w.state.raining);
    w.tick(rng);  // new timer for how long it rains: 10 to 20 minutes
    CHECK(w.state.rain_time >= 12000);
    CHECK(w.state.rain_time < 24000);
    CHECK_FALSE(w.is_raining());
    for (int i = 0; i < 100; ++i) w.tick(rng);
    CHECK(w.rain_strength() == doctest::Approx(1.0f));
    CHECK(w.is_raining());

    // Thunder only counts while it rains.
    w.state.thundering = true;
    w.state.thunder_time = 5000;
    for (int i = 0; i < 100; ++i) w.tick(rng);
    CHECK(w.is_thundering());
    w.state.raining = false;
    w.state.rain_time = 5000;
    for (int i = 0; i < 100; ++i) w.tick(rng);
    CHECK(w.thunder_strength() == doctest::Approx(0.0f));
    CHECK_FALSE(w.is_thundering());

    w.clear();
    CHECK(w.state == WeatherState{});
    CHECK(w.rain_strength() == 0);
}

TEST_CASE("a saved storm is at full strength when the world loads") {
    Weather w(WeatherState{true, true, 3000, 2000});
    CHECK(w.is_raining());
    CHECK(w.is_thundering());
}

TEST_CASE("deserts stay dry and cold biomes get snow") {
    CHECK(precipitation(Biome::DESERT) == Precipitation::NONE);
    CHECK(precipitation(Biome::TUNDRA) == Precipitation::SNOW);
    CHECK(precipitation(Biome::TAIGA) == Precipitation::SNOW);
    CHECK(precipitation(Biome::ICE_DESERT) == Precipitation::SNOW);
    CHECK(precipitation(Biome::FOREST) == Precipitation::RAIN);
    CHECK(precipitation(Biome::PLAINS) == Precipitation::RAIN);
}

TEST_CASE("rain darkens and greys the sky, and hides the sun") {
    const double noon = 6000;
    SkyState clear = sky_state(noon), rain = sky_state(noon, 1, 0), storm = sky_state(noon, 1, 1);
    CHECK(clear.darkness == doctest::Approx(0));
    // Beta: rain takes 5/16 of the daylight, a storm 5/16 of what's left.
    CHECK(rain.darkness == doctest::Approx(11 * 5 / 16.0));
    CHECK(storm.darkness == doctest::Approx(11 * (1 - 11 / 16.0 * 11 / 16.0)));
    CHECK(rain.sun_alpha == 0);
    CHECK(clear.sun_alpha == 1);
    CHECK(rain.sky.b < clear.sky.b);
    CHECK(storm.sky.b < rain.sky.b);
    CHECK(rain.fog.r < clear.fog.r);
    // Grey: the colour channels move closer together.
    CHECK(rain.sky.b - rain.sky.r < clear.sky.b - clear.sky.r);
    // A lightning flash lights the sky up.
    CHECK(sky_state(noon, 1, 1, 1).sky.r > storm.sky.r);
}

TEST_CASE("you can sleep at night and in thunderstorms, not by day or in plain rain") {
    const double noon = 6000, midnight = 18000;
    CHECK_FALSE(can_sleep(sky_state(noon).darkness));
    CHECK(can_sleep(sky_state(midnight).darkness));
    CHECK_FALSE(can_sleep(sky_state(noon, 1, 0).darkness));
    CHECK(can_sleep(sky_state(noon, 1, 1).darkness));
    CHECK(next_morning(18000) == 24000);
    CHECK(next_morning(24000 * 3 + 13000) == 24000 * 4);
    CHECK(next_morning(0) == 24000);
}

TEST_CASE("rain falls on the ground near the player, snow in the cold, nothing in the desert") {
    World w(1);
    load_flat(w, 1);
    w.set_block(3, 10, 3, PLANKS);  // a roof post: rain stops on top of it
    auto cols = precipitation_columns(w, {0.5, 12, 0.5}, 5, 1.0f);
    CHECK(cols.size() == 11 * 11 - 4);  // the four far corners fade out completely
    for (const PrecipColumn& c : cols) {
        CHECK_FALSE(c.snow);
        CHECK(c.bottom == (c.x == 3 && c.z == 3 ? 11 : 10));
        CHECK(c.top == 17);
        CHECK(c.alpha > 0);
    }
    CHECK(precipitation_columns(w, {0.5, 12, 0.5}, 5, 0).empty());

    World cold(1);
    load_flat(cold, 1, 0.05, 0.5);
    auto snow = precipitation_columns(cold, {0.5, 12, 0.5}, 5, 1.0f);
    REQUIRE_FALSE(snow.empty());
    CHECK(snow[0].snow);

    World desert(1);
    load_flat(desert, 1, 1.0, 0.0);
    CHECK(precipitation_columns(desert, {0.5, 12, 0.5}, 5, 1.0f).empty());
    Rng rng(3);
    for (int i = 0; i < 300000; ++i) REQUIRE_FALSE(roll_lightning(desert, 0, 0, rng));
}

TEST_CASE("lightning strikes the top of a rained-on column, about 1 in 100000 ticks per chunk") {
    World w(1);
    load_flat(w, 1);
    Rng rng(11);
    int strikes = 0;
    for (int i = 0; i < 1000000; ++i)
        if (auto at = roll_lightning(w, 0, 0, rng)) {
            ++strikes;
            CHECK(at->y == 10);
            CHECK(at->x >= 0);
            CHECK(at->x < 16);
        }
    CHECK(strikes >= 3);
    CHECK(strikes <= 25);

    // A bolt shows, flickers a few times and goes.
    LightningBolt bolt({0.5, 10, 0.5}, rng);
    CHECK(bolt.visible());
    int ticks = 0, flickers = 0;
    bool reflash = false;
    while (bolt.tick(rng, reflash) && ticks < 1000) {
        ++ticks;
        flickers += reflash;
    }
    CHECK(ticks < 100);
    CHECK(flickers >= 1);
    CHECK(flickers <= 3);
    auto path = lightning_path(42);
    CHECK(path.size() == 9);
    CHECK(path.back().y == 128);
}

TEST_CASE("a bed goes on top of a block, pointing the way the player faces") {
    World w(1);
    load_flat(w, 1);
    CHECK(facing_from_yaw(0) == 0);    // looking +z: south
    CHECK(facing_from_yaw(90) == 3);   // +x: east
    CHECK(facing_from_yaw(-90) == 1);  // -x: west
    CHECK(facing_from_yaw(180) == 2);  // -z: north

    auto bed = bed_placement(w, {4, 9, 4}, {0, 1, 0}, 3);
    REQUIRE(bed);
    CHECK(bed->foot == glm::ivec3(4, 10, 4));
    CHECK(bed->head == glm::ivec3(5, 10, 4));
    CHECK_FALSE(bed_placement(w, {4, 9, 4}, {1, 0, 0}, 3));  // not on the side of a block

    w.set_block(5, 10, 4, PLANKS);
    CHECK_FALSE(bed_placement(w, {4, 9, 4}, {0, 1, 0}, 3));  // the head's cell is taken
    w.set_block(5, 10, 4, AIR);
    w.set_block(5, 9, 4, AIR);
    CHECK_FALSE(bed_placement(w, {4, 9, 4}, {0, 1, 0}, 3));  // the head would hang over a hole
}

TEST_CASE("bed blocks: halves, facing, height and light") {
    for (int dir = 0; dir < 4; ++dir) {
        BlockId foot = bed_block(dir, false), head = bed_block(dir, true);
        CHECK(is_bed(foot));
        CHECK(is_bed(head));
        CHECK(bed_dir(foot) == dir);
        CHECK(bed_dir(head) == dir);
        CHECK_FALSE(bed_is_head(foot));
        CHECK(bed_is_head(head));
        glm::ivec3 p(10, 20, 30);
        CHECK(bed_other_half(p, foot) == p + FACING_DIRS[dir]);
        CHECK(bed_other_half(p + FACING_DIRS[dir], head) == p);
    }
    CHECK(is_solid(BED_HEAD_NORTH));
    CHECK_FALSE(is_opaque(BED_HEAD_NORTH));
    CHECK(light_opacity(BED_FOOT_EAST) == 0);
    CHECK(block_height(BED_FOOT_EAST) == doctest::Approx(9.0 / 16));
    CHECK(block_height(STONE) == 1);
    CHECK_FALSE(is_bed(TORCH));
    CHECK_FALSE(block_info(BED_FOOT_SOUTH)->drop);  // the game drops one bed for the pair
}

TEST_CASE("you stand on a bed 9/16 of a block up") {
    World w(1);
    load_flat(w, 1);
    w.set_block(4, 10, 4, bed_block(0, false));
    w.set_block(4, 10, 5, bed_block(0, true));
    Body b(w, 4.5, 12, 4.5, 0.3, 1.8);
    for (int i = 0; i < 60; ++i) {
        b.apply_gravity(0.05);
        b.move(0, b.vy * 0.05, 0);
    }
    CHECK(b.on_ground);
    CHECK(b.y == doctest::Approx(10 + 9.0 / 16));
    // Next to it, the floor.
    Body c(w, 6.5, 12, 4.5, 0.3, 1.8);
    for (int i = 0; i < 60; ++i) {
        c.apply_gravity(0.05);
        c.move(0, c.vy * 0.05, 0);
    }
    CHECK(c.y == doctest::Approx(10));
}

TEST_CASE("waking up: a free spot next to the bed, or nothing if the bed is gone or walled in") {
    World w(1);
    load_flat(w, 1);
    glm::ivec3 foot(4, 10, 4), head(4, 10, 5);
    w.set_block(foot.x, foot.y, foot.z, bed_block(0, false));
    w.set_block(head.x, head.y, head.z, bed_block(0, true));
    auto spot = bed_stand_spot(w, head);
    REQUIRE(spot);
    CHECK(spot->y == 10);
    CHECK(std::abs(spot->x - head.x) <= 1);
    CHECK(std::abs(spot->z - head.z) <= 1);
    CHECK(w.get_block(spot->x, spot->y, spot->z) == AIR);
    CHECK_FALSE(bed_stand_spot(w, foot));  // spawn points are kept at the head

    // Wall it in: every cell around both halves gets a block.
    for (int x = 3; x <= 5; ++x)
        for (int z = 3; z <= 6; ++z)
            if (!(x == 4 && (z == 4 || z == 5))) w.set_block(x, 10, z, PLANKS);
    CHECK_FALSE(bed_stand_spot(w, head));
    w.set_block(5, 10, 3, AIR);  // a gap by the foot is enough
    CHECK(bed_stand_spot(w, head) == glm::ivec3(5, 10, 3));

    w.set_block(head.x, head.y, head.z, AIR);
    CHECK_FALSE(bed_stand_spot(w, head));
}

TEST_CASE("level.json keeps the weather and the player's bed") {
    LevelData level;
    level.seed = 7;
    level.weather = {true, false, 1234, 5678};
    PlayerData p;
    p.bed = glm::ivec3(-12, 64, 300);
    level.player = p;
    auto back = level_from_json(level_to_json(level));
    REQUIRE(back);
    CHECK(back->weather == level.weather);
    REQUIRE(back->player);
    CHECK(back->player->bed == p.bed);

    // Worlds saved before weather existed load with clear skies and no bed.
    auto old = level_from_json(R"({"seed": "7", "player": {"pos": [1, 2, 3]}})");
    REQUIRE(old);
    CHECK(old->weather == WeatherState{});
    REQUIRE(old->player);
    CHECK_FALSE(old->player->bed);
}

TEST_CASE("rain and snow pictures repeat and are see-through between drops") {
    Image rain = make_rain_image(), snow = make_snow_image();
    int clear = 0, drops = 0;
    for (const Rgba& p : rain.pixels) (p.a == 0 ? clear : drops)++;
    CHECK(drops > 50);
    CHECK(clear > drops * 4);
    int flakes = 0;
    for (const Rgba& p : snow.pixels) flakes += p.a > 0;
    CHECK(flakes > 30);
}
