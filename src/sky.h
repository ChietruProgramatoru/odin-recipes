// Time of day: the sun and moon, sky and fog colours, how dark the world gets, and the cloud
// pattern. Beta's formulas, so a day lasts 20 minutes (24000 ticks) and time 0 is sunrise.
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

constexpr int DAY_TICKS = 24000;     // 20 minutes at 20 ticks a second
constexpr float CLOUD_HEIGHT = 108;  // bottom of the clouds
constexpr float CLOUD_THICKNESS = 4;
constexpr int CLOUD_CELL = 12;       // blocks per pixel of the cloud map
constexpr int CLOUD_MAP = 256;       // the cloud map is 256 x 256 pixels and repeats
constexpr float CLOUD_SPEED = 0.6f;  // blocks per second, towards +x

// Where the sun is: 0 at noon (overhead), 0.25 sunset, 0.5 midnight, 0.75 sunrise.
float celestial_angle(double time);  // time in ticks (fractions allowed)
// 1 in full daylight, 0 at night, in between around sunrise and sunset.
float daylight_factor(float angle);
// How many levels sky light loses: 0 by day, 11 at night (sky light 15 becomes 4).
float sky_darkness(float angle);
// Brightness of the stars (0 by day, up to 0.5 at night).
float star_brightness(float angle);
// Colour overhead, and of the fog and the horizon.
glm::vec3 sky_color(float angle);
glm::vec3 fog_color(float angle);
glm::vec3 cloud_color(float angle);
// The orange glow at the horizon around sunrise and sunset (rgb, alpha), if any.
std::optional<glm::vec4> sunset_color(float angle);
// Time of day as a 24-hour clock reading ("06:00" at time 0, like Beta's clock).
int clock_minutes(int64_t time);

// Everything the renderer needs to draw the sky for one frame.
struct SkyState {
    float angle = 0;
    float darkness = 0;  // sky_darkness
    float stars = 0;
    glm::vec3 sky{0.62f, 0.8f, 1.0f}, fog{0.75f, 0.85f, 1.0f}, clouds{1, 1, 1};
    std::optional<glm::vec4> sunset;
    double cloud_offset = 0;  // how far the clouds have drifted towards +x, in blocks (wraps)
    bool show_clouds = true;
    bool fancy_clouds = true;
    // Weather: rain and thunder strength (0..1), how much the sun, moon and stars show through
    // the rain clouds, and a lightning flash (0..1).
    float rain = 0, thunder = 0, sun_alpha = 1, flash = 0;
};
// How bright a light level (0..15) looks, Beta's curve (the same one the world shader uses).
float light_brightness(float level);

// Rain and thunder darken the sky and turn it grey; a lightning flash turns it white for a moment.
SkyState sky_state(double time, float rain = 0, float thunder = 0, float flash = 0);
// sky_darkness with the weather: rain takes 5/16 off the daylight, thunder another 5/16.
float weather_darkness(float angle, float rain, float thunder);

// Which pixels of the cloud map are cloud (index y * CLOUD_MAP + x). Same map in every world.
const std::vector<uint8_t>& cloud_map();
bool cloud_at(int cell_x, int cell_z);  // wraps around
