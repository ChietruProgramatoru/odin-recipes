#include "world.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/vec2.hpp>

#include "bed.h"
#include "icons.h"
#include "rails.h"
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
// The front of a furnace, on the side it faces.
const std::array<float, 4> FURNACE_FRONT_UV = tile_uv(IT_FURNACE_FRONT), LIT_FURNACE_FRONT_UV = tile_uv(IT_FURNACE_LIT_FRONT);

const std::array<bool, 256> LEAVES_TABLE = [] {
    std::array<bool, 256> o{};
    for (int b = 0; b < 256; ++b) o[b] = is_leaves(static_cast<BlockId>(b));
    return o;
}();

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
    std::unique_ptr<Chunk> c;
    if (store_) c = store_->load(cx, cz);
    if (c) c->climate = generator_.climate(cx, cz);
    else c = generator_.generate(cx, cz);
    light_chunk(*c);
    return c;
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
    if (chunks_.count(key)) return;
    if (!chunk->lit) light_chunk(*chunk);
    Chunk& c = *chunks_.emplace(key, std::move(chunk)).first->second;
    join_light(c);
}

BlockId World::get_block(int x, int y, int z) {
    if (y < 0) return BEDROCK;
    if (y >= HEIGHT) return AIR;
    Chunk* c = find(chunk_of(x), chunk_of(z));
    if (!c) return AIR;
    return c->blocks[Chunk::index(x & 15, y, z & 15)];
}

Furnace* World::furnace_at(glm::ivec3 p) {
    if (p.y < 0 || p.y >= HEIGHT) return nullptr;
    Chunk* c = find(chunk_of(p.x), chunk_of(p.z));
    if (!c) return nullptr;
    auto it = c->furnaces.find(Chunk::index(p.x & 15, p.y, p.z & 15));
    return it == c->furnaces.end() ? nullptr : &it->second;
}

Furnace& World::furnace(glm::ivec3 p) {
    Chunk& c = chunk(chunk_of(p.x), chunk_of(p.z));
    c.dirty = true;
    return c.furnaces[Chunk::index(p.x & 15, std::clamp(p.y, 0, HEIGHT - 1), p.z & 15)];
}

std::optional<Furnace> World::take_furnace(glm::ivec3 p) {
    if (p.y < 0 || p.y >= HEIGHT) return std::nullopt;
    Chunk* c = find(chunk_of(p.x), chunk_of(p.z));
    if (!c) return std::nullopt;
    auto it = c->furnaces.find(Chunk::index(p.x & 15, p.y, p.z & 15));
    if (it == c->furnaces.end()) return std::nullopt;
    Furnace f = it->second;
    c->furnaces.erase(it);
    c->dirty = true;
    return f;
}

int World::get_data(int x, int y, int z) {
    if (y < 0 || y >= HEIGHT) return 0;
    Chunk* c = find(chunk_of(x), chunk_of(z));
    if (!c) return 0;
    return c->data[Chunk::index(x & 15, y, z & 15)];
}

std::vector<glm::ivec3> World::take_changes() {
    std::vector<glm::ivec3> out;
    out.swap(changes_);
    return out;
}

std::vector<ChunkPos> World::set_data(int x, int y, int z, int data, bool notify) {
    if (y < 0 || y >= HEIGHT) return {};
    Chunk& c = chunk(chunk_of(x), chunk_of(z));
    c.data[Chunk::index(x & 15, y, z & 15)] = static_cast<uint8_t>(data & 15);
    c.dirty = true;
    if (!notify) return {};
    changes_.push_back({x, y, z});
    auto dirty = neighbours_to_rebuild(x, y, z);
    std::erase_if(dirty, [&](ChunkPos p) { return !find(p.cx, p.cz); });
    return dirty;
}

