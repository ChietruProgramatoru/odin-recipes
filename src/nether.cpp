// Nether generation works like the overworld's (worldgen.cpp): a chunk is generated inside a
// 48 x 48 region, features are anchored 8 to 24 blocks into one of the four chunks that can reach
// it and reach at most 8 blocks from their anchor, so neighbouring chunks agree on them.
#include "nether.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "noise.h"
#include "rng.h"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr int MARGIN = 16;
constexpr int RW = CHUNK + 2 * MARGIN;  // region width

// Salts so each kind of feature gets its own random numbers (and none match the overworld's).
enum Salt : uint64_t { GLOW = 101, FIRES, SHROOMS };

uint64_t chunk_seed(uint64_t seed, int sx, int sz, uint64_t salt) {
    Rng mix(seed ^ (static_cast<uint64_t>(static_cast<uint32_t>(sx)) * 0x9E3779B97F4A7C15ull) ^
            (static_cast<uint64_t>(static_cast<uint32_t>(sz)) * 0xC2B2AE3D27D4EB4Full) ^ (salt << 56));
    return mix.next();
}

struct Region {
    int x0, z0;
    std::vector<BlockId> blocks = std::vector<BlockId>(RW * RW * HEIGHT, AIR);  // column-major: y fastest

    static int col(int lx, int lz) { return lz * RW + lx; }
    bool inside(int x, int y, int z) const { return x >= x0 && x < x0 + RW && z >= z0 && z < z0 + RW && y >= 0 && y < HEIGHT; }
    BlockId get(int x, int y, int z) const { return inside(x, y, z) ? blocks[col(x - x0, z - z0) * HEIGHT + y] : BlockId{AIR}; }
    void set(int x, int y, int z, BlockId b) {
        if (inside(x, y, z)) blocks[col(x - x0, z - z0) * HEIGHT + y] = b;
    }
};

// Beta's glowstone (WorldGenLightGem): a block under the roof, then blocks grown down and around
// it, each touching exactly one glowstone block, so it hangs in branching clumps.
void glowstone(Region& r, Rng& rng, int x, int y, int z) {
    if (r.get(x, y, z) != AIR || r.get(x, y + 1, z) != NETHERRACK) return;
    r.set(x, y, z, GLOWSTONE);
    for (int i = 0; i < 1000; ++i) {
        const int gx = x + rng.below(8) - rng.below(8), gy = y - rng.below(12), gz = z + rng.below(8) - rng.below(8);
        if (r.get(gx, gy, gz) != AIR) continue;
        int touching = 0;
        const int n[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const auto& d : n)
            if (r.get(gx + d[0], gy + d[1], gz + d[2]) == GLOWSTONE) ++touching;
        if (touching == 1) r.set(gx, gy, gz, GLOWSTONE);
    }
}

// Beta's WorldGenFire: a patch of fires on netherrack.
void fires(Region& r, Rng& rng, int x, int y, int z) {
    for (int i = 0; i < 64; ++i) {
        const int fx = x + rng.below(8) - rng.below(8), fy = y + rng.below(4) - rng.below(4), fz = z + rng.below(8) - rng.below(8);
        if (r.get(fx, fy, fz) == AIR && r.get(fx, fy - 1, fz) == NETHERRACK) r.set(fx, fy, fz, FIRE);
    }
}

// A few mushrooms in the dark on netherrack (Beta's nether flowers).
void mushrooms(Region& r, Rng& rng, int x, int y, int z, BlockId kind) {
    for (int i = 0; i < 64; ++i) {
        const int mx = x + rng.below(8) - rng.below(8), my = y + rng.below(4) - rng.below(4), mz = z + rng.below(8) - rng.below(8);
        if (r.get(mx, my, mz) == AIR && r.get(mx, my - 1, mz) == NETHERRACK) r.set(mx, my, mz, kind);
    }
}

