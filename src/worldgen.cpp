// Beta-style world generation.
//
// A chunk is generated inside a 48 x 48 region: the chunk plus 16 blocks on every side. The
// region gets the whole pipeline (3D noise terrain, biome surface, caves, lakes, dungeons, ores
// and trees) and only the chunk in its middle is kept. Like Beta's chunk population, features are
// anchored 8 to 24 blocks into a source chunk and reach at most 8 blocks from their anchor, so
// they only touch that chunk and the ones to its +x/+z side. The sources that can reach chunk
// (cx, cz) are the 4 chunks (cx-1..cx, cz-1..cz), and everything they read lies in the region,
// so neighbouring chunks agree on where trees, lakes and dungeons are.
#include <algorithm>
#include <cmath>
#include <vector>

#include "rng.h"
#include "world.h"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr int MARGIN = 16;
constexpr int RW = CHUNK + 2 * MARGIN;  // region width
constexpr int CAVE_RANGE = 8;           // chunks a cave system can wander from where it starts
constexpr int LAVA_LEVEL = 10;          // caves below this fill with lava

int ifloor(double v) { return static_cast<int>(std::floor(v)); }

// Salts so each kind of feature gets its own random numbers.
enum Salt : uint64_t { CAVES = 1, LAKES, DUNGEONS, ORE_VEINS, CLAY_SALT, TREES };

uint64_t chunk_seed(uint64_t seed, int sx, int sz, uint64_t salt) {
    Rng mix(seed ^ (static_cast<uint64_t>(static_cast<uint32_t>(sx)) * 0x9E3779B97F4A7C15ull) ^
            (static_cast<uint64_t>(static_cast<uint32_t>(sz)) * 0xC2B2AE3D27D4EB4Full) ^ (salt << 56));
    return mix.next();
}

// The blocks being generated, in world coordinates. Reads outside it give AIR, writes are dropped.
struct Region {
    int x0, z0;
    std::vector<BlockId> blocks = std::vector<BlockId>(RW * RW * HEIGHT, AIR);  // column-major: y fastest
    std::array<Biome, RW * RW> biomes{};

    static int col(int lx, int lz) { return lz * RW + lx; }
    bool inside(int x, int y, int z) const {
        return x >= x0 && x < x0 + RW && z >= z0 && z < z0 + RW && y >= 0 && y < HEIGHT;
    }
    BlockId get(int x, int y, int z) const {
        return inside(x, y, z) ? blocks[col(x - x0, z - z0) * HEIGHT + y] : BlockId{AIR};
    }
    void set(int x, int y, int z, BlockId b) {
        if (inside(x, y, z)) blocks[col(x - x0, z - z0) * HEIGHT + y] = b;
    }
    Biome biome(int x, int z) const {
        int lx = std::clamp(x - x0, 0, RW - 1), lz = std::clamp(z - z0, 0, RW - 1);
        return biomes[col(lx, lz)];
    }
    // Y of the highest block that isn't air, or -1.
    int top(int x, int z) const {
        for (int y = HEIGHT - 1; y >= 0; --y)
            if (get(x, y, z) != AIR) return y;
        return -1;
    }
};

// --- Caves (Beta's worm carver) -----------------------------------------------------------

class CaveCarver {
public:
    CaveCarver(Region& r, int cx, int cz) : r_(r), centre_x_(cx * CHUNK + 8.0), centre_z_(cz * CHUNK + 8.0) {}

