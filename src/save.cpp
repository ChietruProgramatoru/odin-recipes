#include "save.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#include "json.h"
#include "zlib.h"

namespace fs = std::filesystem;

namespace {

bool valid_item(int id) { return !item_name(id).empty(); }

// --- Little-endian byte writing and reading -------------------------------------------

class ByteWriter {
public:
    std::vector<uint8_t> out;
    void u8(uint8_t v) { out.push_back(v); }
    void u16(uint16_t v) { put(v, 2); }
    void u32(uint32_t v) { put(v, 4); }
    void i32(int32_t v) { put(static_cast<uint32_t>(v), 4); }
    void f32(float v) { put(std::bit_cast<uint32_t>(v), 4); }
    void f64(double v) { put(std::bit_cast<uint64_t>(v), 8); }
    void bytes(const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); }

private:
    void put(uint64_t v, int n) {
        for (int i = 0; i < n; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
};

class ByteReader {
public:
    ByteReader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    bool ok() const { return ok_; }
    size_t left() const { return n_ - i_; }
    uint8_t u8() { return static_cast<uint8_t>(get(1)); }
    uint16_t u16() { return static_cast<uint16_t>(get(2)); }
    uint32_t u32() { return static_cast<uint32_t>(get(4)); }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() { return std::bit_cast<float>(u32()); }
    double f64() { return std::bit_cast<double>(get(8)); }
    const uint8_t* bytes(size_t n) {
        if (left() < n) {
            ok_ = false;
            return nullptr;
        }
        const uint8_t* p = p_ + i_;
        i_ += n;
        return p;
    }

private:
    uint64_t get(int n) {
        if (left() < static_cast<size_t>(n)) {
            ok_ = false;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < n; ++i) v |= static_cast<uint64_t>(p_[i_ + i]) << (8 * i);
        i_ += n;
        return v;
    }
    const uint8_t* p_;
    size_t n_, i_ = 0;
    bool ok_ = true;
};

Json vec3(glm::dvec3 v) {
    Json a = Json::array();
    a.push(v.x);
    a.push(v.y);
    a.push(v.z);
    return a;
}

std::optional<glm::dvec3> read_vec3(const Json* j) {
    if (!j || !j->is_array() || j->items().size() != 3) return std::nullopt;
    glm::dvec3 v;
    for (int i = 0; i < 3; ++i) {
        if (!j->items()[i].is_number()) return std::nullopt;
        v[i] = j->items()[i].as_number();
        if (!std::isfinite(v[i])) return std::nullopt;
    }
    return v;
}

std::optional<uint64_t> parse_u64(const std::string& s) {
    uint64_t v;
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc() || res.ptr != s.data() + s.size() || s.empty()) return std::nullopt;
    return v;
}

constexpr char REGION_MAGIC[4] = {'P', 'C', 'R', 'G'};
constexpr uint32_t REGION_VERSION = 1;
constexpr size_t REGION_HEADER = 8 + REGION_SIZE * REGION_SIZE * 8;

int region_of(int c) { return c >> 5; }
int region_index(int cx, int cz) { return (cz & (REGION_SIZE - 1)) * REGION_SIZE + (cx & (REGION_SIZE - 1)); }

}  // namespace

bool PlayerData::operator==(const PlayerData& o) const {
    return pos == o.pos && yaw == o.yaw && pitch == o.pitch && health == o.health && hunger == o.hunger &&
           saturation == o.saturation && exhaustion == o.exhaustion && creative == o.creative && flying == o.flying &&
           inventory.slots == o.inventory.slots && inventory.selected == o.inventory.selected;
}

// --- level.json ------------------------------------------------------------------------

std::string level_to_json(const LevelData& level) {
    Json j = Json::object();
    j["format"] = LEVEL_FORMAT;
    j["name"] = level.name;
    j["seed"] = std::to_string(level.seed);  // as text: JSON numbers can't hold every 64-bit seed
    j["time"] = static_cast<long long>(level.time);
    j["lastPlayed"] = static_cast<long long>(level.last_played);
    if (level.spawn) j["spawn"] = vec3(*level.spawn);
    if (level.player) {
        const PlayerData& p = *level.player;
        Json& pj = j["player"];
        pj["pos"] = vec3(p.pos);
        pj["yaw"] = p.yaw;
        pj["pitch"] = p.pitch;
        pj["health"] = p.health;
        pj["hunger"] = p.hunger;
        pj["saturation"] = p.saturation;
        pj["exhaustion"] = p.exhaustion;
        pj["creative"] = p.creative;
        pj["flying"] = p.flying;
        pj["selected"] = p.inventory.selected;
        Json inv = Json::array();
        for (int i = 0; i < Inventory::SIZE; ++i) {
            const auto& s = p.inventory.slots[i];
            if (!s) continue;
            Json slot = Json::object();
            slot["slot"] = i;
            slot["item"] = s->item;
            slot["count"] = s->count;
            inv.push(std::move(slot));
        }
        pj["inventory"] = std::move(inv);
    }
    return j.dump();
}