bool frame_block(World& w, glm::ivec3 p) { return w.get_block(p.x, p.y, p.z) == OBSIDIAN; }

}  // namespace

std::unique_ptr<Chunk> generate_nether(uint64_t seed, int cx, int cz) {
    Region r{cx * CHUNK - MARGIN, cz * CHUNK - MARGIN};
    const Perlin3D caverns(seed + 201), lumps(seed + 202);
    const Perlin2D shore(seed + 203), roof(seed + 204), floor_noise(seed + 205);

    // 1. Netherrack from 3D noise, solid towards the floor and the roof and open in between (Beta's
    // cosine profile), sampled every 4 x 8 x 4 blocks and blended. Open space low down is the lava sea.
    constexpr int SX = 4, SY = 8, NX = RW / SX + 1, NY = HEIGHT / SY + 1;
    std::vector<double> grid(NX * NX * NY);
    auto g = [&](int ix, int iy, int iz) -> double& { return grid[(ix * NX + iz) * NY + iy]; };
    for (int ix = 0; ix < NX; ++ix)
        for (int iz = 0; iz < NX; ++iz) {
            const int wx = r.x0 + ix * SX, wz = r.z0 + iz * SX;
            const double ceiling = roof.fbm(wx / 90.0, wz / 90.0, 3) * 0.35, ground = floor_noise.fbm(wx / 70.0, wz / 70.0, 3) * 0.35;
            for (int iy = 0; iy < NY; ++iy) {
                const int y = iy * SY;
                const double t = y / static_cast<double>(HEIGHT - 1);
                double d = caverns.fbm(wx / 56.0, y / 36.0, wz / 56.0, 4) * 1.1 + lumps.fbm(wx / 18.0, y / 14.0, wz / 18.0, 2) * 0.25;
                d += std::cos(t * 2 * PI) * 0.62 - 0.12 + (t > 0.5 ? ceiling : ground);
                if (y < 8) d += (8 - y) / 3.0;
                if (y > HEIGHT - 12) d += (y - (HEIGHT - 12)) / 3.0;
                g(ix, iy, iz) = d;
            }
        }
    for (int lx = 0; lx < RW; ++lx) {
        const int ix = lx / SX;
        const double fx = (lx % SX) / static_cast<double>(SX);
        for (int lz = 0; lz < RW; ++lz) {
            const int iz = lz / SX;
            const double fz = (lz % SX) / static_cast<double>(SX);
            double level[NY];
            for (int iy = 0; iy < NY; ++iy) {
                const double a = g(ix, iy, iz) + (g(ix + 1, iy, iz) - g(ix, iy, iz)) * fx;
                const double b = g(ix, iy, iz + 1) + (g(ix + 1, iy, iz + 1) - g(ix, iy, iz + 1)) * fx;
                level[iy] = a + (b - a) * fz;
            }
            BlockId* col = &r.blocks[Region::col(lx, lz) * HEIGHT];
            const int wx = r.x0 + lx, wz = r.z0 + lz;
            for (int y = 0; y < HEIGHT; ++y) {
                const int iy = std::min(y / SY, NY - 2);
                const double d = level[iy] + (level[iy + 1] - level[iy]) * ((y - iy * SY) / static_cast<double>(SY));
                col[y] = d > 0 ? NETHERRACK : y < NETHER_LAVA_LEVEL ? LAVA : AIR;
            }
            // Bedrock floor and roof, thinning out over the next four layers (Beta).
            col[0] = col[HEIGHT - 1] = BEDROCK;
            for (int k = 1; k <= 4; ++k) {
                if (hash01(wx, wz, 300 + k, seed) * 5 < 5 - k) col[k] = BEDROCK;
                if (hash01(wx, wz, 310 + k, seed) * 5 < 5 - k) col[HEIGHT - 1 - k] = BEDROCK;
            }
            // 2. Soul sand and gravel on ground near the lava sea's shore (Beta lays them around y 64
            // of its 128; here the shore is lower, so they go where you walk out of the lava).
            const double s = shore.noise(wx / 18.0, wz / 18.0) + hash01(wx, wz, 320, seed) * 0.2;
            const double gv = shore.noise(wz / 14.0 + 50, wx / 14.0 - 50) + hash01(wx, wz, 321, seed) * 0.2;
            const int depth = 1 + static_cast<int>(hash01(wx, wz, 322, seed) * 3);
            for (int y = NETHER_LAVA_LEVEL + 12; y >= NETHER_LAVA_LEVEL - 4; --y) {
                if (col[y] != NETHERRACK || col[y + 1] != AIR) continue;
                const BlockId top = s > 0.25 ? SOUL_SAND : gv > 0.4 ? GRAVEL : NETHERRACK;
                if (top == NETHERRACK) continue;
                for (int k = 0; k < depth && y - k > 0 && col[y - k] == NETHERRACK; ++k) col[y - k] = top;
            }
        }
    }

    // 3. Features from the four chunks that can reach this one.
    auto sources = [&](uint64_t salt, auto&& fn) {
        for (int sz = cz - 1; sz <= cz; ++sz)
            for (int sx = cx - 1; sx <= cx; ++sx) {
                Rng rng(chunk_seed(seed, sx, sz, salt));
                fn(rng, sx * CHUNK + 8, sz * CHUNK + 8);
            }
    };
    sources(FIRES, [&](Rng& rng, int x, int z) {
        for (int i = 0, n = rng.below(rng.below(10) + 1) + 1; i < n; ++i)
            fires(r, rng, x + rng.below(16), rng.below(HEIGHT - 8) + 4, z + rng.below(16));
    });
    sources(GLOW, [&](Rng& rng, int x, int z) {
        // Glowstone clusters only start right under netherrack, so most tries find nothing; search
        // up from a random height to the roof above it.
        for (int i = 0, n = rng.below(rng.below(10) + 1) + 4; i < n; ++i) {
            const int gx = x + rng.below(16), gz = z + rng.below(16);
            int gy = rng.below(HEIGHT - 16) + NETHER_LAVA_LEVEL;
            while (gy < HEIGHT - 2 && r.get(gx, gy + 1, gz) == AIR) ++gy;
            glowstone(r, rng, gx, gy, gz);
        }
    });
    sources(SHROOMS, [&](Rng& rng, int x, int z) {
        if (rng.below(2) == 0) mushrooms(r, rng, x + rng.below(16), rng.below(HEIGHT), z + rng.below(16), BROWN_MUSHROOM);
        if (rng.below(2) == 0) mushrooms(r, rng, x + rng.below(16), rng.below(HEIGHT), z + rng.below(16), RED_MUSHROOM);
    });

    // 4. Keep the chunk in the middle. The Nether is hot and dry everywhere (Beta's Hell biome).
    auto chunk = std::make_unique<Chunk>(cx, cz);
    chunk->climate.temperature.fill(255);
    chunk->climate.rainfall.fill(0);
    chunk->mob_spawned = true;  // no animals here
    for (int lz = 0; lz < CHUNK; ++lz)
        for (int lx = 0; lx < CHUNK; ++lx) {
            const BlockId* col = &r.blocks[Region::col(lx + MARGIN, lz + MARGIN) * HEIGHT];
            for (int y = 0; y < HEIGHT; ++y) chunk->blocks[Chunk::index(lx, y, lz)] = col[y];
        }
    chunk->recompute_max_y();
    return chunk;
}