    void from_chunk(Rng& rng, int sx, int sz) {
        int n = rng.below(rng.below(rng.below(40) + 1) + 1);
        if (rng.below(15) != 0) n = 0;
        for (int i = 0; i < n; ++i) {
            double x = sx * CHUNK + rng.below(CHUNK);
            double y = rng.below(rng.below(120) + 8);
            double z = sz * CHUNK + rng.below(CHUNK);
            int tunnels = 1;
            if (rng.below(4) == 0) {
                tunnel(rng.next(), x, y, z, 1 + rng.random() * 6, 0, 0, -1, -1, 0.5);  // a round room
                tunnels += rng.below(4);
            }
            for (int t = 0; t < tunnels; ++t) {
                double yaw = rng.random() * PI * 2;
                double pitch = (rng.random() - 0.5) * 2 / 8;
                double width = rng.random() * 2 + rng.random();
                tunnel(rng.next(), x, y, z, width, yaw, pitch, 0, 0, 1.0);
            }
        }
    }

private:
    void tunnel(uint64_t seed, double x, double y, double z, double width, double yaw, double pitch, int step,
                int length, double y_scale) {
        Rng rng(seed);
        if (length <= 0) {
            int most = CAVE_RANGE * CHUNK - CHUNK;
            length = most - rng.below(most / 4);
        }
        bool room = false;
        if (step == -1) {
            step = length / 2;
            room = true;
        }
        int split = rng.below(length / 2) + length / 4;
        bool steep = rng.below(6) == 0;
        double d_yaw = 0, d_pitch = 0;
        for (; step < length; ++step) {
            double h_radius = 1.5 + std::sin(step * PI / length) * width;
            double v_radius = h_radius * y_scale;
            double cp = std::cos(pitch);
            x += std::cos(yaw) * cp;
            y += std::sin(pitch);
            z += std::sin(yaw) * cp;
            pitch *= steep ? 0.92 : 0.7;
            pitch += d_pitch * 0.1;
            yaw += d_yaw * 0.1;
            d_pitch *= 0.9;
            d_yaw *= 0.75;
            d_pitch += (rng.random() - rng.random()) * rng.random() * 2;
            d_yaw += (rng.random() - rng.random()) * rng.random() * 4;
            if (!room && step == split && width > 1) {
                tunnel(rng.next(), x, y, z, rng.random() * 0.5 + 0.5, yaw - PI / 2, pitch / 3, step, length, 1.0);
                tunnel(rng.next(), x, y, z, rng.random() * 0.5 + 0.5, yaw + PI / 2, pitch / 3, step, length, 1.0);
                return;
            }
            if (!room && rng.below(4) == 0) continue;
            // Give up once the rest of the tunnel can't reach the region any more.
            double dx = x - centre_x_, dz = z - centre_z_, left = length - step, reach = width + 2 + RW;
            if (dx * dx + dz * dz - left * left > reach * reach) return;
            carve(x, y, z, h_radius, v_radius);
        }
    }

    void carve(double x, double y, double z, double hr, double vr) {
        int x0 = std::max(ifloor(x - hr) - 1, r_.x0), x1 = std::min(ifloor(x + hr) + 1, r_.x0 + RW - 1);
        int z0 = std::max(ifloor(z - hr) - 1, r_.z0), z1 = std::min(ifloor(z + hr) + 1, r_.z0 + RW - 1);
        int y0 = std::max(ifloor(y - vr) - 1, 1), y1 = std::min(ifloor(y + vr) + 1, HEIGHT - 8);
        if (x0 > x1 || z0 > z1 || y0 > y1) return;
        // Don't break into the sea or a lake from below.
        for (int bx = x0; bx <= x1; ++bx)
            for (int bz = z0; bz <= z1; ++bz)
                for (int by = y0; by <= y1 + 1; ++by)
                    if (r_.get(bx, by, bz) == WATER) return;
        for (int bx = x0; bx <= x1; ++bx) {
            double nx = (bx + 0.5 - x) / hr;
            for (int bz = z0; bz <= z1; ++bz) {
                double nz = (bz + 0.5 - z) / hr;
                if (nx * nx + nz * nz >= 1) continue;
                for (int by = y1; by >= y0; --by) {
                    double ny = (by + 0.5 - y) / vr;
                    if (ny <= -0.7 || nx * nx + ny * ny + nz * nz >= 1) continue;
                    BlockId b = r_.get(bx, by, bz);
                    if (b != STONE && b != DIRT && b != GRASS && b != SANDSTONE) continue;
                    r_.set(bx, by, bz, by < LAVA_LEVEL ? LAVA : AIR);
                    // Keep grass on top when a cave opens up through the surface.
                    if (b == GRASS && r_.get(bx, by - 1, bz) == DIRT) r_.set(bx, by - 1, bz, GRASS);
                }
            }
        }
    }

    Region& r_;
    double centre_x_, centre_z_;
};

// --- Lakes ------------------------------------------------------------------------------------

