// World generation: biomes, terrain, caves, ores, dungeons, trees and the blocks they use.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <map>

#include "world.h"

namespace {

// Every block of a square of chunks, generated directly.
struct Area {
    TerrainGenerator gen;
    int r;
    std::map<std::pair<int, int>, std::unique_ptr<Chunk>> chunks;
    Area(uint64_t seed, int radius) : gen(seed), r(radius) {
        for (int cx = -r; cx < r; ++cx)
            for (int cz = -r; cz < r; ++cz) chunks[{cx, cz}] = gen.generate(cx, cz);
    }
    BlockId get(int x, int y, int z) const {
        if (y < 0 || y >= HEIGHT) return AIR;
        auto it = chunks.find({chunk_of(x), chunk_of(z)});
        return it == chunks.end() ? BlockId{AIR} : it->second->blocks[Chunk::index(x & 15, y, z & 15)];
    }
    template <typename F>
    void each(F&& fn) const {
        for (const auto& [pos, c] : chunks)
            for (int y = 0; y < HEIGHT; ++y)
                for (int z = 0; z < CHUNK; ++z)
                    for (int x = 0; x < CHUNK; ++x)
                        fn(c->cx * CHUNK + x, y, c->cz * CHUNK + z, c->blocks[Chunk::index(x, y, z)]);
    }
};

}  // namespace

TEST_CASE("biomes come from temperature and rainfall like Beta") {
    CHECK(biome_for(0.05, 0.9) == Biome::TUNDRA);
    CHECK(biome_for(0.05, 0.1) == Biome::ICE_DESERT);
    CHECK(biome_for(1.0, 0.1) == Biome::DESERT);
    CHECK(biome_for(0.8, 0.1) == Biome::SAVANNA);
    CHECK(biome_for(0.6, 0.95) == Biome::SWAMP);
    CHECK(biome_for(0.4, 0.6) == Biome::TAIGA);
    CHECK(biome_for(0.9, 0.3) == Biome::SHRUBLAND);
    CHECK(biome_for(0.9, 0.6) == Biome::FOREST);
    CHECK(biome_for(1.0, 0.4) == Biome::PLAINS);
    CHECK(biome_for(1.0, 0.7) == Biome::SEASONAL_FOREST);
    CHECK(biome_for(1.0, 0.95) == Biome::RAINFOREST);
    CHECK(is_cold(Biome::TAIGA));
    CHECK_FALSE(is_cold(Biome::DESERT));
    CHECK(std::string(biome_name(Biome::SEASONAL_FOREST)) == "Seasonal Forest");
}

TEST_CASE("every biome shows up somewhere") {
    TerrainGenerator gen(7);
    std::map<Biome, int> seen;
    for (int x = -6000; x < 6000; x += 50)
        for (int z = -6000; z < 6000; z += 50) seen[gen.biome_at(x, z)]++;
    for (int b = 0; b < static_cast<int>(Biome::COUNT); ++b)
        CHECK_MESSAGE(seen[static_cast<Biome>(b)] > 0, biome_name(static_cast<Biome>(b)));
}

TEST_CASE("grass and leaves are coloured by the climate") {
    Rgb8 cold = grass_color(0.0, 0.5), dry = grass_color(1.0, 0.0), wet = grass_color(1.0, 1.0);
    CHECK_FALSE(cold == dry);
    CHECK_FALSE(dry == wet);
    CHECK(wet.g > wet.r);  // hot and wet is bright green
    CHECK(dry.r > wet.r);  // hot and dry is yellow-brown
    CHECK_FALSE(foliage_color(0.0, 0.5) == foliage_color(1.0, 1.0));
    // Grass blocks in the mesh carry the colour of their column.
    World w(5);
    const int y = HEIGHT - 10;
    w.set_block(4, y, 4, GRASS);
    auto grass_tint = [&](uint8_t temperature) {
        Chunk& c = w.chunk(0, 0);
        c.climate.temperature.fill(temperature);
        c.climate.rainfall.fill(200);
        for (const Vertex& v : w.build_mesh_now(0, 0).solid.vertices)
            if (v.y > y - 0.01f) return glm::vec3(v.r, v.g, v.b);
        return glm::vec3(-1);
    };
    glm::vec3 hot = grass_tint(255), frozen = grass_tint(0);
    CHECK(hot.x >= 0);
    CHECK(hot != frozen);
}