std::vector<ChunkPos> World::neighbours_to_rebuild(int x, int y, int z) {
    // Faces and ambient occlusion reach one block into neighbouring chunks.
    const int cx = chunk_of(x), cz = chunk_of(z), lx = x & 15, lz = z & 15;
    std::vector<int> xs{cx}, zs{cz};
    if (lx == 0) xs.push_back(cx - 1);
    if (lx == 15) xs.push_back(cx + 1);
    if (lz == 0) zs.push_back(cz - 1);
    if (lz == 15) zs.push_back(cz + 1);
    std::vector<ChunkPos> dirty;
    for (int a : xs)
        for (int b : zs) dirty.push_back({a, b});
    return dirty;
}

std::vector<ChunkPos> World::set_block(int x, int y, int z, BlockId block, int data) {
    if (y < 0 || y >= HEIGHT) return {};
    int cx = chunk_of(x), cz = chunk_of(z);
    Chunk& c = chunk(cx, cz);
    int lx = x & 15, lz = z & 15;
    c.blocks[Chunk::index(lx, y, lz)] = block;
    c.data[Chunk::index(lx, y, lz)] = static_cast<uint8_t>(data & 15);
    c.dirty = true;
    changes_.push_back({x, y, z});
    if (block != AIR) c.max_y = std::max(c.max_y, std::min(HEIGHT - 1, y + 1));
    std::vector<ChunkPos> dirty = neighbours_to_rebuild(x, y, z);
    relight(x, y, z, dirty);
    std::erase_if(dirty, [&](ChunkPos p) { return !find(p.cx, p.cz); });
    return dirty;
}

std::optional<int> World::rain_height(int x, int z) {
    Chunk* c = find(chunk_of(x), chunk_of(z));
    if (!c) return std::nullopt;
    int lx = x & 15, lz = z & 15;
    for (int y = std::min(c->max_y, HEIGHT - 1); y >= 0; --y) {
        BlockId b = c->blocks[Chunk::index(lx, y, lz)];
        if (is_solid(b) || is_fluid(b)) return y + 1;
    }
    return 0;
}

std::optional<Biome> World::biome_at(int x, int z) {
    Chunk* c = find(chunk_of(x), chunk_of(z));
    if (!c) return std::nullopt;
    return c->climate.biome(((z & 15) << 4) | (x & 15));
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
                if (BlockId b = get_block(bx, by, bz); is_solid(b) && y0 < by + block_height(b)) return true;
    return false;
}

double World::box_top(double x0, double y0, double z0, double x1, double y1, double z1) {
    double top = y0;
    for (int bx = ifloor(x0); bx <= ifloor(x1); ++bx)
        for (int by = ifloor(y0); by <= ifloor(y1); ++by)
            for (int bz = ifloor(z0); bz <= ifloor(z1); ++bz)
                if (BlockId b = get_block(bx, by, bz); is_solid(b) && y0 < by + block_height(b))
                    top = std::max(top, by + static_cast<double>(block_height(b)));
    return top;
}

bool World::box_in(BlockId block, double x0, double y0, double z0, double x1, double y1, double z1) {
    for (int bx = ifloor(x0); bx <= ifloor(x1); ++bx)
        for (int by = ifloor(y0); by <= ifloor(y1); ++by)
            for (int bz = ifloor(z0); bz <= ifloor(z1); ++bz)
                if (get_block(bx, by, bz) == block) return true;
    return false;
}

std::optional<RayHit> World::raycast(glm::vec3 origin, glm::vec3 direction, float max_dist, bool (*stops)(BlockId)) {
    if (stops) return raycast(origin, direction, max_dist, [stops](glm::ivec3, BlockId b) { return stops(b); });
    return raycast(origin, direction, max_dist, [](glm::ivec3, BlockId b) { return b != AIR && !is_fluid(b); });
}