std::optional<LevelData> level_from_json(const std::string& text) {
    auto parsed = Json::parse(text);
    if (!parsed || !parsed->is_object()) return std::nullopt;
    const Json& j = *parsed;
    auto seed = parse_u64(j.string("seed"));
    if (!seed) return std::nullopt;
    LevelData level;
    level.seed = *seed;
    level.name = j.string("name", "World");
    level.time = static_cast<int64_t>(std::max(0.0, j.number("time", 0)));
    level.last_played = static_cast<int64_t>(j.number("lastPlayed", 0));
    level.spawn = read_vec3(j.get("spawn"));
    if (const Json* pj = j.get("player"); pj && pj->is_object()) {
        auto pos = read_vec3(pj->get("pos"));
        if (pos) {
            PlayerData p;
            p.pos = *pos;
            p.yaw = pj->number("yaw", 0);
            p.pitch = std::clamp(pj->number("pitch", 0), -90.0, 90.0);
            p.health = std::clamp(static_cast<int>(pj->number("health", MAX_HEALTH)), 0, MAX_HEALTH);
            p.hunger = std::clamp(static_cast<int>(pj->number("hunger", MAX_HUNGER)), 0, MAX_HUNGER);
            p.saturation = std::clamp(pj->number("saturation", 5), 0.0, static_cast<double>(MAX_HUNGER));
            p.exhaustion = std::clamp(pj->number("exhaustion", 0), 0.0, 4.0);
            p.creative = pj->boolean("creative", false);
            p.flying = p.creative && pj->boolean("flying", false);
            p.inventory.selected = std::clamp(static_cast<int>(pj->number("selected", 0)), 0, Inventory::SIZE - 1);
            if (const Json* inv = pj->get("inventory"); inv && inv->is_array())
                for (const Json& s : inv->items()) {
                    int slot = static_cast<int>(s.number("slot", -1));
                    int item = static_cast<int>(s.number("item", 0));
                    int count = static_cast<int>(s.number("count", 0));
                    if (slot < 0 || slot >= Inventory::SIZE || !valid_item(item) || count < 1) continue;
                    p.inventory.slots[slot] = Stack{item, std::min(count, STACK)};
                }
            level.player = p;
        }
    }
    return level;
}

// --- Chunks ----------------------------------------------------------------------------

std::vector<uint8_t> encode_chunk(const Chunk& c) {
    ByteWriter w;
    w.u8(CHUNK_FORMAT);
    w.u8(c.mob_spawned ? 1 : 0);
    w.u16(HEIGHT);
    w.bytes(c.blocks.data(), c.blocks.size());
    w.u16(static_cast<uint16_t>(std::min<size_t>(c.entities.size(), 0xFFFF)));
    for (size_t i = 0; i < c.entities.size() && i < 0xFFFF; ++i) {
        const EntityRecord& e = c.entities[i];
        w.u8(e.kind);
        w.f64(e.x);
        w.f64(e.y);
        w.f64(e.z);
        w.f32(e.yaw);
        w.i32(e.health);
        w.i32(e.item);
        w.f32(e.age);
    }
    return w.out;
}

std::unique_ptr<Chunk> decode_chunk(const std::vector<uint8_t>& bytes, int cx, int cz) {
    ByteReader r(bytes.data(), bytes.size());
    if (r.u8() != CHUNK_FORMAT) return nullptr;
    auto c = std::make_unique<Chunk>(cx, cz);
    c->mob_spawned = r.u8() & 1;
    if (r.u16() != HEIGHT) return nullptr;
    const uint8_t* blocks = r.bytes(c->blocks.size());
    if (!blocks) return nullptr;
    for (size_t i = 0; i < c->blocks.size(); ++i) c->blocks[i] = blocks[i] < BLOCK_COUNT ? blocks[i] : static_cast<BlockId>(AIR);
    int n = r.u16();
    for (int i = 0; i < n && r.ok(); ++i) {
        EntityRecord e;
        uint8_t kind = r.u8();
        e.x = r.f64();
        e.y = r.f64();
        e.z = r.f64();
        e.yaw = r.f32();
        e.health = r.i32();
        e.item = r.i32();
        e.age = r.f32();
        bool finite = std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z);
        if (kind == EntityRecord::COW && finite && e.health > 0) {
            e.kind = EntityRecord::COW;
            c->entities.push_back(e);
        } else if (kind == EntityRecord::ITEM && finite && valid_item(e.item)) {
            e.kind = EntityRecord::ITEM;
            c->entities.push_back(e);
        }
    }
    if (!r.ok()) return nullptr;
    c->recompute_max_y();
    c->dirty = false;
    c->saved_entities = !c->entities.empty();
    return c;
}