// A blob of liquid in a 16 x 8 x 16 box with its corner at (ox, oy, oz); the bottom half is
// liquid and the top half air. Beta's rules: no lake that would leak out of its box.
bool lake(Region& r, Rng& rng, int ox, int oy, int oz, BlockId liquid) {
    constexpr int W = 16, H = 8;
    if (oy <= 4) return false;
    std::vector<bool> shape(W * W * H, false);
    auto at = [&](int x, int y, int z) { return shape[(x * W + z) * H + y]; };
    int blobs = rng.below(4) + 4;
    for (int i = 0; i < blobs; ++i) {
        double sx = rng.random() * 6 + 3, sy = rng.random() * 4 + 2, sz = rng.random() * 6 + 3;
        double px = rng.random() * (W - sx - 2) + 1 + sx / 2;
        double py = rng.random() * (H - sy - 4) + 2 + sy / 2;
        double pz = rng.random() * (W - sz - 2) + 1 + sz / 2;
        for (int x = 1; x < W - 1; ++x)
            for (int z = 1; z < W - 1; ++z)
                for (int y = 1; y < H - 1; ++y) {
                    double dx = (x - px) / (sx / 2), dy = (y - py) / (sy / 2), dz = (z - pz) / (sz / 2);
                    if (dx * dx + dy * dy + dz * dz < 1) shape[(x * W + z) * H + y] = true;
                }
    }
    auto border = [&](int x, int y, int z) {
        return !at(x, y, z) && ((x < W - 1 && at(x + 1, y, z)) || (x > 0 && at(x - 1, y, z)) ||
                                (z < W - 1 && at(x, y, z + 1)) || (z > 0 && at(x, y, z - 1)) ||
                                (y < H - 1 && at(x, y + 1, z)) || (y > 0 && at(x, y - 1, z)));
    };
    for (int x = 0; x < W; ++x)
        for (int z = 0; z < W; ++z)
            for (int y = 0; y < H; ++y) {
                if (!border(x, y, z)) continue;
                BlockId b = r.get(ox + x, oy + y, oz + z);
                if (y >= H / 2 && is_fluid(b)) return false;
                if (y < H / 2 && !is_solid(b) && b != liquid) return false;
            }
    for (int x = 0; x < W; ++x)
        for (int z = 0; z < W; ++z)
            for (int y = 0; y < H; ++y)
                if (at(x, y, z)) r.set(ox + x, oy + y, oz + z, y >= H / 2 ? BlockId{AIR} : liquid);
    for (int x = 0; x < W; ++x)
        for (int z = 0; z < W; ++z)
            for (int y = H / 2; y < H; ++y)
                if (at(x, y, z) && r.get(ox + x, oy + y - 1, oz + z) == DIRT) r.set(ox + x, oy + y - 1, oz + z, GRASS);
    if (liquid == LAVA)  // lava lakes get a stone rim
        for (int x = 0; x < W; ++x)
            for (int z = 0; z < W; ++z)
                for (int y = 0; y < H; ++y)
                    if (border(x, y, z) && (y < H / 2 || rng.below(2) != 0) && is_solid(r.get(ox + x, oy + y, oz + z)))
                        r.set(ox + x, oy + y, oz + z, STONE);
    return true;
}

// --- Dungeons ---------------------------------------------------------------------------------

// A mossy cobblestone room with a spawner in the middle, only where it fits underground with
// 1 to 5 openings into caves (Beta's rule). Loot chests come once chests exist.
bool dungeon(Region& r, Rng& rng, int x, int y, int z) {
    const int h = 3, rx = rng.below(2) + 2, rz = rng.below(2) + 2;
    int openings = 0;
    for (int i = x - rx - 1; i <= x + rx + 1; ++i)
        for (int j = y - 1; j <= y + h + 1; ++j)
            for (int k = z - rz - 1; k <= z + rz + 1; ++k) {
                bool solid = is_solid(r.get(i, j, k));
                if ((j == y - 1 || j == y + h + 1) && !solid) return false;
                bool wall = i == x - rx - 1 || i == x + rx + 1 || k == z - rz - 1 || k == z + rz + 1;
                if (wall && j == y && r.get(i, j, k) == AIR && r.get(i, j + 1, k) == AIR) ++openings;
            }
    if (openings < 1 || openings > 5) return false;
    for (int i = x - rx - 1; i <= x + rx + 1; ++i)
        for (int j = y + h; j >= y - 1; --j)
            for (int k = z - rz - 1; k <= z + rz + 1; ++k) {
                bool inner = i != x - rx - 1 && i != x + rx + 1 && k != z - rz - 1 && k != z + rz + 1 && j != y - 1;
                if (inner)
                    r.set(i, j, k, AIR);
                else if (j >= 0 && !is_solid(r.get(i, j - 1, k)))
                    r.set(i, j, k, AIR);  // no walls hanging over a cave
                else if (is_solid(r.get(i, j, k)))
                    r.set(i, j, k, j == y - 1 && rng.below(4) != 0 ? MOSSY_COBBLE : COBBLE);
            }
    r.set(x, y, z, SPAWNER);
    return true;
}