std::optional<RayHit> World::raycast(glm::vec3 origin, glm::vec3 direction, float max_dist,
                                     const std::function<bool(glm::ivec3, BlockId)>& stops) {
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
        if (stops(pos, blk)) return RayHit{pos, prev, t};
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

namespace {

// Copy one per-block array of a chunk plus a one-block border from its 8 neighbours.
template <typename T, typename Field>
std::vector<T> pad(World& w, int cx, int cz, T below, T above, Field field) {
    std::vector<T> P(PADDED_SIZE, T{});
    std::fill(P.begin(), P.begin() + LAYER, below);  // y = -1
    std::fill(P.end() - LAYER, P.end(), above);      // y = HEIGHT
    const auto& b = field(w.chunk(cx, cz));
    const auto& nxm = field(w.chunk(cx - 1, cz));
    const auto& nxp = field(w.chunk(cx + 1, cz));
    const auto& nzm = field(w.chunk(cx, cz - 1));
    const auto& nzp = field(w.chunk(cx, cz + 1));
    const auto& dmm = field(w.chunk(cx - 1, cz - 1));
    const auto& dpm = field(w.chunk(cx + 1, cz - 1));
    const auto& dmp = field(w.chunk(cx - 1, cz + 1));
    const auto& dpp = field(w.chunk(cx + 1, cz + 1));
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

}  // namespace

Padded World::padded(int cx, int cz) {
    return pad<BlockId>(*this, cx, cz, BEDROCK, AIR, [](Chunk& c) -> const auto& { return c.blocks; });
}

Padded World::padded_data(int cx, int cz) {
    return pad<BlockId>(*this, cx, cz, 0, 0, [](Chunk& c) -> const auto& { return c.data; });
}

PaddedLight World::padded_light(int cx, int cz) {
    // Dark below the world, open sky above it.
    return pad<uint8_t>(*this, cx, cz, 0, MAX_LIGHT << 4, [](Chunk& c) -> const auto& { return c.light; });
}

ChunkMesh World::build_mesh_now(int cx, int cz, MeshOptions options) {
    Padded P = padded(cx, cz);
    PaddedLight L = padded_light(cx, cz);
    Padded D = padded_data(cx, cz);
    const Chunk& c = chunk(cx, cz);
    return build_mesh(P, L, cx, cz, c.max_y, options, &c.climate, &D);
}

namespace {

float sky_of(uint8_t l) { return static_cast<float>(l >> 4) / MAX_LIGHT; }
float block_of(uint8_t l) { return static_cast<float>(l & 15) / MAX_LIGHT; }

// A torch: a thin stick 2/16 wide and 10/16 tall. Wall torches lean out from their wall.
void add_torch(MeshData& mesh, float wx, float wy, float wz, BlockId blk, uint8_t light) {
    constexpr float W = 2.0f / 16, H = 10.0f / 16;
    glm::vec3 lean(0);
    if (blk == TORCH_WALL_WEST) lean = {1, 0, 0};
    if (blk == TORCH_WALL_EAST) lean = {-1, 0, 0};
    if (blk == TORCH_WALL_NORTH) lean = {0, 0, 1};
    if (blk == TORCH_WALL_SOUTH) lean = {0, 0, -1};
    const bool wall = blk != TORCH;
    auto uv = tile_uv(T_TORCH);
    auto sub = [&](float px0, float py0, float px1, float py1) {  // pixel rectangle of the tile, rows from the top
        float du = (uv[2] - uv[0]) / 16, dv = (uv[3] - uv[1]) / 16;
        return std::array<float, 4>{uv[0] + px0 * du, uv[3] - py1 * dv, uv[0] + px1 * du, uv[3] - py0 * dv};
    };
    const auto side_uv = sub(7, 6, 9, 16), cap_uv = sub(7, 6, 9, 8);
    for (const FaceDef& f : face_defs()) {
        std::array<glm::vec3, 4> pts;
        for (int c = 0; c < 4; ++c) {
            glm::vec3 k(f.corners[c]);
            glm::vec3 p(0.5f + (k.x - 0.5f) * W, k.y * H, 0.5f + (k.z - 0.5f) * W);
            if (wall) p += lean * (p.y * 0.4f - 0.35f) + glm::vec3(0, 0.2f, 0);
            pts[c] = p + glm::vec3(wx, wy, wz);
        }
        mesh.quad_points(pts, f.slot == 1 ? side_uv : cap_uv, 1.0f, sky_of(light), block_of(light));
    }
}

// Fire: two crossed sheets of flame, drawn from both sides so it shows from every angle.
void add_fire(MeshData& mesh, float wx, float wy, float wz, uint8_t light) {
    const auto uv = tile_uv(T_FIRE);
    const glm::vec3 o(wx, wy, wz);
    const glm::vec3 sheets[2][2] = {{{0.05f, 0, 0.05f}, {0.95f, 0, 0.95f}}, {{0.05f, 0, 0.95f}, {0.95f, 0, 0.05f}}};
    for (const auto& s : sheets) {
        glm::vec3 a = o + s[0], b = o + s[1], up(0, 1, 0);
        // Corners in the order quad_points wants (bottom left, bottom right, top right, top left), then reversed.
        mesh.quad_points({a, b, b + up, a + up}, uv, 1.0f, sky_of(light), block_of(light));
        mesh.quad_points({b, a, a + up, b + up}, uv, 1.0f, sky_of(light), block_of(light));
    }
}

// Part of a tile: a pixel rectangle, rows counted from the top.
std::array<float, 4> sub_uv(int tile, float px0, float py0, float px1, float py1) {
    auto uv = tile_uv(tile);
    float du = (uv[2] - uv[0]) / 16, dv = (uv[3] - uv[1]) / 16;
    return {uv[0] + px0 * du, uv[3] - py1 * dv, uv[0] + px1 * du, uv[3] - py0 * dv};
}

// One half of a bed: a slab from 3/16 to 9/16 up with legs at the outer corners (the legs are
// drawn by the see-through bottom rows of the side tiles). Built pointing south (head towards
// +z), then turned to the bed's facing.
void add_bed(MeshData& mesh, float wx, float wy, float wz, BlockId blk, uint8_t light) {
    constexpr float H = 9.0f / 16, B = 3.0f / 16;
    const bool head = bed_is_head(blk);
    const glm::ivec3 f = FACING_DIRS[bed_dir(blk)];
    const glm::vec3 fwd(f), right(f.z, 0, -f.x);
    auto place = [&](glm::vec3 p) {
        glm::vec3 c = p - glm::vec3(0.5f, 0, 0.5f);
        return glm::vec3(wx + 0.5f, wy + p.y, wz + 0.5f) + right * c.x + fwd * c.z;
    };
    auto face = [&](std::array<glm::vec3, 4> pts, glm::vec3 normal, std::array<float, 4> uv) {
        for (auto& p : pts) p = place(p);
        glm::vec3 n = right * normal.x + fwd * normal.z + glm::vec3(0, normal.y, 0);
        float shade = n.y > 0 ? 1.0f : n.y < 0 ? 0.5f : std::abs(n.x) > 0.5f ? 0.8f : 0.68f;
        mesh.quad_points(pts, uv, shade, sky_of(light), block_of(light));
    };
    const int side = head ? T_BED_SIDE_HEAD : T_BED_SIDE_FOOT;
    auto side_uv = sub_uv(side, 0, 7, 16, 16);
    auto flipped = std::array<float, 4>{side_uv[2], side_uv[1], side_uv[0], side_uv[3]};
    face({{{0, H, 0}, {1, H, 0}, {1, H, 1}, {0, H, 1}}}, {0, 1, 0}, tile_uv(head ? T_BED_TOP_HEAD : T_BED_TOP_FOOT));
    face({{{0, B, 1}, {1, B, 1}, {1, B, 0}, {0, B, 0}}}, {0, -1, 0}, tile_uv(T_PLANKS));
    face({{{1, 0, 0}, {1, 0, 1}, {1, H, 1}, {1, H, 0}}}, {1, 0, 0}, side_uv);
    face({{{0, 0, 1}, {0, 0, 0}, {0, H, 0}, {0, H, 1}}}, {-1, 0, 0}, flipped);
    if (head)
        face({{{1, 0, 1}, {0, 0, 1}, {0, H, 1}, {1, H, 1}}}, {0, 0, 1}, sub_uv(T_BED_END_HEAD, 0, 7, 16, 16));
    else
        face({{{0, 0, 0}, {1, 0, 0}, {1, H, 0}, {0, H, 0}}}, {0, 0, -1}, sub_uv(T_BED_END_FOOT, 0, 7, 16, 16));
}

// A box inside a block, from `lo` to `hi` (0..1), every face drawn with the part of `tiles[slot]`
// (top, side, bottom) the box covers; `cut` replaces the -x side's tile (a cake with slices gone).
void add_box(MeshData& mesh, glm::vec3 o, glm::vec3 lo, glm::vec3 hi, const int tiles[3], int cut, uint8_t light) {
    for (const FaceDef& f : face_defs()) {
        std::array<glm::vec3, 4> pts;
        for (int c = 0; c < 4; ++c) pts[c] = o + lo + glm::vec3(f.corners[c]) * (hi - lo);
        const glm::ivec3 n = f.normal;
        float u0, u1, v0, v1;  // the box's extent across the face, in pixels
        if (n.y != 0) {
            u0 = lo.x, u1 = hi.x, v0 = lo.z, v1 = hi.z;
        } else if (n.x != 0) {
            u0 = lo.z, u1 = hi.z, v0 = 1 - hi.y, v1 = 1 - lo.y;
        } else {
            u0 = lo.x, u1 = hi.x, v0 = 1 - hi.y, v1 = 1 - lo.y;
        }
        const int tile = (n.x < 0 && cut >= 0) ? cut : tiles[f.slot];
        mesh.quad_points(pts, sub_uv(tile, u0 * 16, v0 * 16, u1 * 16, v1 * 16), f.light, sky_of(light), block_of(light));
    }
}

// Cake: 14/16 wide and half a block tall, losing 2/16 from its -x side for every slice eaten.
void add_cake(MeshData& mesh, float wx, float wy, float wz, int eaten, uint8_t light) {
    const BlockInfo* info = block_info(CAKE_BLOCK);
    const float x0 = (1.0f + 2.0f * eaten) / 16;
    add_box(mesh, {wx, wy, wz}, {x0, 0, 1.0f / 16}, {15.0f / 16, 0.5f, 15.0f / 16}, info->tiles, eaten ? T_CAKE_INSIDE : -1,
            light);
}

// Plants: two sheets crossing corner to corner, or wheat's four sheets in a # shape. Drawn from both
// sides. Tall grass and ferns take the biome's grass colour.
void add_plant(MeshData& mesh, float wx, float wy, float wz, BlockId blk, int data, uint8_t light, Rgb8 tint) {
    const int tile = blk == WHEAT_CROP ? T_WHEAT_0 + std::min(data, 7) : block_info(blk)->tiles[1];
    const auto uv = tile_uv(tile);
    const glm::vec3 o(wx, wy, wz), up(0, 1, 0);
    auto sheet = [&](glm::vec3 a, glm::vec3 b) {
        a += o;
        b += o;
        mesh.quad_points({a, b, b + up, a + up}, uv, 1.0f, sky_of(light), block_of(light), tint);
        mesh.quad_points({b, a, a + up, b + up}, uv, 1.0f, sky_of(light), block_of(light), tint);
    };
    if (blk == WHEAT_CROP) {
        for (float k : {0.25f, 0.75f}) {
            sheet({k, 0, 0}, {k, 0, 1});
            sheet({0, 0, k}, {1, 0, k});
        }
        return;
    }
    sheet({0.15f, 0, 0.15f}, {0.85f, 0, 0.85f});
    sheet({0.15f, 0, 0.85f}, {0.85f, 0, 0.15f});
}

// Rails: one sheet just above the block below, drawn from both sides. The tiles run north-south
// (bottom of the picture to the south); curves join the bottom and right of the picture (south and
// east) and are turned for the other corners. Slopes are tilted up to the next block.
void add_rail(MeshData& mesh, float wx, float wy, float wz, BlockId blk, int data, uint8_t light) {
    const int shape = rail_shape(blk, data);
    const bool on = rail_powered(blk, data);
    const int tile = blk == POWERED_RAIL ? (on ? T_POWERED_RAIL_ON : T_POWERED_RAIL)
                     : blk == DETECTOR_RAIL ? T_DETECTOR_RAIL
                     : shape >= RAIL_SOUTH_EAST ? T_RAIL_CURVE : T_RAIL;
    // Quarter turns from the picture's own direction.
    int turns = 0;
    switch (shape) {
        case RAIL_EAST_WEST: case RAIL_ASCENDING_EAST: case RAIL_ASCENDING_WEST: turns = 1; break;
        case RAIL_SOUTH_WEST: turns = 1; break;
        case RAIL_NORTH_WEST: turns = 2; break;
        case RAIL_NORTH_EAST: turns = 3; break;
        default: break;
    }
    // Corners BL, BR, TR, TL of the picture on the block's top, from above: bottom edge to the south.
    std::array<glm::vec2, 4> c = {glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0), glm::vec2(0, 0)};
    for (int t = 0; t < turns; ++t)
        for (glm::vec2& k : c) k = glm::vec2(1 - k.y, k.x);  // a quarter turn, east to south
    std::array<glm::vec3, 4> pts;
    for (int k = 0; k < 4; ++k) {
        float h = 1.0f / 16;
        switch (shape) {
            case RAIL_ASCENDING_EAST: h += c[k].x; break;
            case RAIL_ASCENDING_WEST: h += 1 - c[k].x; break;
            case RAIL_ASCENDING_NORTH: h += 1 - c[k].y; break;
            case RAIL_ASCENDING_SOUTH: h += c[k].y; break;
            default: break;
        }
        pts[k] = glm::vec3(wx + c[k].x, wy + h, wz + c[k].y);
    }
    const auto uv = tile_uv(tile);
    mesh.quad_points(pts, uv, 1.0f, sky_of(light), block_of(light));
    mesh.quad_points({pts[1], pts[0], pts[3], pts[2]}, {uv[2], uv[1], uv[0], uv[3]}, 0.6f, sky_of(light), block_of(light));
}

// Water or lava: the top slopes between the heights at its four corners, which come from how far
// the fluid around each corner has flowed (Beta's RenderBlocks.getFluidHeight). Sides are drawn
// up to the corner heights where the next cell isn't the same fluid.
void add_fluid(MeshData& mesh, const Padded& P, const Padded* D, const PaddedLight& L, int i, BlockId blk, float wx,
               float wy, float wz) {
    const int up = off(0, 1, 0);
    auto level = [&](int j) { return D ? static_cast<int>((*D)[j]) : 0; };
    // Height of the corner between cells (x-1..x, z-1..z) of the corner (cx, cz) of this block.
    auto corner = [&](int cx, int cz) {
        float sum = 0;
        int n = 0;
        for (int k = 0; k < 4; ++k) {
            const int j = i + off(cx - (k & 1), 0, cz - ((k >> 1) & 1));
            if (P[j + up] == blk) return 1.0f;
            if (P[j] == blk) {
                const int m = level(j);
                const float pct = fluid_height_percent(m);
                if (m >= 8 || m == 0) {
                    sum += pct * 10;
                    n += 10;
                }
                sum += pct;
                ++n;
            } else if (!is_solid(P[j])) {
                sum += 1;
                ++n;
            }
        }
        return 1.0f - sum / static_cast<float>(n);
    };
    // Corner heights at (x, z) = (0,0), (1,0), (1,1), (0,1).
    const float h[2][2] = {{corner(0, 0), corner(0, 1)}, {corner(1, 0), corner(1, 1)}};
    const glm::vec3 o(wx, wy, wz);
    for (const Face& f : FACES) {
        const BlockId nb = P[i + f.offset];
        if (nb == blk) continue;
        const bool is_top = f.def->normal.y == 1, is_bottom = f.def->normal.y == -1;
        if (!is_top) {
            if (blk == WATER && (is_bottom || (nb != AIR && !is_thin(nb) && !is_cutout(nb)))) continue;
            if (blk == LAVA && OPAQUE[nb]) continue;
        }
        std::array<glm::vec3, 4> pts;
        for (int c = 0; c < 4; ++c) {
            const glm::ivec3 k = f.def->corners[c];
            pts[c] = o + glm::vec3(k.x, k.y ? h[k.x][k.z] : 0.0f, k.z);
        }
        const uint8_t light = L[i + f.offset];
        mesh.quad_points(pts, BLOCK_UVS[blk].uv[f.def->slot], f.def->light, sky_of(light), block_of(light));
    }
}

}  // namespace

ChunkMesh build_mesh(const Padded& P, const PaddedLight& L, int cx, int cz, int max_y, MeshOptions options,
                     const Climate* climate, const Padded* data) {
    ChunkMesh mesh;
    const int ox = cx * CHUNK, oz = cz * CHUNK;
    const int top = std::min(HEIGHT, max_y + 1);
    const Rgb8 white{255, 255, 255};
    auto D = [&](int j) { return data ? static_cast<int>((*data)[j]) : 0; };
    static const std::array<float, 4> FARMLAND_WET_UV = tile_uv(T_FARMLAND_WET);
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
                if (is_torch(blk)) {
                    add_torch(mesh.solid, wx, wy, wz, blk, L[i]);
                    continue;
                }
                if (blk == FIRE) {
                    add_fire(mesh.solid, wx, wy, wz, L[i]);
                    continue;
                }
                if (is_bed(blk)) {
                    add_bed(mesh.solid, wx, wy, wz, blk, L[i]);
                    continue;
                }
                if (is_plant(blk)) {
                    Rgb8 tint = block_tint(blk) == BlockTint::GRASS ? grass[(z << 4) | x] : white;
                    add_plant(mesh.solid, wx, wy, wz, blk, D(i), L[i], tint);
                    continue;
                }
                if (is_rail(blk)) {
                    add_rail(mesh.solid, wx, wy, wz, blk, D(i), L[i]);
                    continue;
                }
                if (blk == CAKE_BLOCK) {
                    add_cake(mesh.solid, wx, wy, wz, D(i), L[i]);
                    continue;
                }
                if (blk == WATER || blk == LAVA) {
                    add_fluid(blk == WATER ? mesh.water : mesh.solid, P, data, L, i, blk, wx, wy, wz);
                    continue;
                }
                if (blk == ICE) {
                    // Ice is drawn see-through with the water.
                    for (const Face& f : FACES) {
                        BlockId nb = P[i + f.offset];
                        if (OPAQUE[nb] || nb == blk || nb == WATER) continue;
                        float l = f.def->light, s = sky_of(L[i + f.offset]), b = block_of(L[i + f.offset]);
                        mesh.water.quad(wx, wy, wz, f.def->corners, 1.0f, BLOCK_UVS[blk].uv[f.def->slot], {l, l, l, l},
                                        {s, s, s, s}, {b, b, b, b});
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
                // Farmland is 15/16 tall, darker on top when wet.
                const float h = blk == FARMLAND ? 15.0f / 16 : 1.0f;
                const bool wet_farmland = blk == FARMLAND && D(i) > 0;
                const bool furnace = is_furnace(blk);
                const glm::ivec3 facing = furnace ? FACING_DIRS[furnace_dir(blk)] : glm::ivec3(0);
                for (const Face& f : FACES) {
                    const int front = i + f.offset;
                    BlockId nb = P[front];
                    if (OPAQUE[nb] && !(LEAVES_TABLE[nb] && options.fancy_leaves)) continue;
                    if (nb == blk && is_cutout(blk)) continue;
                    std::array<float, 4> shades, sky, block;
                    for (int c = 0; c < 4; ++c) {
                        int occ = 0;
                        float s = sky_of(L[front]), b = block_of(L[front]);
                        if (options.smooth_lighting) {
                            bool a = OPAQUE[P[i + f.ao[c][0]]], bb = OPAQUE[P[i + f.ao[c][1]]];
                            bool d = OPAQUE[P[i + f.ao[c][2]]];
                            occ = (a && bb) ? 3 : a + bb + d;
                            // Smooth lighting: average the light of the open cells around the corner.
                            int n = 1;
                            auto add = [&](int j) {
                                s += sky_of(L[j]);
                                b += block_of(L[j]);
                                ++n;
                            };
                            if (!a) add(i + f.ao[c][0]);
                            if (!bb) add(i + f.ao[c][1]);
                            if (!d && !(a && bb)) add(i + f.ao[c][2]);
                            s /= n;
                            b /= n;
                        }
                        shades[c] = f.def->light * AO_LEVELS[occ];
                        sky[c] = s;
                        block[c] = b;
                    }
                    const auto& uv = furnace && f.def->normal == facing
                                         ? (is_lit_furnace(blk) ? LIT_FURNACE_FRONT_UV : FURNACE_FRONT_UV)
                                     : (wet_farmland && f.def->slot == 0) ? FARMLAND_WET_UV
                                                                          : uvs[f.def->slot];
                    mesh.solid.quad(wx, wy, wz, f.def->corners, h, uv, shades, sky, block, tint);
                }
            }
        }
    return mesh;
}

void MeshData::quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
                    const std::array<float, 4>& uv, const std::array<float, 4>& shades, Rgb8 tint) {
    quad(x, y, z, corners, h, uv, shades, {1, 1, 1, 1}, {0, 0, 0, 0}, tint);
}