// --- Portals -------------------------------------------------------------------------------------

std::vector<glm::ivec3> PortalShape::cells() const {
    std::vector<glm::ivec3> out;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) out.push_back(origin + dir() * i + glm::ivec3(0, j, 0));
    return out;
}

glm::dvec3 PortalShape::exit() const {
    const glm::dvec3 d(dir());
    const glm::dvec3 across = glm::dvec3(1, 0, 1) - d;  // the way you walk through it
    return glm::dvec3(origin) + d * 1.0 + across * 0.5;
}

std::optional<PortalShape> find_portal_frame(World& world, glm::ivec3 inside) {
    auto open = [&](glm::ivec3 p) {
        const BlockId b = world.get_block(p.x, p.y, p.z);
        return b == AIR || b == FIRE;
    };
    for (int axis = 0; axis < 2; ++axis)
        for (int k = 0; k < 2; ++k)
            for (int dy = 0; dy < 3; ++dy) {
                PortalShape s;
                s.axis = axis;
                s.origin = inside - s.dir() * k - glm::ivec3(0, dy, 0);
                const glm::ivec3 d = s.dir(), up(0, 1, 0);
                bool ok = true;
                for (glm::ivec3 c : s.cells()) ok = ok && open(c);
                for (int i = 0; i < 2 && ok; ++i)
                    ok = frame_block(world, s.origin + d * i - up) && frame_block(world, s.origin + d * i + up * 3);
                for (int j = 0; j < 3 && ok; ++j)
                    ok = frame_block(world, s.origin - d + up * j) && frame_block(world, s.origin + d * 2 + up * j);
                if (ok) return s;
            }
    return std::nullopt;
}