TEST_CASE("a chunk is the same however and whenever it is generated") {
    TerrainGenerator a(99), b(99);
    auto c1 = a.generate(3, -2), c2 = b.generate(3, -2), c3 = a.generate(3, -2);
    CHECK(c1->blocks == c2->blocks);
    CHECK(c1->blocks == c3->blocks);
    CHECK(c1->climate.temperature == c2->climate.temperature);
}

TEST_CASE("bedrock floor, terrain, caves and ores by depth") {
    Area area(7, 4);
    std::map<BlockId, int> count, highest;
    int cave_air = 0, overhangs = 0, holes_in_floor = 0, high_bedrock = 0;
    area.each([&](int x, int y, int z, BlockId b) {
        ++count[b];
        highest[b] = std::max(highest[b], y);
        holes_in_floor += y == 0 && b != BEDROCK;
        high_bedrock += y > 4 && b == BEDROCK;
        if (b == AIR && y < 40) ++cave_air;
        if (b == AIR && y > SEA_LEVEL && is_solid(area.get(x, y + 1, z)) && area.get(x, y + 1, z) != LOG &&
            !is_leaves(area.get(x, y + 1, z)) && area.get(x, y + 1, z) != SPRUCE_LOG && area.get(x, y + 1, z) != BIRCH_LOG)
            ++overhangs;
    });
    CHECK(holes_in_floor == 0);
    CHECK(high_bedrock == 0);
    CHECK(cave_air > 1000);  // caves wind through the underground
    CHECK(overhangs > 0);    // the 3D noise makes overhangs above the sea
    for (BlockId ore : {COAL_ORE, IRON_ORE, GOLD_ORE, REDSTONE_ORE, DIAMOND_ORE, LAPIS_ORE, GRAVEL})
        CHECK_MESSAGE(count[ore] > 0, block_info(ore)->name);
    // A vein can stray a few blocks above where it starts.
    CHECK(highest[DIAMOND_ORE] < 16 + 6);
    CHECK(highest[REDSTONE_ORE] < 16 + 6);
    CHECK(highest[GOLD_ORE] < 32 + 6);
    CHECK(highest[LAPIS_ORE] < 32 + 6);
    CHECK(highest[IRON_ORE] < 64 + 6);
    CHECK(count[COAL_ORE] > count[IRON_ORE]);
    CHECK(count[IRON_ORE] > count[GOLD_ORE]);
    CHECK(count[GOLD_ORE] > count[DIAMOND_ORE]);
}

TEST_CASE("trees are whole even where they cross chunk borders") {
    // Each chunk is generated on its own, so a tree split between chunks only comes out whole
    // if every chunk agrees on it. Every leaf needs a log close by.
    Area area(11, 4);
    int leaves = 0, orphans = 0, logs = 0;
    const int inner = (area.r - 1) * CHUNK;  // skip the outer ring, whose neighbours weren't generated
    area.each([&](int x, int y, int z, BlockId b) {
        if (b == LOG || b == BIRCH_LOG || b == SPRUCE_LOG) ++logs;
        if (!is_leaves(b) || x < -inner || x >= inner || z < -inner || z >= inner) return;
        ++leaves;
        bool near = false;
        for (int dx = -4; dx <= 4 && !near; ++dx)
            for (int dy = -4; dy <= 4 && !near; ++dy)
                for (int dz = -4; dz <= 4 && !near; ++dz) {
                    BlockId n = area.get(x + dx, y + dy, z + dz);
                    near = n == LOG || n == BIRCH_LOG || n == SPRUCE_LOG;
                }
        orphans += !near;
    });
    CHECK(logs > 0);
    CHECK(leaves > 0);
    CHECK(orphans == 0);
}

TEST_CASE("cold biomes freeze their water and deserts have sandstone under the sand") {
    TerrainGenerator gen(7);
    int frozen = 0, open = 0, sand_tops = 0, sandstone = 0, desert_columns = 0;
    for (int cx = -40; cx < 40; cx += 5)
        for (int cz = -40; cz < 40; cz += 5) {
            Biome b = gen.biome_at(cx * CHUNK + 8, cz * CHUNK + 8);
            if (!is_cold(b) && b != Biome::DESERT) continue;
            auto c = gen.generate(cx, cz);
            for (int i = 0; i < CHUNK * CHUNK; ++i) {
                int x = i & 15, z = i >> 4;
                int top = HEIGHT - 1;
                while (top > 0 && c->blocks[Chunk::index(x, top, z)] == AIR) --top;
                BlockId t = c->blocks[Chunk::index(x, top, z)];
                Biome col = c->climate.biome((z << 4) | x);
                if (is_cold(col)) {
                    frozen += t == ICE;
                    open += t == WATER;
                }
                if (col == Biome::DESERT && top > SEA_LEVEL + 2) {
                    ++desert_columns;
                    sand_tops += t == SAND;
                    for (int y = top; y > top - 10; --y) sandstone += c->blocks[Chunk::index(x, y, z)] == SANDSTONE ? 1 : 0;
                }
            }
        }
    CHECK(frozen > 0);
    CHECK(open == 0);
    REQUIRE(desert_columns > 0);
    CHECK(sand_tops > desert_columns * 3 / 4);
    CHECK(sandstone > desert_columns);
}