// --- Ores, dirt, gravel and clay --------------------------------------------------------------

// Beta's vein: blobs along a short line, replacing only `replace`. Veins don't change anything
// other features look at, so only the blobs inside the kept chunk are placed.
void vein(Region& r, Rng& rng, int x, int y, int z, int size, BlockId ore, BlockId replace = STONE) {
    double a = rng.random() * PI;
    double x1 = x + std::sin(a) * size / 8, x2 = x - std::sin(a) * size / 8;
    double z1 = z + std::cos(a) * size / 8, z2 = z - std::cos(a) * size / 8;
    double y1 = y + rng.below(3) - 2, y2 = y + rng.below(3) - 2;
    for (int i = 0; i <= size; ++i) {
        double cx = x1 + (x2 - x1) * i / size, cy = y1 + (y2 - y1) * i / size, cz = z1 + (z2 - z1) * i / size;
        double s = rng.random() * size / 16;
        double radius = ((std::sin(i * PI / size) + 1) * s + 1) / 2;
        // Only the chunk in the middle is kept, and nothing else depends on ores.
        const int kx = r.x0 + MARGIN, kz = r.z0 + MARGIN;
        if (cx + radius < kx || cx - radius >= kx + CHUNK || cz + radius < kz || cz - radius >= kz + CHUNK) continue;
        for (int bx = ifloor(cx - radius); bx <= ifloor(cx + radius); ++bx)
            for (int by = ifloor(cy - radius); by <= ifloor(cy + radius); ++by)
                for (int bz = ifloor(cz - radius); bz <= ifloor(cz + radius); ++bz) {
                    double dx = (bx + 0.5 - cx) / radius, dy = (by + 0.5 - cy) / radius, dz = (bz + 0.5 - cz) / radius;
                    if (dx * dx + dy * dy + dz * dz < 1 && r.get(bx, by, bz) == replace) r.set(bx, by, bz, ore);
                }
    }
}

struct OreRule {
    BlockId block;
    int tries, size, max_y;
};
// Beta's ore table (per chunk).
const OreRule ORES[] = {
    {DIRT, 20, 32, HEIGHT}, {GRAVEL, 10, 32, HEIGHT}, {COAL_ORE, 20, 16, HEIGHT}, {IRON_ORE, 20, 8, 64},
    {GOLD_ORE, 2, 8, 32},   {REDSTONE_ORE, 8, 7, 16}, {DIAMOND_ORE, 1, 7, 16},
};

// A patch of clay in the sand or dirt under shallow water.
void clay(Region& r, Rng& rng, int x, int z) {
    if (r.get(x, SEA_LEVEL, z) != WATER) return;
    int floor = SEA_LEVEL;
    while (floor > 0 && r.get(x, floor, z) == WATER) --floor;
    if (SEA_LEVEL - floor > 4) return;  // only shallow water
    int radius = rng.below(2) + 2;
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz) {
            if (dx * dx + dz * dz > radius * radius) continue;
            for (int y = floor - 1; y <= floor + 1; ++y) {
                BlockId b = r.get(x + dx, y, z + dz);
                if ((b == SAND || b == DIRT || b == GRAVEL) && r.get(x + dx, y + 1, z + dz) == WATER)
                    r.set(x + dx, y, z + dz, CLAY);
            }
        }
}

// --- Trees ------------------------------------------------------------------------------------

void leaf(Region& r, int x, int y, int z, BlockId leaves) {
    if (r.get(x, y, z) == AIR) r.set(x, y, z, leaves);
}
void wood(Region& r, int x, int y, int z, BlockId log) {
    BlockId b = r.get(x, y, z);
    if (b == AIR || is_leaves(b)) r.set(x, y, z, log);
}

// Room for a trunk of this height above the ground block?
bool trunk_fits(const Region& r, int x, int ground, int z, int height) {
    if (ground + height + 2 >= HEIGHT) return false;
    for (int y = ground + 1; y <= ground + height; ++y)
        if (r.get(x, y, z) != AIR && !is_leaves(r.get(x, y, z))) return false;
    return true;
}

// Oak and birch: Beta's small round tree.
void small_tree(Region& r, Rng& rng, int x, int ground, int z, int height, BlockId log, BlockId leaves) {
    if (!trunk_fits(r, x, ground, z, height)) return;
    int top = ground + height;
    for (int y = top - 3; y <= top; ++y) {
        int radius = 1 - (y - top) / 2;
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::abs(dx) == radius && std::abs(dz) == radius && (rng.below(2) == 0 || y == top)) continue;
                leaf(r, x + dx, y, z + dz, leaves);
            }
    }
    for (int y = ground + 1; y < top; ++y) wood(r, x, y, z, log);
    r.set(x, ground, z, DIRT);
}

