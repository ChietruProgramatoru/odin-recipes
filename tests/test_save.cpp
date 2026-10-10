// Saving: compression, JSON, level.json, chunk and region files, world folders and options.txt.
#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "json.h"
#include "options.h"
#include "rng.h"
#include "save.h"
#include "zlib.h"

namespace fs = std::filesystem;

namespace {

// A fresh empty folder for one test.
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) : path(fs::temp_directory_path() / ("petrutz_test_" + name)) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

}  // namespace

TEST_CASE("zlib streams survive a round trip") {
    Rng rng(9);
    std::vector<std::vector<uint8_t>> inputs = {{}, {42}, std::vector<uint8_t>(40000, 7)};
    std::vector<uint8_t> noisy(20000);
    for (auto& b : noisy) b = static_cast<uint8_t>(rng.below(rng.random() < 0.5 ? 4 : 256));
    inputs.push_back(noisy);
    for (const auto& data : inputs) {
        auto packed = zlib_compress(data);
        auto back = zlib_decompress(packed);
        REQUIRE(back);
        CHECK(*back == data);
    }
    CHECK(zlib_compress(std::vector<uint8_t>(40000, 7)).size() < 400);
}

TEST_CASE("the decompressor reads stored and dynamic Huffman blocks from other zlib writers") {
    // zlib.compress(b"stored!", 0) in Python: one stored block.
    std::vector<uint8_t> stored = {0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x73, 0x74,
                                   0x6f, 0x72, 0x65, 0x64, 0x21, 0x0b, 0xef, 0x02, 0xb3};
    auto s = zlib_decompress(stored);
    REQUIRE(s);
    CHECK(std::string(s->begin(), s->end()) == "stored!");

    // zlib.compress(120 random words, 9) in Python: one dynamic Huffman block.
    std::vector<uint8_t> dynamic = {
        0x78, 0xda, 0x75, 0x51, 0x41, 0x0e, 0xc3, 0x20, 0x0c, 0xfb, 0x0a, 0x5f, 0x83, 0x36, 0x9b, 0xaa, 0x55, 0x74,
        0x6a, 0xaa, 0xed, 0xfb, 0x13, 0x09, 0x10, 0x87, 0x8e, 0x43, 0x2b, 0xc0, 0xb1, 0x1d, 0x27, 0x89, 0xe8, 0x11,
        0xde, 0x7b, 0xcc, 0x2f, 0x0e, 0xcf, 0x33, 0x32, 0x07, 0x8e, 0x79, 0x0d, 0xcb, 0xf1, 0x6d, 0xd7, 0xeb, 0xc8,
        0x14, 0xd6, 0xed, 0xbc, 0x5a, 0x59, 0x2f, 0xd8, 0x29, 0x7e, 0x88, 0x15, 0x33, 0x02, 0x20, 0xa9, 0x88, 0xab,
        0x40, 0x7d, 0x01, 0x35, 0x3d, 0x8a, 0x58, 0x3d, 0xca, 0x5f, 0x45, 0x4c, 0xaa, 0x7c, 0x88, 0x27, 0x68, 0xd8,
        0xe3, 0xa3, 0xb1, 0x6a, 0xd4, 0x52, 0xe7, 0x0f, 0x4a, 0x18, 0xec, 0xce, 0x12, 0x14, 0x1d, 0x35, 0x51, 0xe9,
        0xb9, 0x0d, 0x03, 0x52, 0x58, 0x14, 0x15, 0x99, 0x76, 0xa1, 0x6f, 0xf5, 0xf2, 0x0f, 0x12, 0x29, 0x9c, 0x7f,
        0xc9, 0xd6, 0xe7, 0x8c, 0xfe, 0xb7, 0xbe, 0x26, 0x81, 0xc6, 0x5d, 0x79, 0xba, 0x9a, 0x20, 0x6b, 0x92, 0x7c,
        0x5c, 0x9f, 0xf9, 0x62, 0x70, 0x29, 0x70, 0x81, 0xc0, 0xd2, 0x28, 0x6e, 0x27, 0xb8, 0x77, 0x0c, 0xd8, 0xf7,
        0xfb, 0x03, 0x7b, 0x55, 0xfb, 0x77};
    auto d = zlib_decompress(dynamic);
    REQUIRE(d);  // the checksum inside the stream was verified too
    CHECK(d->size() == 682);
    CHECK(std::string(d->begin(), d->begin() + 17) == "beef planks grass");

    // Damage is detected instead of producing garbage.
    auto broken = dynamic;
    broken[60] ^= 0x10;
    CHECK_FALSE(zlib_decompress(broken));
    CHECK_FALSE(zlib_decompress({0x78, 0xda, 0x01}));
}

