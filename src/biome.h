// Biomes picked from temperature and rainfall like Beta, and the grass and leaf colours they give.
#pragma once
#include <array>
#include <cstdint>

enum class Biome : uint8_t {
    RAINFOREST, SWAMP, SEASONAL_FOREST, FOREST, SAVANNA, SHRUBLAND, TAIGA, DESERT, PLAINS, ICE_DESERT, TUNDRA,
    COUNT
};

const char* biome_name(Biome b);

// Beta's lookup: temperature and rainfall both in [0, 1].
Biome biome_for(double temperature, double rainfall);

// Water freezes (and, from M11, snow falls) in these.
bool is_cold(Biome b);

struct Rgb8 {
    uint8_t r, g, b;
    bool operator==(const Rgb8&) const = default;
};

// Colours for grey grass and oak leaf pixels at a temperature and rainfall (Beta's colour triangle:
// cold is blue-green, hot and dry is yellow-brown, hot and wet is bright green).
Rgb8 grass_color(double temperature, double rainfall);
Rgb8 foliage_color(double temperature, double rainfall);
// Birch and spruce leaves don't change with the climate.
constexpr Rgb8 BIRCH_COLOR{128, 167, 85};
constexpr Rgb8 SPRUCE_COLOR{97, 153, 97};
// Used for the hotbar icons of grass and oak leaves.
constexpr double ICON_TEMPERATURE = 0.8, ICON_RAINFALL = 0.4;

// Temperature and rainfall of every column of a chunk, 0..255 each, index (z << 4) | x.
struct Climate {
    std::array<uint8_t, 256> temperature{}, rainfall{};
    double temp(int i) const { return temperature[i] / 255.0; }
    double rain(int i) const { return rainfall[i] / 255.0; }
    Biome biome(int i) const { return biome_for(temp(i), rain(i)); }
};