TEST_CASE("dungeons are cobblestone rooms with a spawner in the middle") {
    TerrainGenerator gen(7);
    std::optional<glm::ivec3> spawner;
    std::unique_ptr<Chunk> chunk;
    for (int r = 0; r < 14 && !spawner; ++r)
        for (int cx = -r; cx <= r && !spawner; ++cx)
            for (int cz = -r; cz <= r && !spawner; ++cz) {
                if (std::max(std::abs(cx), std::abs(cz)) != r) continue;
                chunk = gen.generate(cx, cz);
                for (int i = 0; i < CHUNK * CHUNK * HEIGHT && !spawner; ++i)
                    if (chunk->blocks[i] == SPAWNER) spawner = glm::ivec3(cx * CHUNK + (i & 15), i >> 8, cz * CHUNK + ((i >> 4) & 15));
            }
    REQUIRE(spawner);
    World w(7);
    w.chunk(chunk_of(spawner->x), chunk_of(spawner->z));
    BlockId floor = w.get_block(spawner->x, spawner->y - 1, spawner->z);
    CHECK((floor == COBBLE || floor == MOSSY_COBBLE));
    CHECK(w.get_block(spawner->x + 1, spawner->y, spawner->z) == AIR);
    CHECK(w.get_block(spawner->x, spawner->y + 1, spawner->z) == AIR);
}

TEST_CASE("ores drop Beta's items and new blocks have names") {
    CHECK(block_info(COAL_ORE)->drop == COAL);
    CHECK(block_info(DIAMOND_ORE)->drop == DIAMOND);
    CHECK(block_info(IRON_ORE)->drop == IRON_ORE);  // smelted later (M7)
    CHECK(block_info(GOLD_ORE)->drop == GOLD_ORE);
    CHECK(block_info(REDSTONE_ORE)->drop == REDSTONE);
    CHECK(block_info(REDSTONE_ORE)->drop_min == 4);
    CHECK(block_info(REDSTONE_ORE)->drop_max == 5);
    CHECK(block_info(LAPIS_ORE)->drop == LAPIS);
    CHECK(block_info(LAPIS_ORE)->drop_max == 8);
    CHECK(block_info(CLAY)->drop == CLAY_BALL);
    CHECK(block_info(CLAY)->drop_min == 4);
    CHECK_FALSE(block_info(SPAWNER)->drop);
    CHECK_FALSE(block_info(LAVA)->hardness);
    CHECK(item_name(COAL) == "Coal");
    CHECK(item_name(CLAY_BALL) == "Clay");
    CHECK(item_name(MOSSY_COBBLE) == "Mossy Cobblestone");
    CHECK_FALSE(is_block(DIAMOND));
    for (ItemId item : all_items()) CHECK_MESSAGE(icon_tile(item) < TILE_COUNT, item_name(item));
    CHECK(std::count(all_items().begin(), all_items().end(), LAPIS) == 1);
}

TEST_CASE("lava and water don't stop the crosshair; lava, ice and spawners show what's behind them") {
    World w(5);
    const int y = HEIGHT - 10;
    w.set_block(4, y, 4, LAVA);
    w.set_block(4, y, 6, STONE);
    auto hit = w.raycast({4.5f, y + 0.5f, 3.5f}, {0, 0, 1}, 5);
    REQUIRE(hit);
    CHECK(hit->block == glm::ivec3(4, y, 6));
    CHECK(is_fluid(LAVA));
    CHECK_FALSE(is_solid(LAVA));
    CHECK(is_solid(ICE));
    CHECK_FALSE(is_opaque(ICE));
    CHECK_FALSE(is_opaque(SPAWNER));
    CHECK(is_leaves(SPRUCE_LEAVES));
}
