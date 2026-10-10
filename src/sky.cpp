#include "sky.h"

#include <algorithm>
#include <cmath>

#include "noise.h"

namespace {
constexpr double PI = 3.14159265358979323846;
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float cos_turn(float angle) { return static_cast<float>(std::cos(angle * 2 * PI)); }
}  // namespace

float celestial_angle(double time) {
    double t = std::fmod(time, DAY_TICKS) / DAY_TICKS;
    if (t < 0) t += 1;
    t -= 0.25;
    if (t < 0) t += 1;
    // Beta stretches the day a little: the sun spends longer high in the sky than near the horizon.
    double eased = 1 - (std::cos(t * PI) + 1) / 2;
    return static_cast<float>(t + (eased - t) / 3);
}

float daylight_factor(float angle) { return clamp01(cos_turn(angle) * 2 + 0.5f); }

float sky_darkness(float angle) { return (1 - daylight_factor(angle)) * 11; }

float star_brightness(float angle) {
    float f = clamp01(1 - (cos_turn(angle) * 2 + 0.75f));
    return f * f * 0.5f;
}

glm::vec3 sky_color(float angle) { return glm::vec3(0.62f, 0.8f, 1.0f) * daylight_factor(angle); }

glm::vec3 fog_color(float angle) {
    float d = daylight_factor(angle);
    return {0.753f * (d * 0.94f + 0.06f), 0.847f * (d * 0.94f + 0.06f), 1.0f * (d * 0.91f + 0.09f)};
}

glm::vec3 cloud_color(float angle) {
    float d = daylight_factor(angle);
    return {d * 0.9f + 0.1f, d * 0.9f + 0.1f, d * 0.85f + 0.15f};
}

std::optional<glm::vec4> sunset_color(float angle) {
    float c = cos_turn(angle);
    if (c < -0.4f || c > 0.4f) return std::nullopt;
    float f = c / 0.4f * 0.5f + 0.5f;
    float alpha = 1 - (1 - static_cast<float>(std::sin(f * PI))) * 0.99f;
    return glm::vec4(f * 0.3f + 0.7f, f * f * 0.7f + 0.2f, 0.2f, alpha * alpha);
}

int clock_minutes(int64_t time) {
    int64_t t = ((time % DAY_TICKS) + DAY_TICKS) % DAY_TICKS;
    return static_cast<int>((t * 1440 / DAY_TICKS + 6 * 60) % 1440);
}

float light_brightness(float level) {
    float f = 1 - std::clamp(level, 0.0f, 15.0f) / 15;
    return (1 - f) / (f * 3 + 1) * 0.95f + 0.05f;
}

float weather_darkness(float angle, float rain, float thunder) {
    return (1 - daylight_factor(angle) * (1 - rain * 5 / 16) * (1 - thunder * 5 / 16)) * 11;
}

namespace {
// Beta greys a colour towards its brightness times `grey`, by `amount`.
glm::vec3 grey_out(glm::vec3 c, float grey, float amount) {
    float g = (c.r * 0.3f + c.g * 0.59f + c.b * 0.11f) * grey;
    return c * (1 - amount) + glm::vec3(g) * amount;
}
}  // namespace

SkyState sky_state(double time, float rain, float thunder, float flash) {
    SkyState s;
    s.angle = celestial_angle(time);
    s.rain = rain;
    s.thunder = thunder;
    s.flash = flash;
    s.darkness = weather_darkness(s.angle, rain, thunder);
    s.sun_alpha = 1 - rain;
    s.stars = star_brightness(s.angle) * (1 - rain);
    s.sky = sky_color(s.angle);
    s.fog = fog_color(s.angle);
    s.clouds = cloud_color(s.angle);
    s.sunset = sunset_color(s.angle);
    // Beta's weather colours (World.getSkyColor, getCloudColor and the renderer's fog colour).
    if (rain > 0) {
        s.sky = grey_out(s.sky, 0.6f, rain * 0.75f);
        s.clouds = grey_out(s.clouds, 0.6f, rain * 0.95f);
        s.fog *= glm::vec3(1 - rain * 0.5f, 1 - rain * 0.5f, 1 - rain * 0.4f);
    }
    if (thunder > 0) {
        s.sky = grey_out(s.sky, 0.2f, thunder * 0.75f);
        s.clouds = grey_out(s.clouds, 0.2f, thunder * 0.95f);
        s.fog *= 1 - thunder * 0.5f;
    }
    if (flash > 0) {
        float f = std::min(flash, 1.0f) * 0.45f;
        s.sky = s.sky * (1 - f) + glm::vec3(0.8f, 0.8f, 1.0f) * f;
    }
    // The map repeats every CLOUD_MAP cells, so the drift can wrap without a visible jump.
    s.cloud_offset = std::fmod(time / 20 * CLOUD_SPEED, static_cast<double>(CLOUD_MAP) * CLOUD_CELL);
    return s;
}

const std::vector<uint8_t>& cloud_map() {
    static const std::vector<uint8_t> map = [] {
        // Tileable noise: sample Perlin noise on a torus so the map wraps without a seam.
        Perlin2D a(77), b(78);
        std::vector<uint8_t> m(CLOUD_MAP * CLOUD_MAP);
        for (int y = 0; y < CLOUD_MAP; ++y)
            for (int x = 0; x < CLOUD_MAP; ++x) {
                double u = x * 2 * PI / CLOUD_MAP, v = y * 2 * PI / CLOUD_MAP;
                double n = a.fbm(std::cos(u) * 6 + 20, std::sin(u) * 6 + 20 + std::cos(v) * 6, 3) +
                           0.6 * b.fbm(std::sin(v) * 6 + 40, std::cos(v) * 6 + std::sin(u) * 6, 2);
                m[y * CLOUD_MAP + x] = n > 0.12 ? 1 : 0;
            }
        return m;
    }();
    return map;
}

bool cloud_at(int cell_x, int cell_z) {
    int x = ((cell_x % CLOUD_MAP) + CLOUD_MAP) % CLOUD_MAP, z = ((cell_z % CLOUD_MAP) + CLOUD_MAP) % CLOUD_MAP;
    return cloud_map()[z * CLOUD_MAP + x] != 0;
}