// Spruce: layered cone of leaves.
void spruce(Region& r, Rng& rng, int x, int ground, int z) {
    int height = rng.below(4) + 6, bare = 1 + rng.below(2), widest = 2 + rng.below(2);
    if (!trunk_fits(r, x, ground, z, height)) return;
    int top = ground + height + 1;
    int radius = rng.below(2), limit = 1, restart = 0;
    for (int y = top; y >= ground + 1 + bare; --y) {
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz)
                if (!(radius > 0 && std::abs(dx) == radius && std::abs(dz) == radius)) leaf(r, x + dx, y, z + dz, SPRUCE_LEAVES);
        if (radius >= limit) {
            radius = restart;
            restart = 1;
            limit = std::min(limit + 1, widest);
        } else {
            ++radius;
        }
    }
    int trunk_top = top - rng.below(3);
    for (int y = ground + 1; y < trunk_top; ++y) wood(r, x, y, z, SPRUCE_LOG);
    r.set(x, ground, z, DIRT);
}

// Pine: tall bare trunk with a small tuft of leaves at the top.
void pine(Region& r, Rng& rng, int x, int ground, int z) {
    int height = rng.below(5) + 7, leafy = 2 + rng.below(2), widest = 1 + rng.below(2);
    if (!trunk_fits(r, x, ground, z, height)) return;
    int top = ground + height;
    for (int i = 0; i <= leafy; ++i) {
        int y = top + 1 - i;
        int radius = std::min(widest, i);
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz)
                if (!(radius > 0 && std::abs(dx) == radius && std::abs(dz) == radius)) leaf(r, x + dx, y, z + dz, SPRUCE_LEAVES);
    }
    for (int y = ground + 1; y <= top; ++y) wood(r, x, y, z, SPRUCE_LOG);
    r.set(x, ground, z, DIRT);
}

void leaf_ball(Region& r, double cx, double cy, double cz, double radius) {
    for (int x = ifloor(cx - radius); x <= ifloor(cx + radius); ++x)
        for (int y = ifloor(cy - radius / 2); y <= ifloor(cy + radius / 2); ++y)
            for (int z = ifloor(cz - radius); z <= ifloor(cz + radius); ++z) {
                double dx = x + 0.5 - cx, dy = (y + 0.5 - cy) * 2, dz = z + 0.5 - cz;
                if (dx * dx + dy * dy + dz * dz < radius * radius) leaf(r, x, y, z, LEAVES);
            }
}

// Big oak: a tall trunk with branches ending in clusters of leaves.
void big_oak(Region& r, Rng& rng, int x, int ground, int z) {
    int height = 8 + rng.below(6);
    if (!trunk_fits(r, x, ground, z, height)) return;
    int top = ground + height;
    int branches = 2 + rng.below(3);
    for (int i = 0; i < branches; ++i) {
        double a = rng.random() * PI * 2, length = 2 + rng.random() * 2.5;
        double sy = ground + height * 0.5 + rng.random() * height * 0.35;
        double ex = x + 0.5 + std::cos(a) * length, ez = z + 0.5 + std::sin(a) * length, ey = sy + length * 0.6;
        leaf_ball(r, ex, ey + 1, ez, 2.6);
        for (int s = 0; s <= 8; ++s) {
            double t = s / 8.0;
            wood(r, ifloor(x + 0.5 + (ex - x - 0.5) * t), ifloor(sy + (ey - sy) * t), ifloor(z + 0.5 + (ez - z - 0.5) * t), LOG);
        }
    }
    leaf_ball(r, x + 0.5, top + 0.5, z + 0.5, 3.0);
    for (int y = ground + 1; y <= top; ++y) wood(r, x, y, z, LOG);
    r.set(x, ground, z, DIRT);
}

// Beta's extra trees per chunk by biome (on top of 0 to 3 from noise).
int biome_trees(Biome b) {
    switch (b) {
        case Biome::FOREST: case Biome::RAINFOREST: case Biome::TAIGA: return 5;
        case Biome::SEASONAL_FOREST: return 2;
        case Biome::DESERT: case Biome::ICE_DESERT: case Biome::TUNDRA: case Biome::PLAINS: return -20;
        default: return 0;
    }
}

