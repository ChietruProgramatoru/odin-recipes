// Rain, snow and thunderstorms, the Beta way: two random timers switch rain and thunder on and
// off, and the world fades in and out of each over 5 seconds. Lightning strikes only while it
// rains and thunders at once. Deserts stay dry and cold biomes get snow instead of rain.
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "biome.h"
#include "rng.h"

// What level.json saves (Beta's level.dat has the same four values).
struct WeatherState {
    bool raining = false, thundering = false;
    int rain_time = 0, thunder_time = 0;  // ticks until each switches; 0 picks a new time on the next tick
    bool operator==(const WeatherState&) const = default;
};

struct Weather {
    WeatherState state;
    // How far each has faded in (0..1), now and one tick ago (for smooth drawing between ticks).
    float rain = 0, thunder = 0, prev_rain = 0, prev_thunder = 0;

    Weather() = default;
    // A loaded world starts at full strength if it was raining, like Beta.
    explicit Weather(const WeatherState& s);

    // One game tick (20 a second).
    void tick(Rng& rng);
    // Clear skies now and new timers, like waking up after sleeping.
    void clear();

    float rain_strength(float partial = 1) const { return prev_rain + (rain - prev_rain) * partial; }
    // Thunder only counts while it also rains.
    float thunder_strength(float partial = 1) const {
        return (prev_thunder + (thunder - prev_thunder) * partial) * rain_strength(partial);
    }
    bool is_raining() const { return rain_strength() > 0.2f; }
    bool is_thundering() const { return thunder_strength() > 0.9f; }
};

enum class Precipitation { NONE, RAIN, SNOW };
// What falls from the sky in a biome when it rains.
Precipitation precipitation(Biome b);

// Lightning: each tick, each chunk near the player has this chance of a strike while it thunders.
constexpr int LIGHTNING_CHANCE = 100000;  // 1 in this many
constexpr int LIGHTNING_DAMAGE = 5;       // half hearts, to anything within LIGHTNING_REACH blocks
constexpr double LIGHTNING_REACH = 3.0;
constexpr int LIGHTNING_FLASH_TICKS = 2;  // how long the sky flashes white

// Sleeping: Beta lets you sleep once the sky has dimmed by 4 light levels, which is at night or
// in a thunderstorm (dark clouds), but not in plain rain.
bool can_sleep(float sky_darkness);
constexpr int SLEEP_TICKS = 100;  // lying in bed this long skips to morning
// The next sunrise after a time (in ticks).
int64_t next_morning(int64_t time);

class World;

// One column of falling rain or snow around the player, from `bottom` up to `top` (block y).
struct PrecipColumn {
    int x, z;
    float bottom, top;
    bool snow;
    float alpha;   // fades out with distance and with the rain strength
    uint8_t light; // packed sky/block light where the rain lands (like Chunk::light)
};
// Beta draws rain in the columns within 5 blocks (Fast graphics) or 10 (Fancy) of the player,
// from the ground (or `radius` below the eyes) up to `radius` above the eyes.
std::vector<PrecipColumn> precipitation_columns(World& world, glm::dvec3 eye, int radius, float rain);

// A lightning bolt: it shows for a tick or two, then flickers back up to a few times (Beta's
// EntityLightningBolt).
struct LightningBolt {
    glm::dvec3 pos;
    uint64_t seed;  // shape of the zigzag
    int state = 2;  // shows while >= 0
    int flashes;    // flickers left
    LightningBolt(glm::dvec3 p, Rng& rng);
    // One game tick. Returns false once the bolt is gone; sets `reflash` when it flickers again.
    bool tick(Rng& rng, bool& reflash);
    bool visible() const { return state >= 0; }
};
// The zigzag of a bolt, from the strike point upwards: points relative to the bolt's position.
std::vector<glm::vec3> lightning_path(uint64_t seed);

// Where lightning would strike in chunk (cx, cz) this tick, if it does: Beta rolls 1 in
// LIGHTNING_CHANCE per chunk, then picks a column that is open to the sky and rained on.
std::optional<glm::dvec3> roll_lightning(World& world, int cx, int cz, Rng& rng);