// --- Region files ------------------------------------------------------------------------

RegionStore::RegionStore(fs::path dir) : dir_(std::move(dir)) {}

std::string RegionStore::file_name(int rx, int rz) { return "r." + std::to_string(rx) + "." + std::to_string(rz) + ".pcr"; }

RegionStore::Region& RegionStore::region(int rx, int rz) {
    auto [it, added] = regions_.try_emplace({rx, rz});
    if (!added) return it->second;
    auto data = read_file(dir_ / file_name(rx, rz));
    if (!data) return it->second;
    const auto* p = reinterpret_cast<const uint8_t*>(data->data());
    ByteReader r(p, data->size());
    const uint8_t* magic = r.bytes(4);
    if (!magic || std::memcmp(magic, REGION_MAGIC, 4) != 0 || r.u32() != REGION_VERSION) {
        std::fprintf(stderr, "PetrutzCraft: region file %s is damaged, its chunks will be regenerated\n",
                     file_name(rx, rz).c_str());
        return it->second;
    }
    for (int i = 0; i < REGION_SIZE * REGION_SIZE; ++i) {
        uint32_t offset = r.u32(), size = r.u32();
        if (!r.ok() || size == 0) continue;
        if (offset < REGION_HEADER || offset > data->size() || size > data->size() - offset) continue;
        it->second.chunks[i].assign(p + offset, p + offset + size);
    }
    return it->second;
}

std::unique_ptr<Chunk> RegionStore::load(int cx, int cz) {
    std::vector<uint8_t> compressed;
    {
        std::lock_guard lock(mutex_);
        compressed = region(region_of(cx), region_of(cz)).chunks[region_index(cx, cz)];
    }
    if (compressed.empty()) return nullptr;
    auto raw = zlib_decompress(compressed, 1 << 20);
    auto chunk = raw ? decode_chunk(*raw, cx, cz) : nullptr;
    if (!chunk) std::fprintf(stderr, "PetrutzCraft: chunk %d %d is damaged and will be regenerated\n", cx, cz);
    return chunk;
}

bool RegionStore::write(const std::vector<std::pair<ChunkPos, std::vector<uint8_t>>>& chunks) {
    std::lock_guard lock(mutex_);
    std::vector<std::pair<int, int>> touched;
    for (const auto& [pos, data] : chunks) {
        std::pair<int, int> key{region_of(pos.cx), region_of(pos.cz)};
        region(key.first, key.second).chunks[region_index(pos.cx, pos.cz)] = data;
        if (std::find(touched.begin(), touched.end(), key) == touched.end()) touched.push_back(key);
    }
    std::error_code ec;
    fs::create_directories(dir_, ec);
    bool ok = true;
    for (auto [rx, rz] : touched) {
        const Region& reg = regions_[{rx, rz}];
        ByteWriter w;
        w.bytes(reinterpret_cast<const uint8_t*>(REGION_MAGIC), 4);
        w.u32(REGION_VERSION);
        uint32_t offset = static_cast<uint32_t>(REGION_HEADER);
        for (const auto& c : reg.chunks) {
            w.u32(c.empty() ? 0 : offset);
            w.u32(static_cast<uint32_t>(c.size()));
            offset += static_cast<uint32_t>(c.size());
        }
        for (const auto& c : reg.chunks) w.bytes(c.data(), c.size());
        ok &= write_file_atomic(dir_ / file_name(rx, rz), std::string(w.out.begin(), w.out.end()));
    }
    return ok;
}

// --- Background saving ---------------------------------------------------------------------

WorldSaver::WorldSaver(fs::path world_dir)
    : dir_(std::move(world_dir)), store_(dir_ / "region"), thread_([this] { run(); }) {}

WorldSaver::~WorldSaver() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    thread_.join();
}

void WorldSaver::save(LevelData level, std::vector<std::unique_ptr<Chunk>> chunks) {
    {
        std::lock_guard lock(mutex_);
        jobs_.push_back({std::move(level), std::move(chunks)});
    }
    cv_.notify_all();
}

bool WorldSaver::wait() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return jobs_.empty() && !busy_; });
    bool ok = ok_;
    ok_ = true;
    return ok;
}

bool WorldSaver::busy() {
    std::lock_guard lock(mutex_);
    return busy_ || !jobs_.empty();
}

