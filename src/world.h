// Voxel world: chunk storage, terrain generation, meshing and ray casting.
// Nothing in here touches OpenGL, so it can be tested without a window.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>

#include "blocks.h"
#include "noise.h"

constexpr int CHUNK = 16;
constexpr int HEIGHT = 128;
constexpr int SEA_LEVEL = 64;
constexpr int SNOWLESS_PEAK = 88;  // above this height the surface is bare stone

// --- Padded chunk layout used while meshing ------------------------------------
// An 18 x (HEIGHT + 2) x 18 copy of a chunk plus a one-block border from its neighbours,
// so neighbour lookups are plain index offsets.
constexpr int PW = CHUNK + 2;
constexpr int LAYER = PW * PW;
constexpr int PADDED_SIZE = LAYER * (HEIGHT + 2);
using Padded = std::vector<BlockId>;

struct ChunkPos {
    int cx = 0, cz = 0;
    bool operator==(const ChunkPos&) const = default;
};
struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const {
        return std::hash<uint64_t>()((static_cast<uint64_t>(static_cast<uint32_t>(p.cx)) << 32) |
                                     static_cast<uint32_t>(p.cz));
    }
};

// Floor division by 16 for block -> chunk coordinates (works for negatives).
inline int chunk_of(int block) { return block >> 4; }

struct Chunk {
    int cx, cz;
    std::array<BlockId, CHUNK * CHUNK * HEIGHT> blocks{};  // index (y << 8) | (z << 4) | x
    int max_y = 0;             // nothing above this height
    bool mob_spawned = false;  // cows were already rolled for this chunk

    Chunk(int x, int z) : cx(x), cz(z) {}
    static int index(int x, int y, int z) { return (y << 8) | (z << 4) | x; }
};

// Pure function of the seed, so worker threads can share one generator.
class TerrainGenerator {
public:
    explicit TerrainGenerator(uint64_t seed);
    int height(int wx, int wz) const;
    bool is_tree(int wx, int wz) const;
    std::unique_ptr<Chunk> generate(int cx, int cz) const;
    uint64_t seed() const { return seed_; }

private:
    uint64_t seed_;
    Perlin2D continent_, hills_, mountains_;
};

// Deterministic hash of a column to [0, 1).
double hash01(int x, int z, int salt, uint64_t seed);

struct RayHit {
    glm::ivec3 block;                 // the solid block that was hit
    std::optional<glm::ivec3> before; // the empty cell just before it (where a block would be placed)
    float distance;
};

// Vertex for chunk meshes, cows and dropped items.
struct Vertex {
    float x, y, z;
    float shade;
    float u, v;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool empty() const { return vertices.empty(); }
    // Quad from a unit-cube face (corners BL, BR, TR, TL) at block (x, y, z), height scaled by h.
    void quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
              const std::array<float, 4>& uv, const std::array<float, 4>& shades);
    // Quad from four absolute corner points (BL, BR, TR, TL) with one shade.
    void quad_points(const std::array<glm::vec3, 4>& pts, const std::array<float, 4>& uv, float shade);
};

struct ChunkMesh {
    MeshData solid, water;
};

// One cube face: outward normal, corners counter-clockwise seen from outside (BL, BR, TR, TL),
// light level and which tile to use (0 top, 1 side, 2 bottom).
struct FaceDef {
    glm::ivec3 normal;
    std::array<glm::ivec3, 4> corners;
    float light;
    int slot;
};
const std::array<FaceDef, 6>& face_defs();

// Mesh a padded chunk copy. Thread-safe: touches nothing but its arguments.
ChunkMesh build_mesh(const Padded& padded, int cx, int cz, int max_y);

// All chunks, keyed by chunk position. Only the main thread touches it; worker threads get
// generated chunks handed over with insert() and mesh from padded copies.
class World {
public:
    explicit World(uint64_t seed = 2024);

    uint64_t seed() const { return generator_.seed(); }
    const TerrainGenerator& generator() const { return generator_; }

    Chunk* find(int cx, int cz);                 // nullptr if not generated yet
    Chunk& chunk(int cx, int cz);                // generates it right away if missing
    void insert(std::unique_ptr<Chunk> chunk);   // keeps an existing chunk if there is one
    size_t chunk_count() const { return chunks_.size(); }

    BlockId get_block(int x, int y, int z);
    // Set a block. Returns the chunks whose meshes need rebuilding.
    std::vector<ChunkPos> set_block(int x, int y, int z, BlockId block);
    bool is_solid_at(int x, int y, int z) { return is_solid(get_block(x, y, z)); }
    // Y of the first free block above the highest solid one in a column.
    int surface_y(int x, int z);

    bool box_collides(double x0, double y0, double z0, double x1, double y1, double z1);
    bool box_in_water(double x0, double y0, double z0, double x1, double y1, double z1);

    // Voxel DDA ray cast. Water and air don't stop the ray.
    std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float max_dist);

    // Copy a chunk plus a one-block border from its 8 neighbours (generating any that are missing).
    Padded padded(int cx, int cz);
    // Mesh a chunk right now on this thread.
    ChunkMesh build_mesh_now(int cx, int cz);

private:
    TerrainGenerator generator_;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> chunks_;
    Chunk* last_ = nullptr;  // tiny cache for get_block
};