TEST_CASE("JSON reads back what it writes") {
    Json j = Json::object();
    j["name"] = "Bob's \"world\"\n\ttab";
    j["n"] = 1.5;
    j["big"] = static_cast<long long>(1700000000123);
    j["yes"] = true;
    j["none"] = nullptr;
    Json list = Json::array();
    list.push(1);
    list.push("two");
    list.push(Json::object());
    j["list"] = list;
    auto back = Json::parse(j.dump());
    REQUIRE(back);
    CHECK(back->string("name") == "Bob's \"world\"\n\ttab");
    CHECK(back->number("n", 0) == 1.5);
    CHECK(back->number("big", 0) == 1700000000123.0);
    CHECK(back->boolean("yes", false));
    CHECK(back->get("none")->type() == Json::Type::Null);
    CHECK(back->get("list")->items().size() == 3);
    CHECK(back->dump() == j.dump());

    auto unicode = Json::parse(R"({"s": "café 😀"})");
    REQUIRE(unicode);
    CHECK(unicode->string("s") == "caf\xc3\xa9 \xf0\x9f\x98\x80");

    for (const char* bad : {"", "{", "[1,]", "{\"a\" 1}", "tru", "\"open", "1 2", "{\"a\":}"})
        CHECK_FALSE(Json::parse(bad));
}

TEST_CASE("level.json keeps the seed, spawn and the player") {
    LevelData level;
    level.name = "My World";
    level.seed = 18446744073709551000ull;  // too big for a JSON number, so it is stored as text
    level.time = 12345;
    level.last_played = 1700000000000;
    level.spawn = glm::dvec3(0.5, 70, -3.5);
    PlayerData p;
    p.pos = {10.25, 71, -4.75};
    p.yaw = 123.5;
    p.pitch = -20;
    p.health = 13;
    p.hunger = 9;
    p.saturation = 1.5;
    p.exhaustion = 2.25;
    p.inventory.add(DIRT, 70);
    p.inventory.add(BEEF, 3);
    p.inventory.add(DIAMOND, 2);  // materials from ores are saved too
    p.inventory.selected = 4;
    level.player = p;

    auto back = level_from_json(level_to_json(level));
    REQUIRE(back);
    CHECK(back->name == "My World");
    CHECK(back->seed == level.seed);
    CHECK(back->time == 12345);
    CHECK(back->last_played == 1700000000000);
    REQUIRE(back->spawn);
    CHECK(*back->spawn == *level.spawn);
    REQUIRE(back->player);
    CHECK(*back->player == p);

    CHECK_FALSE(level_from_json("{}"));  // no seed
    auto fresh = level_from_json(R"({"seed": "7", "name": "x"})");
    REQUIRE(fresh);
    CHECK_FALSE(fresh->player);
    // Bad values are fixed up rather than trusted.
    auto odd = level_from_json(R"({"seed": "7", "player": {"pos": [1, 2, 3], "health": 99,
        "inventory": [{"slot": 0, "item": 9999, "count": 5}, {"slot": 1, "item": 3, "count": 500}]}})");
    REQUIRE(odd);
    CHECK(odd->player->health == MAX_HEALTH);
    CHECK_FALSE(odd->player->inventory.slots[0]);
    CHECK(odd->player->inventory.slots[1] == Stack{STONE, STACK});
}

TEST_CASE("chunks keep their blocks, cow flag and entities") {
    World w(11);
    Chunk& c = w.chunk(3, -2);
    c.blocks[Chunk::index(4, 100, 5)] = BRICK;
    c.mob_spawned = true;
    c.entities.push_back({EntityRecord::COW, 50.5, 70, -30.25, 90, 8, 0, 0});
    c.entities.push_back({EntityRecord::ITEM, 51.5, 70.5, -29.75, 0, 0, BEEF, 12.5f});
    auto back = decode_chunk(encode_chunk(c), 3, -2);
    REQUIRE(back);
    CHECK(back->blocks == c.blocks);
    CHECK(back->mob_spawned);
    CHECK(back->entities == c.entities);
    CHECK(back->max_y == 101);
    CHECK_FALSE(back->dirty);
    CHECK(back->saved_entities);

    auto bytes = encode_chunk(c);
    bytes.resize(bytes.size() / 2);
    CHECK_FALSE(decode_chunk(bytes, 3, -2));
}

TEST_CASE("region files store chunks from every corner of a region") {
    TempDir dir("region");
    World w(12);
    std::vector<std::pair<ChunkPos, std::vector<uint8_t>>> data;
    for (ChunkPos pos : {ChunkPos{0, 0}, ChunkPos{31, 31}, ChunkPos{-1, -1}, ChunkPos{-32, 5}}) {
        Chunk& c = w.chunk(pos.cx, pos.cz);
        c.blocks[Chunk::index(1, 90, 1)] = BRICK;
        data.push_back({pos, zlib_compress(encode_chunk(c))});
    }
    {
        RegionStore store(dir.path);
        REQUIRE(store.write(data));
    }
    CHECK(fs::exists(dir.path / RegionStore::file_name(0, 0)));
    CHECK(fs::exists(dir.path / RegionStore::file_name(-1, -1)));
    CHECK(fs::exists(dir.path / RegionStore::file_name(-1, 0)));
    RegionStore fresh(dir.path);  // reads the files again
    for (ChunkPos pos : {ChunkPos{0, 0}, ChunkPos{31, 31}, ChunkPos{-1, -1}, ChunkPos{-32, 5}}) {
        auto c = fresh.load(pos.cx, pos.cz);
        REQUIRE(c);
        CHECK(c->blocks == w.chunk(pos.cx, pos.cz).blocks);
    }
    CHECK_FALSE(fresh.load(5, 5));    // never saved
    CHECK_FALSE(fresh.load(100, 0));  // no region file
}