std::vector<ChunkPos> light_portal(World& world, const PortalShape& shape) {
    std::vector<ChunkPos> dirty;
    for (glm::ivec3 c : shape.cells())
        for (ChunkPos p : world.set_block(c.x, c.y, c.z, PORTAL, shape.axis))
            if (std::find(dirty.begin(), dirty.end(), p) == dirty.end()) dirty.push_back(p);
    return dirty;
}

bool portal_can_stay(World& world, glm::ivec3 p) {
    if (world.get_block(p.x, p.y, p.z) != PORTAL) return false;
    const glm::ivec3 d = (world.get_data(p.x, p.y, p.z) & 1) ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
    auto at = [&](glm::ivec3 q) { return world.get_block(q.x, q.y, q.z); };
    // Down to the bottom of the column, which must sit on obsidian, then exactly three high.
    glm::ivec3 b = p;
    while (b.y > 0 && at(b - glm::ivec3(0, 1, 0)) == PORTAL) --b.y;
    if (at(b - glm::ivec3(0, 1, 0)) != OBSIDIAN) return false;
    int h = 0;
    while (h < 4 && at(b + glm::ivec3(0, h, 0)) == PORTAL) ++h;
    if (h != 3 || at(b + glm::ivec3(0, 3, 0)) != OBSIDIAN) return false;
    // Obsidian or portal on both sides in its plane, and portal on at least one.
    const BlockId l = at(p - d), r = at(p + d);
    if ((l != PORTAL && l != OBSIDIAN) || (r != PORTAL && r != OBSIDIAN)) return false;
    return l == PORTAL || r == PORTAL;
}

std::optional<PortalShape> portal_at(World& world, glm::ivec3 p) {
    if (world.get_block(p.x, p.y, p.z) != PORTAL) return std::nullopt;
    PortalShape s;
    s.axis = world.get_data(p.x, p.y, p.z) & 1;
    glm::ivec3 o = p;
    while (o.y > 0 && world.get_block(o.x, o.y - 1, o.z) == PORTAL) --o.y;
    const glm::ivec3 back = o - s.dir();
    if (world.get_block(back.x, back.y, back.z) == PORTAL) o = back;
    s.origin = o;
    return s;
}

glm::ivec3 portal_target(glm::ivec3 pos, Dimension to) {
    auto div = [](int v) { return static_cast<int>(std::floor(v / static_cast<double>(NETHER_SCALE))); };
    if (to == Dimension::Nether)
        return {div(pos.x), std::clamp(pos.y, NETHER_LAVA_LEVEL + 4, HEIGHT - 12), div(pos.z)};
    return {pos.x * NETHER_SCALE, std::clamp(pos.y, SEA_LEVEL, HEIGHT - 12), pos.z * NETHER_SCALE};
}

