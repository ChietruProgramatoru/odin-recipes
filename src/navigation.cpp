#include "navigation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "sky.h"
#include "world.h"

namespace {

constexpr double PI = 3.14159265358979323846;

int ifloor(double v) { return static_cast<int>(std::floor(v)); }
int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

double wrap_angle(double a) {
    a = std::fmod(a + PI, 2 * PI);
    if (a < 0) a += 2 * PI;
    return a - PI;
}

// The round face shared by the compass and the clock: a dark rim, a metal ring and the face.
// Returns the distance of each pixel's centre from the middle, and fills in the rim and ring.
Image dial_frame(Rgba ring, Rgba ring_dark) {
    Image img(16, 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const double d = std::hypot(x + 0.5 - 8, y + 0.5 - 8);
            if (d > 7.6) continue;
            if (d > 6.7) img.at(x, y) = {50, 50, 50, 255};
            else if (d > 5.7) img.at(x, y) = (x + y) < 15 ? ring : ring_dark;  // lit from the top left
        }
    return img;
}
bool on_face(int x, int y) { return std::hypot(x + 0.5 - 8, y + 0.5 - 8) <= 5.7; }

}  // namespace

// --- compass and clock ---------------------------------------------------------------------

void Needle::tick(double target) {
    // Beta: speed += clamp(difference, -1, 1) * 0.1, then speed *= 0.8.
    const double d = std::clamp(wrap_angle(target - angle), -1.0, 1.0);
    speed = (speed + d * 0.1) * 0.8;
    angle = wrap_angle(angle + speed);
}

double compass_target(glm::dvec3 player, double yaw, glm::dvec3 spawn) {
    const double dx = spawn.x - player.x, dz = spawn.z - player.z;
    if (dx * dx + dz * dz < 1e-6) return 0;
    // Game's yaw: 0 looks along +z and 90 along +x, so a bigger yaw is turned to the left.
    return wrap_angle(std::atan2(dx, dz) - yaw * PI / 180.0);
}

double clock_target(double time) { return celestial_angle(time) * 2 * PI; }

Image compass_icon(double needle) {
    Image img = dial_frame({170, 170, 175, 255}, {120, 120, 125, 255});
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (on_face(x, y)) img.at(x, y) = {225, 225, 228, 255};
    // Four little marks at the edge of the face.
    for (auto [x, y] : std::array<std::pair<int, int>, 4>{{{7, 2}, {13, 7}, {8, 13}, {2, 8}}}) img.at(x, y) = {150, 150, 155, 255};
    // The needle: red towards the target, dark grey the other way. Up on the icon is straight ahead.
    const double dx = -std::sin(needle), dy = -std::cos(needle);
    for (double t = -3.75; t <= 4.75; t += 0.25) {
        const int x = ifloor(8 + dx * t), y = ifloor(8 + dy * t);
        if (x < 0 || x > 15 || y < 0 || y > 15 || !on_face(x, y)) continue;
        img.at(x, y) = t > 0 ? Rgba{210, 30, 30, 255} : Rgba{80, 80, 85, 255};
    }
    img.at(7, 7) = img.at(8, 8) = {40, 40, 40, 255};  // the pin
    return img;
}

Image clock_icon(double dial) {
    Image img = dial_frame({240, 205, 60, 255}, {190, 150, 30, 255});
    // The dial turns behind the face: day sky with the sun on one half, night with the moon on the other.
    const double sun_x = 8 + std::sin(dial) * 3.2, sun_y = 8 - std::cos(dial) * 3.2;
    const double moon_x = 16 - sun_x, moon_y = 16 - sun_y;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            if (!on_face(x, y)) continue;
            const double px = x + 0.5, py = y + 0.5;
            const double phi = std::atan2(px - 8, 8 - py);  // 0 up, growing clockwise
            const bool day = std::cos(phi - dial) > 0;
            Rgba c = day ? Rgba{110, 170, 240, 255} : Rgba{25, 25, 70, 255};
            if (!day && ((x * 7 + y * 13) % 11 == 0)) c = {200, 200, 230, 255};  // stars
            if (std::hypot(px - sun_x, py - sun_y) < 1.7) c = {250, 220, 60, 255};
            if (std::hypot(px - moon_x, py - moon_y) < 1.5) c = {225, 225, 210, 255};
            img.at(x, y) = c;
        }
    // The pointer at the top that reads the dial.
    img.at(7, 2) = img.at(8, 2) = {50, 35, 10, 255};
    return img;
}

// --- maps ----------------------------------------------------------------------------------

