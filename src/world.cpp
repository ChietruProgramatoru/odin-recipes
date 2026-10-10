#include "world.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "textures.h"

namespace {

constexpr int off(int dx, int dy, int dz) { return dy * LAYER + dz * PW + dx; }
constexpr int off(glm::ivec3 d) { return off(d.x, d.y, d.z); }

const std::array<FaceDef, 6> FACE_DEFS = {{
    {{1, 0, 0}, {{{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}}}, 0.80f, 1},
    {{-1, 0, 0}, {{{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}}}, 0.80f, 1},
    {{0, 0, 1}, {{{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}}}, 0.68f, 1},
    {{0, 0, -1}, {{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}}, 0.68f, 1},
    {{0, 1, 0}, {{{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}}}, 1.00f, 0},
    {{0, -1, 0}, {{{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}}}, 0.50f, 2},
}};

// A face plus, for each corner, the padded-array offsets of the two side neighbours and the
// diagonal neighbour that darken it (ambient occlusion).
struct Face {
    int offset;
    const FaceDef* def;
    std::array<std::array<int, 3>, 4> ao;
};

std::array<Face, 6> build_faces() {
    std::array<Face, 6> faces{};
    for (int f = 0; f < 6; ++f) {
        const FaceDef& def = FACE_DEFS[f];
        int axis = def.normal.x ? 0 : def.normal.y ? 1 : 2;
        int tangents[2], n = 0;
        for (int a = 0; a < 3; ++a)
            if (a != axis) tangents[n++] = a;
        Face& face = faces[f];
        face.offset = off(def.normal);
        face.def = &def;
        for (int c = 0; c < 4; ++c) {
            glm::ivec3 corner = def.corners[c];
            glm::ivec3 d(0), vecs[2];
            for (int t = 0; t < 2; ++t) {
                int ax = tangents[t];
                int s = corner[ax] == 1 ? 1 : -1;
                vecs[t] = def.normal;
                vecs[t][ax] += s;
                d[ax] = s;
            }
            face.ao[c] = {off(vecs[0]), off(vecs[1]), off(def.normal + d)};
        }
    }
    return faces;
}

const std::array<Face, 6> FACES = build_faces();
constexpr float AO_LEVELS[4] = {1.0f, 0.78f, 0.62f, 0.48f};
constexpr float WATER_SURFACE = 0.875f;

struct BlockUvs {
    std::array<std::array<float, 4>, 3> uv;  // top, side, bottom
};

const std::array<BlockUvs, BLOCK_COUNT> BLOCK_UVS = [] {
    std::array<BlockUvs, BLOCK_COUNT> out{};
    for (int b = GRASS; b < BLOCK_COUNT; ++b)
        for (int s = 0; s < 3; ++s) out[b].uv[s] = tile_uv(block_info(b)->tiles[s]);
    return out;
}();

// Fast graphics draws leaves as solid blocks with their own darker tile.
const BlockUvs FAST_LEAVES_UVS = [] {
    BlockUvs out{};
    for (auto& uv : out.uv) uv = tile_uv(T_LEAVES_FAST);
    return out;
}();

const std::array<bool, 256> OPAQUE = [] {
    std::array<bool, 256> o{};
    for (int b = 0; b < 256; ++b) o[b] = is_opaque(static_cast<BlockId>(b));
    return o;
}();

void place(Chunk& chunk, int wx, int y, int wz, BlockId block, bool only_air = false) {
    int lx = wx - chunk.cx * CHUNK, lz = wz - chunk.cz * CHUNK;
    if (lx >= 0 && lx < CHUNK && lz >= 0 && lz < CHUNK && y >= 0 && y < HEIGHT) {
        BlockId& b = chunk.blocks[Chunk::index(lx, y, lz)];
        if (!only_air || b == AIR) b = block;
    }
}

int ifloor(double v) { return static_cast<int>(std::floor(v)); }

}  // namespace