PortalShape build_portal(World& world, glm::ivec3 target, std::vector<ChunkPos>& dirty) {
    constexpr int RANGE = 16;
    // Make sure the area is there to look at.
    for (int cx = chunk_of(target.x - RANGE - 2); cx <= chunk_of(target.x + RANGE + 2); ++cx)
        for (int cz = chunk_of(target.z - RANGE - 2); cz <= chunk_of(target.z + RANGE + 2); ++cz) world.chunk(cx, cz);
    auto at = [&](int x, int y, int z) { return world.get_block(x, y, z); };
    const bool nether = world.nether();
    const int y_lo = nether ? NETHER_LAVA_LEVEL : 1, y_hi = HEIGHT - 8;

    // The nearest spot (Beta's makePortal) with a floor of solid blocks 4 long and 3 wide, and air
    // 4 high above it, along either axis.
    auto fits = [&](int x, int y, int z, int axis) {
        for (int i = -1; i <= 2; ++i)
            for (int w = -1; w <= 1; ++w) {
                const int fx = x + (axis == 0 ? i : w), fz = z + (axis == 0 ? w : i);
                if (!is_opaque(at(fx, y - 1, fz))) return false;
                for (int h = 0; h < 4; ++h)
                    if (at(fx, y + h, fz) != AIR) return false;
            }
        return true;
    };
    std::optional<PortalShape> best;
    double best_d = 1e18;
    for (int x = target.x - RANGE; x <= target.x + RANGE; ++x)
        for (int z = target.z - RANGE; z <= target.z + RANGE; ++z)
            for (int y = y_hi; y >= y_lo; --y) {
                if (at(x, y, z) != AIR || !is_opaque(at(x, y - 1, z))) continue;
                const double dx = x - target.x, dy = y - target.y, dz = z - target.z;
                const double d = dx * dx + dy * dy * 0.25 + dz * dz;
                if (d >= best_d) continue;
                for (int axis = 0; axis < 2; ++axis)
                    if (fits(x, y, z, axis)) {
                        best = PortalShape{{x, y, z}, axis};
                        best_d = d;
                        break;
                    }
            }
    const bool forced = !best;
    PortalShape s = best ? *best : PortalShape{{target.x, std::clamp(target.y, y_lo + 6, y_hi - 6), target.z}, 0};

    auto set = [&](glm::ivec3 p, BlockId b, int data = 0) {
        for (ChunkPos c : world.set_block(p.x, p.y, p.z, b, data))
            if (std::find(dirty.begin(), dirty.end(), c) == dirty.end()) dirty.push_back(c);
    };
    const glm::ivec3 d = s.dir(), across = glm::ivec3(1, 0, 1) - d, up(0, 1, 0);
    // Room in front and behind, and a ledge to stand on if it hangs in the air.
    for (int i = -1; i <= 2; ++i)
        for (int w = -1; w <= 1; ++w) {
            const glm::ivec3 base = s.origin + d * i + across * w;
            if (w != 0) {
                for (int h = 0; h < 4; ++h) set(base + up * h, AIR);
                if (forced || !is_opaque(at(base.x, base.y - 1, base.z))) set(base - up, OBSIDIAN);
            }
        }
    // The frame, then the portal inside it.
    for (int i = -1; i <= 2; ++i) {
        set(s.origin + d * i - up, OBSIDIAN);
        set(s.origin + d * i + up * 3, OBSIDIAN);
    }
    for (int j = 0; j < 3; ++j) {
        set(s.origin - d + up * j, OBSIDIAN);
        set(s.origin + d * 2 + up * j, OBSIDIAN);
    }
    for (glm::ivec3 c : s.cells()) set(c, PORTAL, s.axis);
    return s;
}