MapColor map_color(BlockId b) {
    if (b == AIR || is_torch(b) || is_rail(b) || b == FIRE || b == GLASS) return MAP_NONE;
    switch (b) {
        case GRASS: return MAP_GRASS;
        case SAND: case SANDSTONE: case GRAVEL: return MAP_SAND;
        case WOOL: return MAP_CLOTH;
        case TNT: case LAVA: return MAP_FIRE;
        case ICE: return MAP_ICE;
        case IRON_BLOCK: case GOLD_BLOCK: case DIAMOND_BLOCK: case LAPIS_BLOCK: case IRON_DOOR_BLOCK: return MAP_IRON;
        case SNOW_BLOCK: case SNOW_LAYER: return MAP_SNOW;
        case CLAY_BLOCK: return MAP_CLAY;
        case DIRT: case FARMLAND: case SOUL_SAND: return MAP_DIRT;
        case WATER: return MAP_WATER;
        case LOG: case BIRCH_LOG: case SPRUCE_LOG: case PLANKS: case CRAFTING_TABLE: case CHEST: case BOOKSHELF:
        case NOTE_BLOCK: case JUKEBOX: case WOOD_STAIRS: case FENCE: case LADDER: case WOOD_DOOR_BLOCK: case TRAPDOOR:
        case SIGN_POST: case WALL_SIGN: case PAINTING_BLOCK:
            return MAP_WOOD;
        default: break;
    }
    if (is_bed(b)) return MAP_CLOTH;
    if (is_leaves(b) || is_plant(b) || b == CACTUS) return MAP_FOLIAGE;
    return MAP_STONE;
}

Rgba map_rgba(uint8_t color) {
    static const std::array<Rgba, MAP_COLOR_COUNT> BASE = {{
        {0, 0, 0, 0}, {127, 178, 56, 255}, {247, 233, 163, 255}, {167, 167, 167, 255}, {255, 0, 0, 255},
        {160, 160, 255, 255}, {167, 167, 167, 255}, {0, 124, 0, 255}, {255, 255, 255, 255}, {164, 168, 184, 255},
        {183, 106, 77, 255}, {112, 112, 112, 255}, {64, 64, 255, 255}, {104, 83, 50, 255},
    }};
    static const int SHADE[MAP_SHADES] = {180, 220, 255, 135};
    const int base = color / MAP_SHADES;
    if (base == MAP_NONE || base >= MAP_COLOR_COUNT) return {0, 0, 0, 0};
    const int m = SHADE[color % MAP_SHADES];
    const Rgba c = BASE[base];
    return {static_cast<uint8_t>(c.r * m / 255), static_cast<uint8_t>(c.g * m / 255), static_cast<uint8_t>(c.b * m / 255), 255};
}

glm::dvec2 MapData::to_map(double x, double z) const {
    const int b = blocks_per_pixel();
    return {x / b - floor_div(center_x, b) + MAP_SIZE / 2, z / b - floor_div(center_z, b) + MAP_SIZE / 2};
}

MapData new_map(int id, double x, double z) {
    MapData m;
    m.id = id;
    m.center_x = ifloor(x);
    m.center_z = ifloor(z);
    m.dirty = true;
    return m;
}

