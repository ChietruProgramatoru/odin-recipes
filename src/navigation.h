// Finding your way (M15): the compass, the clock and maps.
//
// The compass and clock icons move: their atlas tiles are drawn again every tick from a needle
// that springs towards where it should point (Beta's TextureCompassFX and TextureWatchFX). A map
// is a 128 x 128 picture of the land around where it was started, filled in while it is held.
// Each map has a number (the map item's damage value, like Beta), and is saved in the world's
// data/ folder as map_<number>.dat.
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "blocks.h"
#include "image.h"

class World;

// --- compass and clock ---------------------------------------------------------------------

// A needle that springs towards a target angle, overshooting a little and settling (Beta).
// Angles in radians.
struct Needle {
    double angle = 0, speed = 0;
    void tick(double target);  // one game tick
};

// Where the compass needle points, in radians from straight ahead (positive turns left on the
// icon, the way the spawn lies when the player would have to turn left to face it).
// `yaw` is in degrees, as Game uses it.
double compass_target(glm::dvec3 player, double yaw, glm::dvec3 spawn);
// How far the clock's dial is turned, in radians: 0 at noon (the sun at the top), pi at midnight.
double clock_target(double time);

// The 16 x 16 icons for a needle angle and a dial angle.
Image compass_icon(double needle);
Image clock_icon(double dial);

// --- maps ----------------------------------------------------------------------------------

constexpr int MAP_SIZE = 128;   // pixels across
constexpr int MAP_SCALE = 3;    // Beta's: a pixel is 2^3 = 8 blocks across, so a map covers 1024 blocks
constexpr int MAP_SHADES = 4;   // colour byte = base colour * 4 + shade

// Beta's map colours (MapColor). NONE is a pixel not seen yet (or with nothing in it).
enum MapColor : uint8_t {
    MAP_NONE, MAP_GRASS, MAP_SAND, MAP_CLOTH, MAP_FIRE, MAP_ICE, MAP_IRON, MAP_FOLIAGE, MAP_SNOW, MAP_CLAY,
    MAP_DIRT, MAP_STONE, MAP_WATER, MAP_WOOD,
    MAP_COLOR_COUNT
};
// The colour a block shows on a map, MAP_NONE for blocks seen through (air, torches, rails, glass).
MapColor map_color(BlockId block);
// What a colour byte looks like (fully see-through for MAP_NONE).
Rgba map_rgba(uint8_t color);

struct MapData {
    int id = 0;
    int center_x = 0, center_z = 0;  // the block at the middle of the map
    int scale = MAP_SCALE;
    std::vector<uint8_t> colors = std::vector<uint8_t>(MAP_SIZE * MAP_SIZE);  // index z * MAP_SIZE + x
    bool dirty = false;  // changed since it was last saved
    long version = 0;    // goes up with every change (to know when to draw it again)

    int blocks_per_pixel() const { return 1 << scale; }
    uint8_t at(int x, int z) const { return colors[static_cast<size_t>(z) * MAP_SIZE + x]; }
    // Where a world position falls on the map, in pixels from its top-left corner (north-west).
    glm::dvec2 to_map(double x, double z) const;
};

// A new map centred on a spot (Beta centres it on the player).
MapData new_map(int id, double x, double z);
// Draw in what a player at (x, z) can see, Beta's way: a circle 128 blocks across around them,
// one sixteenth of the map's columns each tick (those with x % 16 == tick % 16). Only loaded
// chunks are drawn. Returns true if any pixel changed.
bool update_map(MapData& map, World& world, double x, double z, long tick);
// The whole map drawn on a 128 x 128 image (unseen pixels see-through).
Image map_image(const MapData& map);
// The player's marker drawn on a map: a white arrow pointing up (16 x 16).
Image map_marker_icon();

// Saving: a map as bytes and back (nullopt if damaged), and where it lives in a world's folder.
std::string encode_map(const MapData& map);
std::optional<MapData> decode_map(const std::string& bytes);
std::filesystem::path map_path(const std::filesystem::path& world_dir, int id);
