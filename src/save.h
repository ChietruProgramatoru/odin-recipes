// Saving and loading worlds.
//
// Each world is a folder in saves/:
//   level.json        name, seed, play time, spawn point, weather and the player (see level_to_json)
//   region/r.X.Z.pcr  32x32 chunks per file, each chunk zlib-compressed (see encode_chunk)
// Chunks carry their blocks plus the cows, monsters and dropped items standing in them.
#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "survival.h"
#include "weather.h"
#include "world.h"

constexpr int LEVEL_FORMAT = 1;
// 2 added stack size and wear to dropped items, 3 monsters, 4 block data values and falling blocks
constexpr int CHUNK_FORMAT = 4;
constexpr int ARMOR_SLOT_BASE = 100;  // armor slot numbers in level.json, like Beta
constexpr int REGION_SIZE = 32;  // chunks per side of a region file
constexpr double AUTOSAVE_INTERVAL = 60.0;  // seconds

struct PlayerData {
    glm::dvec3 pos{0};
    double yaw = 0, pitch = 0;
    int health = MAX_HEALTH, hunger = MAX_HUNGER;
    double saturation = 5, exhaustion = 0;
    bool creative = false, flying = false;
    Inventory inventory;
    std::optional<glm::ivec3> bed;  // head of the bed last slept in: where the player respawns
    bool operator==(const PlayerData&) const;
};

struct LevelData {
    std::string name = "New World";
    uint64_t seed = 0;
    int64_t time = 0;         // game ticks played, 20 per second
    int64_t last_played = 0;  // milliseconds since 1970
    std::optional<glm::dvec3> spawn;
    WeatherState weather;
    std::optional<PlayerData> player;  // missing until the world is first saved from the game
};

std::string level_to_json(const LevelData& level);
std::optional<LevelData> level_from_json(const std::string& text);

// A chunk as uncompressed bytes, and back. decode_chunk returns nullptr for damaged data.
std::vector<uint8_t> encode_chunk(const Chunk& chunk);
std::unique_ptr<Chunk> decode_chunk(const std::vector<uint8_t>& bytes, int cx, int cz);

// Region files for one world. load() may be called from any thread.
class RegionStore : public ChunkStore {
public:
    explicit RegionStore(std::filesystem::path dir);
    std::unique_ptr<Chunk> load(int cx, int cz) override;
    // Replace these chunks (already compressed) and rewrite their region files.
    bool write(const std::vector<std::pair<ChunkPos, std::vector<uint8_t>>>& chunks);
    static std::string file_name(int rx, int rz);

private:
    struct Region {
        std::vector<std::vector<uint8_t>> chunks = std::vector<std::vector<uint8_t>>(REGION_SIZE * REGION_SIZE);
    };
    Region& region(int rx, int rz);  // call with mutex_ held

    std::filesystem::path dir_;
    std::mutex mutex_;
    std::map<std::pair<int, int>, Region> regions_;
};

// Writes saves on a background thread so the game doesn't stutter.
class WorldSaver {
public:
    explicit WorldSaver(std::filesystem::path world_dir);
    ~WorldSaver();  // finishes queued saves first
    WorldSaver(const WorldSaver&) = delete;
    WorldSaver& operator=(const WorldSaver&) = delete;

    RegionStore& store() { return store_; }
    const std::filesystem::path& dir() const { return dir_; }
    // Queue a save of the level file plus copies of the chunks that changed.
    void save(LevelData level, std::vector<std::unique_ptr<Chunk>> chunks);
    // Block until every queued save is on disk. Returns false if any of them failed.
    bool wait();
    // A save is queued or being written.
    bool busy();

private:
    struct Job {
        LevelData level;
        std::vector<std::unique_ptr<Chunk>> chunks;
    };
    void run();

    std::filesystem::path dir_;
    RegionStore store_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    bool busy_ = false, stop_ = false, ok_ = true;
    std::thread thread_;  // last: starts after everything above exists
};

// --- World folders ----------------------------------------------------------------------

struct WorldInfo {
    std::string folder;  // name of the folder in saves/
    std::string name;
    int64_t last_played = 0;
    uint64_t seed = 0;
    bool creative = false;
};

std::optional<LevelData> read_level(const std::filesystem::path& world_dir);
bool write_level(const std::filesystem::path& world_dir, const LevelData& level);
// Every readable world in saves/, most recently played first.
std::vector<WorldInfo> list_worlds(const std::filesystem::path& saves);
// A folder name for a new world: the name with characters Windows can't use replaced,
// plus "-2", "-3"... if that folder is taken.
std::string unique_folder_name(const std::filesystem::path& saves, const std::string& world_name);
// Make a new world folder with its level.json. Returns the folder name.
std::optional<std::string> create_world(const std::filesystem::path& saves, const std::string& name, uint64_t seed);
// Rename changes the name shown in the list; the folder keeps its name (like Beta).
bool rename_world(const std::filesystem::path& saves, const std::string& folder, const std::string& new_name);
bool delete_world(const std::filesystem::path& saves, const std::string& folder);

// The seed for a typed seed text: a whole number is used as is, other text is hashed like Beta
// did (Java's String.hashCode). Nothing for empty text (pick a random seed).
std::optional<uint64_t> seed_from_text(const std::string& text);

int64_t now_ms();
bool write_file_atomic(const std::filesystem::path& path, const std::string& bytes);
std::optional<std::string> read_file(const std::filesystem::path& path);