bool update_map(MapData& map, World& world, double px, double pz, long tick) {
    const int b = map.blocks_per_pixel();
    const glm::dvec2 at = map.to_map(px, pz);
    const int cx = ifloor(at.x), cz = ifloor(at.y);
    const int r = MAP_SIZE / b;  // 16 pixels: the player sees 128 blocks around them
    const int x0 = floor_div(map.center_x, b) - MAP_SIZE / 2, z0 = floor_div(map.center_z, b) - MAP_SIZE / 2;
    bool changed = false;
    for (int x = cx - r + 1; x < cx + r; ++x) {
        if (((x % 16) + 16) % 16 != tick % 16) continue;
        double prev_height = -1;
        for (int z = cz - r - 1; z < cz + r; ++z) {
            // Look at every block column under the pixel: the top block that shows, how high, and
            // how deep the water is there.
            std::array<int, MAP_COLOR_COUNT> count{};
            int height_sum = 0, water_depth = 0;
            bool loaded = true;
            for (int i = 0; i < b && loaded; ++i)
                for (int k = 0; k < b; ++k) {
                    const int wx = (x0 + x) * b + i, wz = (z0 + z) * b + k;
                    Chunk* c = world.find(chunk_of(wx), chunk_of(wz));
                    if (!c) {
                        loaded = false;
                        break;
                    }
                    const int lx = wx & 15, lz = wz & 15;
                    int y = std::min(c->max_y, HEIGHT - 1);
                    MapColor col = MAP_NONE;
                    for (; y >= 0; --y)
                        if ((col = map_color(c->blocks[Chunk::index(lx, y, lz)])) != MAP_NONE) break;
                    if (col == MAP_WATER) {
                        int wy = y;
                        while (wy > 0 && c->blocks[Chunk::index(lx, wy, lz)] == WATER) {
                            ++water_depth;
                            --wy;
                        }
                    }
                    height_sum += std::max(y, 0);
                    ++count[col];
                }
            if (!loaded) {
                prev_height = -1;
                continue;
            }
            const double height = static_cast<double>(height_sum) / (b * b);
            int best = MAP_NONE;
            for (int c = 1; c < MAP_COLOR_COUNT; ++c)
                if (count[c] > count[best]) best = c;
            const int odd = (x + z) & 1;
            int shade = 1;
            if (best == MAP_WATER) {
                const double d = static_cast<double>(water_depth) / (b * b) * 0.1 + odd * 0.2;
                if (d < 0.5) shade = 2;
                if (d > 0.9) shade = 0;
            } else if (prev_height >= 0) {
                const double d = (height - prev_height) * 4.0 / (b + 4) + (odd - 0.5) * 0.4;
                if (d > 0.6) shade = 2;
                if (d < -0.6) shade = 0;
            }
            prev_height = height;
            if (z < 0 || z >= MAP_SIZE || x < 0 || x >= MAP_SIZE) continue;
            const int dx = x - cx, dz = z - cz;
            if (dx * dx + dz * dz >= r * r) continue;
            // The edge of the circle is ragged (Beta): only every other pixel there.
            if (dx * dx + dz * dz > (r - 2) * (r - 2) && !odd) continue;
            const uint8_t value = best == MAP_NONE ? 0 : static_cast<uint8_t>(best * MAP_SHADES + shade);
            uint8_t& cell = map.colors[static_cast<size_t>(z) * MAP_SIZE + x];
            if (cell != value) {
                cell = value;
                changed = true;
            }
        }
    }
    if (changed) {
        map.dirty = true;
        ++map.version;
    }
    return changed;
}

Image map_image(const MapData& map) {
    Image img(MAP_SIZE, MAP_SIZE);
    for (int z = 0; z < MAP_SIZE; ++z)
        for (int x = 0; x < MAP_SIZE; ++x) img.at(x, z) = map_rgba(map.at(x, z));
    return img;
}

Image map_marker_icon() {
    Image img(16, 16);
    // An arrowhead pointing up with a dark outline, Beta's player marker.
    for (int y = 2; y < 14; ++y) {
        const int half = (y - 2) / 2;  // widens towards the bottom
        for (int x = 8 - half - 1; x <= 7 + half + 1; ++x) {
            const bool edge = x == 8 - half - 1 || x == 7 + half + 1 || y == 13;
            if (y > 10 && x >= 6 && x <= 9 && y < 13) continue;  // the notch at the back
            img.at(x, y) = edge ? Rgba{40, 40, 40, 255} : Rgba{255, 255, 255, 255};
        }
    }
    return img;
}

// --- saving --------------------------------------------------------------------------------

namespace {
constexpr char MAP_MAGIC[6] = {'P', 'C', 'M', 'A', 'P', 1};

void put_i32(std::string& s, int32_t v) {
    for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((static_cast<uint32_t>(v) >> (8 * i)) & 0xff));
}
int32_t get_i32(const std::string& s, size_t at) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(static_cast<uint8_t>(s[at + i])) << (8 * i);
    return static_cast<int32_t>(v);
}
}  // namespace

std::string encode_map(const MapData& map) {
    std::string s(MAP_MAGIC, sizeof MAP_MAGIC);
    put_i32(s, map.id);
    put_i32(s, map.center_x);
    put_i32(s, map.center_z);
    put_i32(s, map.scale);
    s.append(reinterpret_cast<const char*>(map.colors.data()), map.colors.size());
    return s;
}

std::optional<MapData> decode_map(const std::string& bytes) {
    const size_t header = sizeof MAP_MAGIC + 16;
    if (bytes.size() != header + MAP_SIZE * MAP_SIZE || std::memcmp(bytes.data(), MAP_MAGIC, sizeof MAP_MAGIC) != 0)
        return std::nullopt;
    MapData m;
    m.id = get_i32(bytes, 6);
    m.center_x = get_i32(bytes, 10);
    m.center_z = get_i32(bytes, 14);
    m.scale = get_i32(bytes, 18);
    if (m.scale < 0 || m.scale > 4) return std::nullopt;
    std::memcpy(m.colors.data(), bytes.data() + header, m.colors.size());
    return m;
}

std::filesystem::path map_path(const std::filesystem::path& world_dir, int id) {
    return world_dir / "data" / ("map_" + std::to_string(id) + ".dat");
}
