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
BlockUvs uniform_uvs(int tile) {
    BlockUvs out{};
    for (auto& uv : out.uv) uv = tile_uv(tile);
    return out;
}
const BlockUvs FAST_LEAVES_UVS = uniform_uvs(T_LEAVES_FAST);
const BlockUvs FAST_SPRUCE_LEAVES_UVS = uniform_uvs(T_SPRUCE_LEAVES_FAST);

const std::array<bool, 256> LEAVES_TABLE = [] {
    std::array<bool, 256> o{};
    for (int b = 0; b < 256; ++b) o[b] = is_leaves(static_cast<BlockId>(b));
    return o;
}();

// Liquids and ice: the top is lowered when nothing of the same kind sits on it.
constexpr float LIQUID_SURFACE = 0.875f;

const std::array<bool, 256> OPAQUE = [] {
    std::array<bool, 256> o{};
    for (int b = 0; b < 256; ++b) o[b] = is_opaque(static_cast<BlockId>(b));
    return o;
}();

int ifloor(double v) { return static_cast<int>(std::floor(v)); }

}  // namespace

const std::array<FaceDef, 6>& face_defs() { return FACE_DEFS; }

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
        if (auto c = store_->load(cx, cz)) {
            c->climate = generator_.climate(cx, cz);
            return c;
        }
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
        if (blk != AIR && !is_fluid(blk)) return RayHit{pos, prev, t};
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
    const Chunk& c = chunk(cx, cz);
    return build_mesh(P, cx, cz, c.max_y, options, &c.climate);
}

ChunkMesh build_mesh(const Padded& P, int cx, int cz, int max_y, MeshOptions options, const Climate* climate) {
    ChunkMesh mesh;
    const int ox = cx * CHUNK, oz = cz * CHUNK;
    const int up = off(0, 1, 0);
    const int top = std::min(HEIGHT, max_y + 1);
    const Rgb8 white{255, 255, 255};
    // Grass and leaf colours per column.
    std::array<Rgb8, CHUNK * CHUNK> grass, foliage;
    for (int i = 0; i < CHUNK * CHUNK; ++i) {
        double t = climate ? climate->temp(i) : ICON_TEMPERATURE, r = climate ? climate->rain(i) : ICON_RAINFALL;
        grass[i] = grass_color(t, r);
        foliage[i] = foliage_color(t, r);
    }
    for (int y = 0; y < top; ++y)
        for (int z = 0; z < CHUNK; ++z) {
            int base = (y + 1) * LAYER + (z + 1) * PW + 1;
            for (int x = 0; x < CHUNK; ++x) {
                int i = base + x;
                BlockId blk = P[i];
                if (blk == AIR) continue;
                float wx = static_cast<float>(ox + x), wy = static_cast<float>(y), wz = static_cast<float>(oz + z);
                if (blk == WATER) {
                    float h = P[i + up] == WATER ? 1.0f : LIQUID_SURFACE;
                    const auto& uv = BLOCK_UVS[WATER].uv[0];
                    for (const Face& f : FACES) {
                        if (f.offset == -up || P[i + f.offset] != AIR) continue;
                        float l = f.def->light;
                        mesh.water.quad(wx, wy, wz, f.def->corners, h, uv, {l, l, l, l});
                    }
                    continue;
                }
                if (blk == LAVA || blk == ICE) {
                    // Lava is drawn solid, ice see-through with the water.
                    float h = (blk == ICE || P[i + up] == blk) ? 1.0f : LIQUID_SURFACE;
                    MeshData& out = blk == LAVA ? mesh.solid : mesh.water;
                    for (const Face& f : FACES) {
                        BlockId nb = P[i + f.offset];
                        if (OPAQUE[nb] || nb == blk || (blk == ICE && nb == WATER)) continue;
                        float l = f.def->light;
                        out.quad(wx, wy, wz, f.def->corners, h, BLOCK_UVS[blk].uv[f.def->slot], {l, l, l, l});
                    }
                    continue;
                }
                bool leaves = LEAVES_TABLE[blk];
                const auto& uvs = (leaves && !options.fancy_leaves)
                                      ? (blk == SPRUCE_LEAVES ? FAST_SPRUCE_LEAVES_UVS.uv : FAST_LEAVES_UVS.uv)
                                      : BLOCK_UVS[blk].uv;
                Rgb8 tint = white;
                switch (block_tint(blk)) {
                    case BlockTint::GRASS: tint = grass[(z << 4) | x]; break;
                    case BlockTint::FOLIAGE: tint = foliage[(z << 4) | x]; break;
                    case BlockTint::BIRCH: tint = BIRCH_COLOR; break;
                    case BlockTint::SPRUCE: tint = SPRUCE_COLOR; break;
                    case BlockTint::NONE: break;
                }
                for (const Face& f : FACES) {
                    BlockId nb = P[i + f.offset];
                    if (OPAQUE[nb] && !(LEAVES_TABLE[nb] && options.fancy_leaves)) continue;
                    if (nb == blk && is_cutout(blk)) continue;
                    std::array<float, 4> shades;
                    for (int c = 0; c < 4; ++c) {
                        int occ = 0;
                        if (options.smooth_lighting) {
                            bool a = OPAQUE[P[i + f.ao[c][0]]], b = OPAQUE[P[i + f.ao[c][1]]];
                            occ = (a && b) ? 3 : a + b + OPAQUE[P[i + f.ao[c][2]]];
                        }
                        shades[c] = f.def->light * AO_LEVELS[occ];
                    }
                    mesh.solid.quad(wx, wy, wz, f.def->corners, 1.0f, uvs[f.def->slot], shades, tint);
                }
            }
        }
    return mesh;
}

void MeshData::quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
                    const std::array<float, 4>& uv, const std::array<float, 4>& shades, Rgb8 tint) {
    const float us[4] = {uv[0], uv[2], uv[2], uv[0]};
    const float vs[4] = {uv[1], uv[1], uv[3], uv[3]};
    const float r = tint.r / 255.0f, g = tint.g / 255.0f, b = tint.b / 255.0f;
    uint32_t n = static_cast<uint32_t>(vertices.size());
    for (int c = 0; c < 4; ++c)
        vertices.push_back({x + corners[c].x, y + corners[c].y * h, z + corners[c].z, shades[c], us[c], vs[c], r, g, b});
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
