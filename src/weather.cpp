#include "weather.h"

#include <algorithm>
#include <cmath>

#include "sky.h"
#include "world.h"

Weather::Weather(const WeatherState& s) : state(s) {
    rain = prev_rain = s.raining ? 1.0f : 0.0f;
    thunder = prev_thunder = s.thundering ? 1.0f : 0.0f;
}

void Weather::tick(Rng& rng) {
    // Beta's timers: rain lasts 10-20 minutes and comes back after 10-150 minutes; thunder is
    // rolled on its own (3-13 minutes on, 10-150 off) but only shows while it also rains.
    if (state.thunder_time <= 0) {
        state.thunder_time = state.thundering ? rng.below(12000) + 3600 : rng.below(168000) + 12000;
    } else if (--state.thunder_time <= 0) {
        state.thundering = !state.thundering;
    }
    if (state.rain_time <= 0) {
        state.rain_time = state.raining ? rng.below(12000) + 12000 : rng.below(168000) + 12000;
    } else if (--state.rain_time <= 0) {
        state.raining = !state.raining;
    }
    prev_rain = rain;
    prev_thunder = thunder;
    rain = std::clamp(rain + (state.raining ? 0.01f : -0.01f), 0.0f, 1.0f);
    thunder = std::clamp(thunder + (state.thundering ? 0.01f : -0.01f), 0.0f, 1.0f);
}

void Weather::clear() {
    state = WeatherState{};
    rain = prev_rain = thunder = prev_thunder = 0;
}

Precipitation precipitation(Biome b) {
    if (b == Biome::DESERT) return Precipitation::NONE;
    return is_cold(b) ? Precipitation::SNOW : Precipitation::RAIN;
}

bool can_sleep(float sky_darkness) { return sky_darkness >= 4; }

int64_t next_morning(int64_t time) {
    int64_t t = time + DAY_TICKS;
    return t - t % DAY_TICKS;
}

std::vector<PrecipColumn> precipitation_columns(World& world, glm::dvec3 eye, int radius, float rain) {
    std::vector<PrecipColumn> out;
    if (rain <= 0) return out;
    const int ex = static_cast<int>(std::floor(eye.x)), ey = static_cast<int>(std::floor(eye.y));
    const int ez = static_cast<int>(std::floor(eye.z));
    for (int z = ez - radius; z <= ez + radius; ++z)
        for (int x = ex - radius; x <= ex + radius; ++x) {
            auto biome = world.biome_at(x, z);
            auto ground = world.rain_height(x, z);
            if (!biome || !ground) continue;
            Precipitation p = precipitation(*biome);
            if (p == Precipitation::NONE) continue;
            float bottom = static_cast<float>(std::max(*ground, ey - radius));
            float top = static_cast<float>(std::max(*ground, ey + radius));
            if (top <= bottom) continue;
            double dx = x + 0.5 - eye.x, dz = z + 0.5 - eye.z;
            double d = (dx * dx + dz * dz) / (radius * radius);
            float alpha = static_cast<float>((1 - d) * 0.5 + 0.5) * rain;
            if (alpha <= 0) continue;
            int ly = std::clamp(*ground, 0, HEIGHT - 1);
            uint8_t light = static_cast<uint8_t>((world.sky_light(x, ly, z) << 4) | world.block_light(x, ly, z));
            out.push_back({x, z, bottom, top, p == Precipitation::SNOW, alpha, light});
        }
    return out;
}

LightningBolt::LightningBolt(glm::dvec3 p, Rng& rng) : pos(p), seed(rng.next()), flashes(rng.below(3) + 1) {}

bool LightningBolt::tick(Rng& rng, bool& reflash) {
    reflash = false;
    if (--state >= 0) return true;
    if (flashes == 0) return false;
    if (state < -rng.below(10)) {
        --flashes;
        state = 1;
        seed = rng.next();
        reflash = true;
    }
    return true;
}

std::vector<glm::vec3> lightning_path(uint64_t seed) {
    // Beta's bolt: 8 steps of 16 blocks up, each wandering up to 7 blocks sideways.
    Rng rng(seed);
    std::vector<glm::vec3> pts;
    glm::vec3 p(0);
    pts.push_back(p);
    for (int i = 0; i < 8; ++i) {
        p += glm::vec3(rng.randint(-7, 7) * 0.5f, 16, rng.randint(-7, 7) * 0.5f);
        pts.push_back(p);
    }
    return pts;
}

std::optional<glm::dvec3> roll_lightning(World& world, int cx, int cz, Rng& rng) {
    if (rng.below(LIGHTNING_CHANCE) != 0) return std::nullopt;
    int x = cx * CHUNK + rng.below(CHUNK), z = cz * CHUNK + rng.below(CHUNK);
    auto biome = world.biome_at(x, z);
    auto ground = world.rain_height(x, z);
    if (!biome || !ground || precipitation(*biome) != Precipitation::RAIN) return std::nullopt;
    return glm::dvec3(x + 0.5, *ground, z + 0.5);
}