void MeshData::quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
                    const std::array<float, 4>& uv, const std::array<float, 4>& shades,
                    const std::array<float, 4>& sky, const std::array<float, 4>& block, Rgb8 tint) {
    const float us[4] = {uv[0], uv[2], uv[2], uv[0]};
    const float vs[4] = {uv[1], uv[1], uv[3], uv[3]};
    const float r = tint.r / 255.0f, g = tint.g / 255.0f, b = tint.b / 255.0f;
    uint32_t n = static_cast<uint32_t>(vertices.size());
    float bright[4];
    for (int c = 0; c < 4; ++c) {
        vertices.push_back(
            {x + corners[c].x, y + corners[c].y * h, z + corners[c].z, shades[c], us[c], vs[c], sky[c], block[c], r, g, b});
        bright[c] = shades[c] * (0.2f + std::max(sky[c], block[c]));
    }
    // Flip the quad's diagonal so occlusion and light gradients interpolate smoothly.
    if (bright[0] + bright[2] < bright[1] + bright[3])
        indices.insert(indices.end(), {n + 1, n + 2, n + 3, n + 1, n + 3, n});
    else
        indices.insert(indices.end(), {n, n + 1, n + 2, n, n + 2, n + 3});
}

void MeshData::quad_points(const std::array<glm::vec3, 4>& pts, const std::array<float, 4>& uv, float shade,
                           float sky, float block, Rgb8 tint) {
    const float us[4] = {uv[0], uv[2], uv[2], uv[0]};
    const float vs[4] = {uv[1], uv[1], uv[3], uv[3]};
    const float r = tint.r / 255.0f, g = tint.g / 255.0f, b = tint.b / 255.0f;
    uint32_t n = static_cast<uint32_t>(vertices.size());
    for (int c = 0; c < 4; ++c) vertices.push_back({pts[c].x, pts[c].y, pts[c].z, shade, us[c], vs[c], sky, block, r, g, b});
    indices.insert(indices.end(), {n, n + 1, n + 2, n, n + 2, n + 3});
}