const std::array<FaceDef, 6>& face_defs() { return FACE_DEFS; }

double hash01(int x, int z, int salt, uint64_t seed) {
    uint32_t h = (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(z) * 19349663u) ^
                 (static_cast<uint32_t>(salt) * 83492791u) ^ (static_cast<uint32_t>(seed) * 2654435761u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<double>(h ^ (h >> 16)) / 4294967296.0;
}

// --- Terrain ------------------------------------------------------------------------

TerrainGenerator::TerrainGenerator(uint64_t seed)
    : seed_(seed), continent_(seed), hills_(seed + 1), mountains_(seed + 2) {}

int TerrainGenerator::height(int wx, int wz) const {
    double c = continent_.fbm(wx / 220.0, wz / 220.0, 3);
    double hl = hills_.fbm(wx / 55.0, wz / 55.0, 4);
    double m = std::max(0.0, mountains_.fbm(wx / 110.0 + 50, wz / 110.0 + 50, 3));
    int h = static_cast<int>(SEA_LEVEL + 3 + c * 14 + hl * 6 + m * m * 70);
    return std::clamp(h, 3, HEIGHT - 12);
}

bool TerrainGenerator::is_tree(int wx, int wz) const { return hash01(wx, wz, 1, seed_) < 0.011; }

std::unique_ptr<Chunk> TerrainGenerator::generate(int cx, int cz) const {
    auto chunk = std::make_unique<Chunk>(cx, cz);
    auto& b = chunk->blocks;
    const int bx = cx * CHUNK, bz = cz * CHUNK;

    // Heights for the chunk plus a 2-block margin (trees may hang over from neighbouring columns).
    constexpr int M = 2, HW = CHUNK + 2 * M;
    int heights[HW * HW];
    for (int z = 0; z < HW; ++z)
        for (int x = 0; x < HW; ++x) heights[z * HW + x] = height(bx - M + x, bz - M + z);
    auto h_at = [&](int wx, int wz) { return heights[(wz - bz + M) * HW + (wx - bx + M)]; };

    int top = SEA_LEVEL;
    for (int lz = 0; lz < CHUNK; ++lz)
        for (int lx = 0; lx < CHUNK; ++lx) {
            int h = h_at(bx + lx, bz + lz);
            bool beach = h <= SEA_LEVEL + 1;
            b[Chunk::index(lx, 0, lz)] = BEDROCK;
            for (int y = 1; y <= h; ++y) {
                BlockId blk;
                if (y < h - 3)
                    blk = STONE;
                else if (y < h)
                    blk = beach ? SAND : DIRT;
                else if (beach)
                    blk = SAND;
                else
                    blk = h > SNOWLESS_PEAK ? STONE : GRASS;
                b[Chunk::index(lx, y, lz)] = blk;
            }
            for (int y = h + 1; y <= SEA_LEVEL; ++y) b[Chunk::index(lx, y, lz)] = WATER;
            top = std::max(top, h);
        }

    for (int wz = bz - M; wz < bz + CHUNK + M; ++wz)
        for (int wx = bx - M; wx < bx + CHUNK + M; ++wx) {
            if (!is_tree(wx, wz)) continue;
            int h = h_at(wx, wz);
            if (h <= SEA_LEVEL + 1 || h > SNOWLESS_PEAK - 2) continue;
            int trunk = 4 + static_cast<int>(hash01(wx, wz, 2, seed_) * 3);
            int crown = h + trunk;
            for (int dy = -2; dy < 2; ++dy) {
                int r = dy < 0 ? 2 : 1;
                for (int dz = -r; dz <= r; ++dz)
                    for (int dx = -r; dx <= r; ++dx) {
                        if (r == 2 && std::abs(dx) == 2 && std::abs(dz) == 2 &&
                            hash01(wx + dx, wz + dz, 3 + dy, seed_) < 0.6)
                            continue;
                        if (dy == 1 && std::abs(dx) == 1 && std::abs(dz) == 1) continue;
                        place(*chunk, wx + dx, crown + dy, wz + dz, LEAVES, true);
                    }
            }
            for (int y = h + 1; y < crown; ++y) place(*chunk, wx, y, wz, LOG);
            place(*chunk, wx, h, wz, DIRT);
            top = std::max(top, crown + 2);
        }
    chunk->max_y = std::min(HEIGHT - 1, top + 1);
    return chunk;
}

// --- World storage ------------------------------------------------------------------

World::World(uint64_t seed) : generator_(seed) {}

Chunk* World::find(int cx, int cz) {
    if (last_ && last_->cx == cx && last_->cz == cz) return last_;
    auto it = chunks_.find({cx, cz});
    if (it == chunks_.end()) return nullptr;
    last_ = it->second.get();
    return last_;
}

Chunk& World::chunk(int cx, int cz) {
    if (Chunk* c = find(cx, cz)) return *c;
    insert(load_or_generate(cx, cz));
    return *find(cx, cz);
}

std::unique_ptr<Chunk> World::load_or_generate(int cx, int cz) const {
    if (store_)
        if (auto c = store_->load(cx, cz)) return c;
    return generator_.generate(cx, cz);
}

std::vector<Chunk*> World::all_chunks() {
    std::vector<Chunk*> out;
    out.reserve(chunks_.size());
    for (auto& [pos, c] : chunks_) out.push_back(c.get());
    return out;
}

void Chunk::recompute_max_y() {
    max_y = 0;
    for (int y = HEIGHT - 1; y >= 0 && !max_y; --y)
        for (int i = 0; i < CHUNK * CHUNK; ++i)
            if (blocks[(y << 8) | i] != AIR) {
                max_y = std::min(HEIGHT - 1, y + 1);
                break;
            }
}

void World::insert(std::unique_ptr<Chunk> chunk) {
    ChunkPos key{chunk->cx, chunk->cz};
    chunks_.try_emplace(key, std::move(chunk));
}

BlockId World::get_block(int x, int y, int z) {
    if (y < 0) return BEDROCK;
    if (y >= HEIGHT) return AIR;
    Chunk* c = find(chunk_of(x), chunk_of(z));
    if (!c) return AIR;
    return c->blocks[Chunk::index(x & 15, y, z & 15)];
}

std::vector<ChunkPos> World::set_block(int x, int y, int z, BlockId block) {
    if (y < 0 || y >= HEIGHT) return {};
    int cx = chunk_of(x), cz = chunk_of(z);
    Chunk& c = chunk(cx, cz);
    int lx = x & 15, lz = z & 15;
    c.blocks[Chunk::index(lx, y, lz)] = block;
    c.dirty = true;
    if (block != AIR) c.max_y = std::max(c.max_y, std::min(HEIGHT - 1, y + 1));
    // Faces and ambient occlusion reach one block into neighbouring chunks.
    std::vector<int> xs{cx}, zs{cz};
    if (lx == 0) xs.push_back(cx - 1);
    if (lx == 15) xs.push_back(cx + 1);
    if (lz == 0) zs.push_back(cz - 1);
    if (lz == 15) zs.push_back(cz + 1);
    std::vector<ChunkPos> dirty;
    for (int a : xs)
        for (int b : zs)
            if (find(a, b)) dirty.push_back({a, b});
    return dirty;
}

int World::surface_y(int x, int z) {
    chunk(chunk_of(x), chunk_of(z));
    for (int y = HEIGHT - 1; y >= 0; --y)
        if (is_solid_at(x, y, z)) return y + 1;
    return 1;
}

bool World::box_collides(double x0, double y0, double z0, double x1, double y1, double z1) {
    for (int bx = ifloor(x0); bx <= ifloor(x1); ++bx)
        for (int by = ifloor(y0); by <= ifloor(y1); ++by)
            for (int bz = ifloor(z0); bz <= ifloor(z1); ++bz)
                if (is_solid(get_block(bx, by, bz))) return true;
    return false;
}

bool World::box_in_water(double x0, double y0, double z0, double x1, double y1, double z1) {
    for (int bx = ifloor(x0); bx <= ifloor(x1); ++bx)
        for (int by = ifloor(y0); by <= ifloor(y1); ++by)
            for (int bz = ifloor(z0); bz <= ifloor(z1); ++bz)
                if (get_block(bx, by, bz) == WATER) return true;
    return false;
}

std::optional<RayHit> World::raycast(glm::vec3 origin, glm::vec3 direction, float max_dist) {
    const float inf = std::numeric_limits<float>::infinity();
    glm::ivec3 pos(ifloor(origin.x), ifloor(origin.y), ifloor(origin.z));
    glm::ivec3 step;
    glm::vec3 t_max, t_delta;
    for (int a = 0; a < 3; ++a) {
        float d = direction[a];
        step[a] = d > 0 ? 1 : -1;
        if (std::abs(d) < 1e-9f) {
            t_max[a] = inf;
            t_delta[a] = inf;
        } else {
            float bound = static_cast<float>(d > 0 ? pos[a] + 1 : pos[a]);
            t_max[a] = (bound - origin[a]) / d;
            t_delta[a] = std::abs(1.0f / d);
        }
    }
    std::optional<glm::ivec3> prev;
    float t = 0.0f;
    while (t <= max_dist) {
        BlockId blk = get_block(pos.x, pos.y, pos.z);
        if (blk != AIR && blk != WATER) return RayHit{pos, prev, t};
        prev = pos;
        int axis = 0;
        if (t_max[1] < t_max[axis]) axis = 1;
        if (t_max[2] < t_max[axis]) axis = 2;
        t = t_max[axis];
        pos[axis] += step[axis];
        t_max[axis] += t_delta[axis];
    }
    return std::nullopt;
}

// --- Meshing ----------------------------------------------------------------------------

Padded World::padded(int cx, int cz) {
    Padded P(PADDED_SIZE, AIR);
    std::fill(P.begin(), P.begin() + LAYER, static_cast<BlockId>(BEDROCK));  // y = -1
    const auto& b = chunk(cx, cz).blocks;
    const auto& nxm = chunk(cx - 1, cz).blocks;
    const auto& nxp = chunk(cx + 1, cz).blocks;
    const auto& nzm = chunk(cx, cz - 1).blocks;
    const auto& nzp = chunk(cx, cz + 1).blocks;
    const auto& dmm = chunk(cx - 1, cz - 1).blocks;
    const auto& dpm = chunk(cx + 1, cz - 1).blocks;
    const auto& dmp = chunk(cx - 1, cz + 1).blocks;
    const auto& dpp = chunk(cx + 1, cz + 1).blocks;
    for (int y = 0; y < HEIGHT; ++y) {
        int py = (y + 1) * LAYER;
        int sy = y << 8;
        for (int z = 0; z < CHUNK; ++z) {
            int dst = py + (z + 1) * PW;
            int src = sy | (z << 4);
            std::copy_n(b.begin() + src, 16, P.begin() + dst + 1);
            P[dst] = nxm[src | 15];
            P[dst + 17] = nxp[src];
        }
        std::copy_n(nzm.begin() + (sy | 240), 16, P.begin() + py + 1);
        int row = py + 17 * PW;
        std::copy_n(nzp.begin() + sy, 16, P.begin() + row + 1);
        P[py] = dmm[sy | 255];
        P[py + 17] = dpm[sy | 240];
        P[row] = dmp[sy | 15];
        P[row + 17] = dpp[sy];
    }
    return P;
}

ChunkMesh World::build_mesh_now(int cx, int cz, MeshOptions options) {
    Padded P = padded(cx, cz);
    return build_mesh(P, cx, cz, chunk(cx, cz).max_y, options);
}

ChunkMesh build_mesh(const Padded& P, int cx, int cz, int max_y, MeshOptions options) {
    ChunkMesh mesh;
    const int ox = cx * CHUNK, oz = cz * CHUNK;
    const int up = off(0, 1, 0);
    const int top = std::min(HEIGHT, max_y + 1);
    for (int y = 0; y < top; ++y)
        for (int z = 0; z < CHUNK; ++z) {
            int base = (y + 1) * LAYER + (z + 1) * PW + 1;
            for (int x = 0; x < CHUNK; ++x) {
                int i = base + x;
                BlockId blk = P[i];
                if (blk == AIR) continue;
                float wx = static_cast<float>(ox + x), wy = static_cast<float>(y), wz = static_cast<float>(oz + z);
                if (blk == WATER) {
                    float h = P[i + up] == WATER ? 1.0f : WATER_SURFACE;
                    const auto& uv = BLOCK_UVS[WATER].uv[0];
                    for (const Face& f : FACES) {
                        if (f.offset == -up || P[i + f.offset] != AIR) continue;
                        float l = f.def->light;
                        mesh.water.quad(wx, wy, wz, f.def->corners, h, uv, {l, l, l, l});
                    }
                    continue;
                }
                const auto& uvs = (blk == LEAVES && !options.fancy_leaves) ? FAST_LEAVES_UVS.uv : BLOCK_UVS[blk].uv;
                for (const Face& f : FACES) {
                    BlockId nb = P[i + f.offset];
                    if (OPAQUE[nb] && !(nb == LEAVES && options.fancy_leaves)) continue;
                    std::array<float, 4> shades;
                    for (int c = 0; c < 4; ++c) {
                        int occ = 0;
                        if (options.smooth_lighting) {
                            bool a = OPAQUE[P[i + f.ao[c][0]]], b = OPAQUE[P[i + f.ao[c][1]]];
                            occ = (a && b) ? 3 : a + b + OPAQUE[P[i + f.ao[c][2]]];
                        }
                        shades[c] = f.def->light * AO_LEVELS[occ];
                    }
                    mesh.solid.quad(wx, wy, wz, f.def->corners, 1.0f, uvs[f.def->slot], shades);
                }
            }
        }
    return mesh;
}

void MeshData::quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
                    const std::array<float, 4>& uv, const std::array<float, 4>& shades) {
    const float us[4] = {uv[0], uv[2], uv[2], uv[0]};
    const float vs[4] = {uv[1], uv[1], uv[3], uv[3]};
    uint32_t n = static_cast<uint32_t>(vertices.size());
    for (int c = 0; c < 4; ++c)
        vertices.push_back({x + corners[c].x, y + corners[c].y * h, z + corners[c].z, shades[c], us[c], vs[c]});
    // Flip the quad's diagonal so occlusion gradients interpolate smoothly.
    if (shades[0] + shades[2] < shades[1] + shades[3])
        indices.insert(indices.end(), {n + 1, n + 2, n + 3, n + 1, n + 3, n});
    else
        indices.insert(indices.end(), {n, n + 1, n + 2, n, n + 2, n + 3});
}

void MeshData::quad_points(const std::array<glm::vec3, 4>& pts, const std::array<float, 4>& uv, float shade) {
    const float us[4] = {uv[0], uv[2], uv[2], uv[0]};
    const float vs[4] = {uv[1], uv[1], uv[3], uv[3]};
    uint32_t n = static_cast<uint32_t>(vertices.size());
    for (int c = 0; c < 4; ++c) vertices.push_back({pts[c].x, pts[c].y, pts[c].z, shade, us[c], vs[c]});
    indices.insert(indices.end(), {n, n + 1, n + 2, n, n + 2, n + 3});
}