TEST_CASE("a saved world loads with its edits and entities") {
    TempDir dir("world");
    LevelData level;
    level.seed = 99;
    {
        World w(99);
        WorldSaver saver(dir.path);
        w.set_store(&saver.store());
        w.set_block(5, 90, 5, BRICK);
        w.chunk(0, 0).entities.push_back({EntityRecord::COW, 5.5, 80, 5.5, 0, 10, 0, 0});
        std::vector<std::unique_ptr<Chunk>> copies;
        for (Chunk* c : w.all_chunks()) copies.push_back(std::make_unique<Chunk>(*c));
        saver.save(level, std::move(copies));
        CHECK(saver.wait());
    }
    World w(99);
    WorldSaver saver(dir.path);
    w.set_store(&saver.store());
    CHECK(w.get_block(5, 90, 5) == AIR);  // not loaded yet
    w.chunk(0, 0);
    CHECK(w.get_block(5, 90, 5) == BRICK);
    CHECK(w.find(0, 0)->entities.size() == 1);
    CHECK_FALSE(w.find(0, 0)->dirty);
    CHECK(read_level(dir.path)->seed == 99);
}

TEST_CASE("world folders: create, list, rename and delete") {
    TempDir saves("saves");
    auto a = create_world(saves.path, "Alpha", 1);
    auto b = create_world(saves.path, "Alpha", 2);
    auto c = create_world(saves.path, "what?/<now>", 3);
    REQUIRE(a);
    REQUIRE(b);
    REQUIRE(c);
    CHECK(*a == "Alpha");
    CHECK(*b == "Alpha-2");
    CHECK(*c == "what___now_");
    CHECK(unique_folder_name(saves.path, "con") == "_con");
    CHECK(unique_folder_name(saves.path, "  ") == "World");
    CHECK(list_worlds(saves.path).size() == 3);

    CHECK(rename_world(saves.path, "Alpha-2", "Beta"));
    auto worlds = list_worlds(saves.path);
    bool found = false;
    for (const auto& w : worlds) found |= w.folder == "Alpha-2" && w.name == "Beta" && w.seed == 2;
    CHECK(found);

    CHECK_FALSE(delete_world(saves.path, ".."));
    CHECK_FALSE(delete_world(saves.path, "missing"));
    CHECK(delete_world(saves.path, "Alpha"));
    CHECK(list_worlds(saves.path).size() == 2);
}

TEST_CASE("seeds from text") {
    CHECK_FALSE(seed_from_text(""));
    CHECK_FALSE(seed_from_text("   "));
    CHECK(seed_from_text("42") == 42u);
    CHECK(seed_from_text(" -1 ") == static_cast<uint64_t>(-1));
    CHECK(seed_from_text("Glacier") == static_cast<uint64_t>(static_cast<int64_t>(int32_t(1772835215))));
    CHECK(seed_from_text("hello") == seed_from_text("hello"));
}

TEST_CASE("options.txt keeps every setting and ignores junk") {
    Options o;
    CHECK(o.view_distance == VIEW_NORMAL);
    CHECK(o.keys[KEY_SNEAK] == GLFW_KEY_LEFT_SHIFT);
    CHECK(o.degrees_per_pixel() == doctest::Approx(0.15));

    o.invert_mouse = true;
    o.sensitivity = 0.75f;
    o.view_distance = VIEW_TINY;
    o.fancy_graphics = false;
    o.smooth_lighting = false;
    o.keys[KEY_JUMP] = GLFW_KEY_J;
    Options back = Options::from_text(o.to_text());
    CHECK(back.to_text() == o.to_text());
    CHECK(back.keys[KEY_JUMP] == GLFW_KEY_J);
    CHECK(back.sensitivity == 0.75f);

    Options junk = Options::from_text("mouseSensitivity:fast\nviewDistance:9\nnonsense\nkey_key.jump:-5\r\nao:false\r\n");
    CHECK(junk.sensitivity == 0.5f);
    CHECK(junk.view_distance == VIEW_TINY);  // clamped
    CHECK(junk.keys[KEY_JUMP] == GLFW_KEY_SPACE);
    CHECK_FALSE(junk.smooth_lighting);

    CHECK(view_distance_chunks(VIEW_FAR) == 16);
    CHECK(view_distance_chunks(VIEW_NORMAL) == 8);
    CHECK(view_distance_chunks(VIEW_SHORT) == 4);
    CHECK(view_distance_chunks(VIEW_TINY) == 2);
    CHECK(key_name(GLFW_KEY_W) == "W");
    CHECK(key_name(GLFW_KEY_LEFT_SHIFT) == "LSHIFT");
    CHECK(key_name(GLFW_KEY_F5) == "F5");
}