void WorldSaver::run() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (jobs_.empty()) return;  // stopping, and every save is written
            job = std::move(jobs_.front());
            jobs_.pop_front();
            busy_ = true;
        }
        std::vector<std::pair<ChunkPos, std::vector<uint8_t>>> compressed;
        compressed.reserve(job.chunks.size());
        for (const auto& c : job.chunks) compressed.push_back({{c->cx, c->cz}, zlib_compress(encode_chunk(*c))});
        bool ok = store_.write(compressed);
        ok &= write_level(dir_, job.level);
        if (!ok) std::fprintf(stderr, "PetrutzCraft: could not save the world to %s\n", dir_.string().c_str());
        {
            std::lock_guard lock(mutex_);
            busy_ = false;
            ok_ &= ok;
        }
        cv_.notify_all();
    }
}

// --- World folders -------------------------------------------------------------------------

std::optional<LevelData> read_level(const fs::path& world_dir) {
    auto text = read_file(world_dir / "level.json");
    if (!text) return std::nullopt;
    return level_from_json(*text);
}

bool write_level(const fs::path& world_dir, const LevelData& level) {
    std::error_code ec;
    fs::create_directories(world_dir, ec);
    return write_file_atomic(world_dir / "level.json", level_to_json(level));
}

std::vector<WorldInfo> list_worlds(const fs::path& saves) {
    std::vector<WorldInfo> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(saves, ec)) {
        if (!entry.is_directory(ec)) continue;
        auto level = read_level(entry.path());
        if (!level) continue;
        WorldInfo w;
        w.folder = entry.path().filename().string();
        w.name = level->name;
        w.last_played = level->last_played;
        w.seed = level->seed;
        w.creative = level->player && level->player->creative;
        out.push_back(std::move(w));
    }
    std::sort(out.begin(), out.end(), [](const WorldInfo& a, const WorldInfo& b) {
        return a.last_played != b.last_played ? a.last_played > b.last_played : a.folder < b.folder;
    });
    return out;
}

std::string unique_folder_name(const fs::path& saves, const std::string& world_name) {
    std::string base;
    for (char c : world_name) {
        unsigned char u = static_cast<unsigned char>(c);
        bool bad = u < 0x20 || u >= 0x7F || std::strchr("/\\:*?\"<>|", c);
        base += bad ? '_' : c;
    }
    // Windows doesn't allow names that end in a space or dot.
    while (!base.empty() && (base.back() == ' ' || base.back() == '.')) base.pop_back();
    while (!base.empty() && base.front() == ' ') base.erase(base.begin());
    if (base.empty()) base = "World";
    std::string upper = base;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    static const char* RESERVED[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                     "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
                                     "LPT7", "LPT8", "LPT9"};
    for (const char* r : RESERVED)
        if (upper == r || upper.rfind(std::string(r) + ".", 0) == 0) base = "_" + base;
    std::string name = base;
    std::error_code ec;
    for (int n = 2; fs::exists(saves / name, ec); ++n) name = base + "-" + std::to_string(n);
    return name;
}

std::optional<std::string> create_world(const fs::path& saves, const std::string& name, uint64_t seed) {
    std::string folder = unique_folder_name(saves, name);
    LevelData level;
    level.name = name.empty() ? "New World" : name;
    level.seed = seed;
    level.last_played = now_ms();
    if (!write_level(saves / folder, level)) return std::nullopt;
    return folder;
}

bool rename_world(const fs::path& saves, const std::string& folder, const std::string& new_name) {
    auto level = read_level(saves / folder);
    if (!level || new_name.empty()) return false;
    level->name = new_name;
    return write_level(saves / folder, *level);
}

bool delete_world(const fs::path& saves, const std::string& folder) {
    // Only ever a world folder directly inside saves/.
    if (folder.empty() || folder == "." || folder == ".." || folder.find_first_of("/\\") != std::string::npos)
        return false;
    fs::path dir = saves / folder;
    std::error_code ec;
    if (!fs::exists(dir / "level.json", ec)) return false;
    fs::remove_all(dir, ec);
    return !ec;
}

std::optional<uint64_t> seed_from_text(const std::string& text) {
    size_t a = text.find_first_not_of(' '), b = text.find_last_not_of(' ');
    if (a == std::string::npos) return std::nullopt;
    std::string s = text.substr(a, b - a + 1);
    int64_t n;
    auto res = std::from_chars(s.data(), s.data() + s.size(), n);
    if (res.ec == std::errc() && res.ptr == s.data() + s.size()) return static_cast<uint64_t>(n);
    int32_t h = 0;  // Java's String.hashCode, which Beta used for text seeds
    for (unsigned char c : s) h = static_cast<int32_t>(31u * static_cast<uint32_t>(h) + c);
    return static_cast<uint64_t>(static_cast<int64_t>(h));
}

int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool write_file_atomic(const fs::path& path, const std::string& bytes) {
    // Write a temporary file and rename it over the old one, so a crash never leaves half a file.
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        f.flush();
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    return !ec;
}

std::optional<std::string> read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
