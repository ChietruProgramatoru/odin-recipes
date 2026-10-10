#include "biome.h"

#include <algorithm>

namespace {

const char* NAMES[] = {"Rainforest", "Swamp", "Seasonal Forest", "Forest", "Savanna", "Shrubland",
                       "Taiga", "Desert", "Plains", "Ice Desert", "Tundra"};

struct Color {
    double r, g, b;
};

Color mix(Color a, Color b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

Rgb8 triangle(double temperature, double rainfall, Color cold, Color hot_dry, Color hot_wet) {
    double t = std::clamp(temperature, 0.0, 1.0);
    double r = std::clamp(rainfall, 0.0, 1.0);
    Color c = mix(mix(cold, hot_dry, t), mix(cold, hot_wet, t), r);
    auto u8 = [](double v) { return static_cast<uint8_t>(std::clamp(v, 0.0, 255.0)); };
    return {u8(c.r), u8(c.g), u8(c.b)};
}

}  // namespace

const char* biome_name(Biome b) { return NAMES[static_cast<int>(b)]; }

Biome biome_for(double temperature, double rainfall) {
    double t = temperature, r = rainfall * temperature;  // Beta scales rainfall by temperature
    if (t < 0.1) return rainfall < 0.4 ? Biome::ICE_DESERT : Biome::TUNDRA;
    if (r < 0.2) {
        if (t < 0.5) return Biome::TUNDRA;
        if (t < 0.95) return Biome::SAVANNA;
        return Biome::DESERT;
    }
    if (r > 0.5 && t < 0.7) return Biome::SWAMP;
    if (t < 0.5) return Biome::TAIGA;
    if (t < 0.97) return r < 0.35 ? Biome::SHRUBLAND : Biome::FOREST;
    if (r < 0.45) return Biome::PLAINS;
    if (r < 0.9) return Biome::SEASONAL_FOREST;
    return Biome::RAINFOREST;
}

bool is_cold(Biome b) { return b == Biome::TAIGA || b == Biome::TUNDRA || b == Biome::ICE_DESERT; }

Rgb8 grass_color(double temperature, double rainfall) {
    return triangle(temperature, rainfall * temperature, {128, 180, 151}, {191, 183, 85}, {71, 205, 51});
}

Rgb8 foliage_color(double temperature, double rainfall) {
    return triangle(temperature, rainfall * temperature, {96, 161, 123}, {174, 164, 42}, {26, 191, 0});
}