void tree(Region& r, Rng& rng, Biome b, int x, int z) {
    int ground = r.top(x, z);
    if (ground < 1) return;
    BlockId soil = r.get(x, ground, z);
    if (soil != GRASS && soil != DIRT) return;
    if (b == Biome::TAIGA) {
        if (rng.below(3) == 0) pine(r, rng, x, ground, z);
        else spruce(r, rng, x, ground, z);
    } else if (b == Biome::FOREST && rng.below(5) == 0) {
        small_tree(r, rng, x, ground, z, 5 + rng.below(3), BIRCH_LOG, BIRCH_LEAVES);
    } else if (rng.below(b == Biome::RAINFOREST ? 3 : 10) == 0) {
        big_oak(r, rng, x, ground, z);
    } else {
        small_tree(r, rng, x, ground, z, 4 + rng.below(3), LOG, LEAVES);
    }
}

}  // namespace

double hash01(int x, int z, int salt, uint64_t seed) {
    uint32_t h = (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(z) * 19349663u) ^
                 (static_cast<uint32_t>(salt) * 83492791u) ^ (static_cast<uint32_t>(seed) * 2654435761u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<double>(h ^ (h >> 16)) / 4294967296.0;
}

// The 2D shape of the land at a column.
struct TerrainGenerator::Column {
    double base;   // height the land centres on
    double spread; // blocks of height per unit of density (bigger = taller, steeper hills)
    double wobble; // strength of the 3D noise (overhangs, arches, floating bits)
};

TerrainGenerator::TerrainGenerator(uint64_t seed)
    : seed_(seed), continent_(seed), roughness_(seed + 1), mountains_(seed + 2), temperature_(seed + 3),
      rainfall_(seed + 4), climate_detail_(seed + 5), surface_(seed + 6), beach_(seed + 7), trees_(seed + 8),
      detail_(seed + 9) {}

void TerrainGenerator::climate_at(int wx, int wz, double& temperature, double& rainfall) const {
    double fine = climate_detail_.noise(wx / 9.0, wz / 9.0) * 0.012;
    temperature = std::clamp(0.7 + temperature_.fbm(wx / 420.0, wz / 420.0, 4) * 1.7 + fine, 0.0, 1.0);
    rainfall = std::clamp(0.5 + rainfall_.fbm(wx / 300.0 + 70, wz / 300.0 - 40, 4) * 1.7 + fine, 0.0, 1.0);
}

Biome TerrainGenerator::biome_at(int wx, int wz) const {
    double t, r;
    climate_at(wx, wz, t, r);
    return biome_for(t, r);
}

Climate TerrainGenerator::climate(int cx, int cz) const {
    Climate c;
    for (int z = 0; z < CHUNK; ++z)
        for (int x = 0; x < CHUNK; ++x) {
            double t, r;
            climate_at(cx * CHUNK + x, cz * CHUNK + z, t, r);
            c.temperature[(z << 4) | x] = static_cast<uint8_t>(std::lround(t * 255));
            c.rainfall[(z << 4) | x] = static_cast<uint8_t>(std::lround(r * 255));
        }
    return c;
}

TerrainGenerator::Column TerrainGenerator::column(int wx, int wz) const {
    double cont = continent_.fbm(wx / 320.0, wz / 320.0, 4);
    double rough = std::clamp(0.45 + roughness_.fbm(wx / 240.0 + 30, wz / 240.0 - 30, 3) * 1.5, 0.0, 1.0);
    double mount = std::max(0.0, mountains_.fbm(wx / 200.0 + 50, wz / 200.0 + 50, 3) - 0.08);
    Column c;
    c.base = SEA_LEVEL + 3 + cont * 30 + mount * mount * 140;
    c.spread = 8 + rough * 36 + mount * 40;
    c.wobble = 0.45 + rough * 1.3;
    return c;
}

double TerrainGenerator::density(int wx, int y, int wz) const { return density(column(wx, wz), wx, y, wz); }

double TerrainGenerator::density(const Column& c, int wx, int y, int wz) const {
    double d = (c.base - y) / c.spread + detail_.fbm(wx / 50.0, y / 30.0, wz / 50.0, 4) * c.wobble;
    if (y > HEIGHT - 20) d -= (y - (HEIGHT - 20)) / 6.0;  // keep the top of the world open
    if (y < 12) d += (12 - y) / 4.0;                     // and the bottom solid
    return d;
}

std::unique_ptr<Chunk> TerrainGenerator::generate(int cx, int cz) const {
    Region r{cx * CHUNK - MARGIN, cz * CHUNK - MARGIN};

    // Climate and biome of every column.
    for (int lz = 0; lz < RW; ++lz)
        for (int lx = 0; lx < RW; ++lx) r.biomes[Region::col(lx, lz)] = biome_at(r.x0 + lx, r.z0 + lz);

    // 1. Terrain from 3D noise, sampled every 4 x 8 x 4 blocks and blended in between (like Beta).
    constexpr int SX = 4, SY = 8, NX = RW / SX + 1, NY = HEIGHT / SY + 1;
    std::vector<double> grid(NX * NX * NY);
    auto g = [&](int ix, int iy, int iz) -> double& { return grid[(ix * NX + iz) * NY + iy]; };
    for (int ix = 0; ix < NX; ++ix)
        for (int iz = 0; iz < NX; ++iz) {
            int wx = r.x0 + ix * SX, wz = r.z0 + iz * SX;
            Column c = column(wx, wz);
            for (int iy = 0; iy < NY; ++iy) g(ix, iy, iz) = density(c, wx, iy * SY, wz);
        }
    for (int lx = 0; lx < RW; ++lx) {
        int ix = lx / SX;
        double fx = (lx % SX) / static_cast<double>(SX);
        for (int lz = 0; lz < RW; ++lz) {
            int iz = lz / SX;
            double fz = (lz % SX) / static_cast<double>(SX);
            // Blend across x and z once per grid level, then down the column.
            double level[NY];
            for (int iy = 0; iy < NY; ++iy) {
                double a = g(ix, iy, iz) + (g(ix + 1, iy, iz) - g(ix, iy, iz)) * fx;
                double b = g(ix, iy, iz + 1) + (g(ix + 1, iy, iz + 1) - g(ix, iy, iz + 1)) * fx;
                level[iy] = a + (b - a) * fz;
            }
            BlockId* col = &r.blocks[Region::col(lx, lz) * HEIGHT];
            for (int y = 0; y < HEIGHT; ++y) {
                int iy = y / SY;
                double d = level[iy] + (level[iy + 1] - level[iy]) * ((y % SY) / static_cast<double>(SY));
                col[y] = d > 0 ? STONE : y <= SEA_LEVEL ? WATER : AIR;
            }
            // Bedrock: the bottom layer, then thinning out over the next four.
            int wx = r.x0 + lx, wz = r.z0 + lz;
            col[0] = BEDROCK;
            for (int y = 1; y <= 4; ++y)
                if (hash01(wx, wz, 100 + y, seed_) * 5 < 5 - y) col[y] = BEDROCK;
        }
    }

    // 2. Surface: grass and dirt, sand in deserts, sand and gravel beaches, sandstone under sand.
    for (int lx = 0; lx < RW; ++lx)
        for (int lz = 0; lz < RW; ++lz) {
            int wx = r.x0 + lx, wz = r.z0 + lz;
            Biome b = r.biomes[Region::col(lx, lz)];
            bool sandy = b == Biome::DESERT || b == Biome::ICE_DESERT;
            BlockId biome_top = sandy ? SAND : GRASS, biome_fill = sandy ? SAND : DIRT;
            double rnd = hash01(wx, wz, 7, seed_);
            bool sand_beach = beach_.noise(wx / 24.0, wz / 24.0) + rnd * 0.2 > 0.0;
            bool gravel_beach = beach_.noise(wz / 24.0 + 100, wx / 24.0) + rnd * 0.2 > 0.45;
            int depth = static_cast<int>(surface_.noise(wx / 10.0, wz / 10.0) * 1.6 + 3 + rnd * 0.25);
            BlockId* col = &r.blocks[Region::col(lx, lz) * HEIGHT];
            int counter = -1;
            BlockId top = biome_top, fill = biome_fill;
            for (int y = HEIGHT - 1; y >= 0; --y) {
                BlockId blk = col[y];
                if (blk == AIR || blk == WATER) {
                    counter = -1;
                    continue;
                }
                if (blk != STONE) continue;
                if (counter == -1) {
                    top = biome_top;
                    fill = biome_fill;
                    if (depth <= 0) {
                        top = STONE;
                        fill = STONE;
                    } else if (y >= SEA_LEVEL - 4 && y <= SEA_LEVEL + 1) {
                        if (gravel_beach) top = fill = GRAVEL;
                        else if (sand_beach) top = fill = SAND;
                    }
                    counter = depth;
                    // Under water the top block is the filler (no grass on the sea floor).
                    col[y] = (y >= SEA_LEVEL && col[std::min(y + 1, HEIGHT - 1)] != WATER) ? top : fill;
                } else if (counter > 0) {
                    --counter;
                    col[y] = fill;
                    if (counter == 0 && fill == SAND) {
                        counter = static_cast<int>(hash01(wx, wz, 8, seed_) * 4);
                        fill = SANDSTONE;
                    }
                }
            }
        }

    // 3. Caves from every chunk close enough to reach the region.
    CaveCarver carver(r, cx, cz);
    for (int sx = cx - CAVE_RANGE; sx <= cx + CAVE_RANGE; ++sx)
        for (int sz = cz - CAVE_RANGE; sz <= cz + CAVE_RANGE; ++sz) {
            Rng rng(chunk_seed(seed_, sx, sz, CAVES));
            carver.from_chunk(rng, sx, sz);
        }

    // 4. Features from the four chunks that can reach this one, one kind at a time.
    auto sources = [&](uint64_t salt, auto&& fn) {
        for (int sz = cz - 1; sz <= cz; ++sz)
            for (int sx = cx - 1; sx <= cx; ++sx) {
                Rng rng(chunk_seed(seed_, sx, sz, salt));
                fn(rng, sx * CHUNK + 8, sz * CHUNK + 8);  // features sit 8 to 24 blocks into the source
            }
    };
    sources(LAKES, [&](Rng& rng, int x, int z) {
        if (rng.below(4) == 0) {
            int ox = x - 8 + rng.below(16), oz = z - 8 + rng.below(16), y = rng.below(HEIGHT);
            while (y > 0 && r.get(ox + 8, y, oz + 8) == AIR) --y;  // sit on the ground
            lake(r, rng, ox, y - 4, oz, WATER);
        }
        if (rng.below(8) == 0) {
            int ox = x - 8 + rng.below(16), oz = z - 8 + rng.below(16);
            int y = rng.below(rng.below(HEIGHT - 8) + 8);
            if (y < SEA_LEVEL || rng.below(10) == 0) lake(r, rng, ox, y - 4, oz, LAVA);
        }
    });
    sources(DUNGEONS, [&](Rng& rng, int x, int z) {
        for (int i = 0; i < 8; ++i) dungeon(r, rng, x + rng.below(16), rng.below(HEIGHT), z + rng.below(16));
    });
    sources(CLAY_SALT, [&](Rng& rng, int x, int z) {
        for (int i = 0; i < 2; ++i) clay(r, rng, x + rng.below(16), z + rng.below(16));
    });
    sources(TREES, [&](Rng& rng, int x, int z) {
        Biome b = r.biome(x + 8, z + 8);
        int n = static_cast<int>((trees_.noise(x / 80.0, z / 80.0) + 1) * 1.6 + rng.random()) + biome_trees(b);
        if (rng.below(10) == 0) ++n;
        for (int i = 0; i < n; ++i) {
            int tx = x + rng.below(16), tz = z + rng.below(16);
            tree(r, rng, r.biome(tx, tz), tx, tz);
        }
    });

    // Ores last: they only replace stone, so nothing above depends on them.
    sources(ORE_VEINS, [&](Rng& rng, int x, int z) {
        for (const OreRule& o : ORES)
            for (int i = 0; i < o.tries; ++i) vein(r, rng, x + rng.below(16), rng.below(o.max_y), z + rng.below(16), o.size, o.block);
        vein(r, rng, x + rng.below(16), rng.below(16) + rng.below(16), z + rng.below(16), 6, LAPIS_ORE);
    });

    // 5. Keep the chunk in the middle, freezing still water in cold biomes.
    auto chunk = std::make_unique<Chunk>(cx, cz);
    chunk->climate = climate(cx, cz);
    for (int lz = 0; lz < CHUNK; ++lz)
        for (int lx = 0; lx < CHUNK; ++lx) {
            const BlockId* col = &r.blocks[Region::col(lx + MARGIN, lz + MARGIN) * HEIGHT];
            for (int y = 0; y < HEIGHT; ++y) chunk->blocks[Chunk::index(lx, y, lz)] = col[y];
            if (is_cold(chunk->climate.biome((lz << 4) | lx)))
                for (int y = HEIGHT - 1; y >= 0; --y) {
                    BlockId& b = chunk->blocks[Chunk::index(lx, y, lz)];
                    if (b == AIR) continue;
                    if (b == WATER) b = ICE;
                    break;
                }
        }
    chunk->recompute_max_y();
    return chunk;
}
