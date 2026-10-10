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
// Light for the same cells, packed like Chunk::light.
using PaddedLight = std::vector<uint8_t>;

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

// A cow or dropped item stored with the chunk it is in (in a save file, or while the
// player is too far away for it to be simulated).
struct EntityRecord {
    enum Kind : uint8_t { COW = 1, ITEM = 2 };
    Kind kind = COW;
    double x = 0, y = 0, z = 0;
    float yaw = 0;   // cows
    int health = 0;  // cows
    int item = 0;    // dropped items
    float age = 0;   // dropped items, seconds
    int count = 1;   // dropped items: how many
    int damage = 0;  // dropped items: wear of a tool
    bool operator==(const EntityRecord&) const = default;
};

constexpr int MAX_LIGHT = 15;

struct Chunk {
    int cx, cz;
    std::array<BlockId, CHUNK * CHUNK * HEIGHT> blocks{};  // index (y << 8) | (z << 4) | x
    // Sky light in the high 4 bits, block light in the low 4 bits, same index as blocks.
    // Not saved: it is worked out again when a chunk is loaded.
    std::array<uint8_t, CHUNK * CHUNK * HEIGHT> light{};
    bool lit = false;          // light has been worked out for the chunk on its own
    int max_y = 0;             // nothing above this height
    bool mob_spawned = false;  // cows were already rolled for this chunk
    // Saving: true when the chunk changed since it was last saved (new chunks start dirty).
    bool dirty = true;
    bool saved_entities = false;        // the last save of this chunk had entities in it
    std::vector<EntityRecord> entities;  // waiting to be spawned when the chunk is shown

    Chunk(int x, int z) : cx(x), cz(z) {}
    static int index(int x, int y, int z) { return (y << 8) | (z << 4) | x; }
    void recompute_max_y();
};

// Where saved chunks come from. load() is called from worker threads.
class ChunkStore {
public:
    virtual ~ChunkStore() = default;
    // nullptr if the chunk was never saved (or can't be read).
    virtual std::unique_ptr<Chunk> load(int cx, int cz) = 0;
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

// Work out a chunk's light as if it had no neighbours (sky light down the columns and
// spreading sideways, plus light from torches and glowstone). Thread-safe.
void light_chunk(Chunk& chunk);

// Light passing from a cell with `level` into a neighbouring cell holding `block`. Sky light at
// full strength going straight down keeps its strength through air (open sky above).
inline int spread_light(int level, BlockId block, bool sky_straight_down) {
    int opacity = light_opacity(block);
    if (sky_straight_down && level == MAX_LIGHT) return MAX_LIGHT - opacity;
    return level - (opacity > 1 ? opacity : 1);
}

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
    float shade;  // face direction and ambient occlusion
    float u, v;
    float sky = 1, block = 0;  // light levels 0..1 (sky light is dimmed at night by the shader)
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool empty() const { return vertices.empty(); }
    // Quad from a unit-cube face (corners BL, BR, TR, TL) at block (x, y, z), height scaled by h.
    void quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
              const std::array<float, 4>& uv, const std::array<float, 4>& shades);
    // Same, with light levels (0..1) per corner.
    void quad(float x, float y, float z, const std::array<glm::ivec3, 4>& corners, float h,
              const std::array<float, 4>& uv, const std::array<float, 4>& shades,
              const std::array<float, 4>& sky, const std::array<float, 4>& block);
    // Quad from four absolute corner points (BL, BR, TR, TL) with one shade.
    void quad_points(const std::array<glm::vec3, 4>& pts, const std::array<float, 4>& uv, float shade,
                     float sky = 1, float block = 0);
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

// Video settings that change how chunks are meshed.
struct MeshOptions {
    bool smooth_lighting = true;  // ambient occlusion at the corners of faces
    bool fancy_leaves = true;     // see-through leaves (Fancy graphics); solid leaves when false
    bool operator==(const MeshOptions&) const = default;
};

// Mesh a padded chunk copy. Thread-safe: touches nothing but its arguments.
ChunkMesh build_mesh(const Padded& padded, const PaddedLight& light, int cx, int cz, int max_y, MeshOptions options = {});

// All chunks, keyed by chunk position. Only the main thread touches it; worker threads get
// generated chunks handed over with insert() and mesh from padded copies.
class World {
public:
    explicit World(uint64_t seed = 2024);

    uint64_t seed() const { return generator_.seed(); }
    const TerrainGenerator& generator() const { return generator_; }
    // Saved chunks are loaded from here before anything is generated.
    void set_store(ChunkStore* store) { store_ = store; }
    // A chunk from the store, or freshly generated. Thread-safe (doesn't touch the chunk map).
    std::unique_ptr<Chunk> load_or_generate(int cx, int cz) const;
    std::vector<Chunk*> all_chunks();

    Chunk* find(int cx, int cz);                 // nullptr if not generated yet
    Chunk& chunk(int cx, int cz);                // loads or generates it right away if missing
    // Keeps an existing chunk if there is one. Light flows in and out of the neighbours.
    void insert(std::unique_ptr<Chunk> chunk);
    size_t chunk_count() const { return chunks_.size(); }

    BlockId get_block(int x, int y, int z);
    // Set a block and update the light around it. Returns the chunks whose meshes need rebuilding.
    std::vector<ChunkPos> set_block(int x, int y, int z, BlockId block);
    // Light levels 0..15. Above the world (or in a chunk that isn't loaded) the sky is full.
    int sky_light(int x, int y, int z);
    int block_light(int x, int y, int z);
    bool is_solid_at(int x, int y, int z) { return is_solid(get_block(x, y, z)); }
    // Y of the first free block above the highest solid one in a column.
    int surface_y(int x, int z);

    bool box_collides(double x0, double y0, double z0, double x1, double y1, double z1);
    bool box_in_water(double x0, double y0, double z0, double x1, double y1, double z1);

    // Voxel DDA ray cast. Water and air don't stop the ray.
    std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float max_dist);

    // Copy a chunk plus a one-block border from its 8 neighbours (loading or generating any that are missing).
    Padded padded(int cx, int cz);
    PaddedLight padded_light(int cx, int cz);
    // Mesh a chunk right now on this thread.
    ChunkMesh build_mesh_now(int cx, int cz, MeshOptions options = {});

private:
    // Light (light.cpp): let light flow between a new chunk and its neighbours, and update
    // the light around a changed block (adding the chunks whose meshes see a change to `dirty`).
    void join_light(Chunk& chunk);
    void relight(int x, int y, int z, std::vector<ChunkPos>& dirty);

    TerrainGenerator generator_;
    ChunkStore* store_ = nullptr;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> chunks_;
    Chunk* last_ = nullptr;  // tiny cache for get_block
};
