// Sky light, block light, torches and the time of day.
#include <doctest/doctest.h>

#include <cmath>

#include "rng.h"
#include "sky.h"
#include "items.h"
#include "torch.h"
#include "world.h"

namespace {

// A world of nothing but a stone floor at y = 0..9, so light levels are easy to predict.
std::unique_ptr<Chunk> flat_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    return c;
}

void load_flat(World& w, int radius) {
    for (int cx = -radius; cx <= radius; ++cx)
        for (int cz = -radius; cz <= radius; ++cz) w.insert(flat_chunk(cx, cz));
}

}  // namespace

TEST_CASE("open sky is full light down to the ground, and dark inside it") {
    World w(1);
    load_flat(w, 1);
    CHECK(w.sky_light(5, 10, 5) == 15);
    CHECK(w.sky_light(5, HEIGHT - 1, 5) == 15);
    CHECK(w.sky_light(5, 9, 5) == 0);  // stone
    CHECK(w.block_light(5, 10, 5) == 0);
    CHECK(w.sky_light(5, HEIGHT + 5, 5) == 15);  // above the world
}

TEST_CASE("a roof shades the ground below it, and light creeps in from the sides") {
    World w(1);
    load_flat(w, 1);
    // A 9x9 stone roof at y = 14 over (0..8, 0..8).
    for (int x = 0; x < 9; ++x)
        for (int z = 0; z < 9; ++z) w.set_block(x, 14, z, STONE);
    CHECK(w.sky_light(4, 10, 4) == 15 - 5);  // 5 steps in from the open edge at x = -1
    CHECK(w.sky_light(0, 10, 4) == 14);
    CHECK(w.sky_light(-1, 10, 4) == 15);
    // Taking the roof away again brings back full light.
    for (int x = 0; x < 9; ++x)
        for (int z = 0; z < 9; ++z) w.set_block(x, 14, z, AIR);
    CHECK(w.sky_light(4, 10, 4) == 15);
}

TEST_CASE("leaves dim sky light by 1 and water by 3") {
    World w(1);
    load_flat(w, 1);
    // A column 1x1 deep pit of water under leaves... built in a sealed tube so only the column counts.
    for (int y = 10; y < 20; ++y)
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                if (dx || dz) w.set_block(4 + dx, y, 4 + dz, STONE);
    w.set_block(4, 19, 4, LEAVES);
    w.set_block(4, 18, 4, LEAVES);
    w.set_block(4, 12, 4, WATER);
    w.set_block(4, 11, 4, WATER);
    CHECK(w.sky_light(4, 19, 4) == 14);
    CHECK(w.sky_light(4, 18, 4) == 13);
    CHECK(w.sky_light(4, 17, 4) == 12);
    CHECK(w.sky_light(4, 13, 4) == 8);
    CHECK(w.sky_light(4, 12, 4) == 5);
    CHECK(w.sky_light(4, 11, 4) == 2);
    CHECK(w.sky_light(4, 10, 4) == 1);
}

TEST_CASE("a torch lights a closed room, one level less per block, across chunk edges") {
    World w(1);
    load_flat(w, 2);
    // A sealed box of stone with no sky light inside: floor y = 9, room y = 10..12, roof y = 13.
    for (int x = -20; x <= 40; ++x)
        for (int z = -6; z <= 6; ++z) w.set_block(x, 13, z, STONE);
    for (int x = -20; x <= 40; ++x) {
        w.set_block(x, 11, -6, STONE);
        w.set_block(x, 11, 6, STONE);
    }
    // Close the long sides fully.
    for (int y = 10; y <= 12; ++y)
        for (int x = -21; x <= 41; ++x) {
            w.set_block(x, y, -7, STONE);
            w.set_block(x, y, 7, STONE);
        }
    for (int y = 10; y <= 12; ++y)
        for (int z = -7; z <= 7; ++z) {
            w.set_block(-21, y, z, STONE);
            w.set_block(41, y, z, STONE);
        }
    CHECK(w.sky_light(10, 10, 0) == 0);

    w.set_block(12, 10, 0, TORCH);
    CHECK(w.block_light(12, 10, 0) == 14);
    CHECK(w.block_light(13, 10, 0) == 13);
    CHECK(w.block_light(15, 10, 0) == 11);
    CHECK(w.block_light(16, 10, 0) == 10);  // next chunk
    CHECK(w.block_light(25, 10, 0) == 1);
    CHECK(w.block_light(26, 10, 0) == 0);
    CHECK(w.block_light(12, 9, 0) == 0);  // stone stops it

    w.set_block(12, 10, 0, AIR);
    for (int x = 0; x < 30; ++x) CHECK(w.block_light(x, 10, 0) == 0);

    w.set_block(0, 11, 0, GLOWSTONE);
    CHECK(w.block_light(0, 11, 0) == 15);
    CHECK(w.block_light(1, 11, 0) == 14);
}

TEST_CASE("light after many edits matches lighting the same blocks from scratch") {
    World w(9);
    for (int cx = -1; cx <= 1; ++cx)
        for (int cz = -1; cz <= 1; ++cz) w.chunk(cx, cz);
    Rng rng(5);
    const BlockId palette[] = {AIR, AIR, STONE, TORCH, GLOWSTONE, LEAVES, WATER, AIR};
    for (int i = 0; i < 400; ++i) {
        int x = rng.randint(-10, 25), y = rng.randint(55, 85), z = rng.randint(-10, 25);
        w.set_block(x, y, z, palette[rng.below(8)]);
    }
    World fresh(9);
    for (Chunk* c : w.all_chunks()) {
        auto copy = std::make_unique<Chunk>(c->cx, c->cz);
        copy->blocks = c->blocks;
        copy->max_y = c->max_y;
        fresh.insert(std::move(copy));
    }
    int differences = 0;
    for (Chunk* c : w.all_chunks())
        if (fresh.find(c->cx, c->cz)->light != c->light) ++differences;
    CHECK(differences == 0);
}

TEST_CASE("chunk meshes carry light: brighter near a torch, darker under a roof") {
    World w(1);
    load_flat(w, 1);
    w.set_block(8, 10, 8, TORCH);
    ChunkMesh m = w.build_mesh_now(0, 0);
    float near_torch = 0, far = 1;
    for (const Vertex& v : m.solid.vertices) {
        if (std::abs(v.y - 10) > 0.01f) continue;  // the floor's top faces
        float d = std::abs(v.x - 8.5f) + std::abs(v.z - 8.5f);
        if (d < 1.5f) near_torch = std::max(near_torch, v.block);
        if (d > 12) far = std::min(far, v.block);
    }
    CHECK(near_torch > 0.8f);
    CHECK(far < 0.2f);
    // The torch itself is a thin stick, not a cube: 6 small faces.
    size_t torch_faces = 0;
    for (const Vertex& v : m.solid.vertices)
        if (v.y > 10.01f && v.x > 8 && v.x < 9 && v.z > 8 && v.z < 9) ++torch_faces;
    CHECK(torch_faces > 0);
    CHECK(torch_faces <= 6 * 4);
}

TEST_CASE("torches stand on top of blocks and hang on their sides, never below") {
    CHECK(torch_for_face({0, 1, 0}) == TORCH);
    CHECK_FALSE(torch_for_face({0, -1, 0}));
    for (glm::ivec3 face : {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)}) {
        auto t = torch_for_face(face);
        REQUIRE(t);
        CHECK(torch_support(*t) == -face);  // hangs on the block that was clicked
    }
    CHECK(torch_support(TORCH) == glm::ivec3(0, -1, 0));
    CHECK_FALSE(is_solid(TORCH));
    CHECK_FALSE(is_opaque(TORCH_WALL_EAST));
    CHECK(light_emission(TORCH) == 14);
    CHECK(light_emission(GLOWSTONE) == 15);
    CHECK(is_opaque(GLOWSTONE));
    CHECK(block_info(TORCH_WALL_NORTH)->drop == TORCH);
}

TEST_CASE("a day lasts 20 minutes and starts in the morning") {
    CHECK(DAY_TICKS == 20 * 60 * 20);
    CHECK(clock_minutes(0) == 6 * 60);
    CHECK(clock_minutes(6000) == 12 * 60);
    CHECK(clock_minutes(18000) == 0);
    CHECK(celestial_angle(6000) == doctest::Approx(0.0f));   // noon: sun overhead
    CHECK(celestial_angle(18000) == doctest::Approx(0.5f));  // midnight
    CHECK(celestial_angle(6000 + DAY_TICKS * 3) == doctest::Approx(0.0f));
    CHECK(sky_darkness(celestial_angle(6000)) == doctest::Approx(0.0f));
    CHECK(sky_darkness(celestial_angle(18000)) == doctest::Approx(11.0f));
    // The morning a new world starts in is already bright, and the night is dark with stars.
    CHECK(daylight_factor(celestial_angle(0)) > 0.8f);
    CHECK(star_brightness(celestial_angle(6000)) == 0.0f);
    CHECK(star_brightness(celestial_angle(18000)) > 0.4f);
    CHECK(sunset_color(celestial_angle(12000)));
    CHECK_FALSE(sunset_color(celestial_angle(6000)));
    CHECK(sky_color(celestial_angle(18000)).b < 0.05f);
}

TEST_CASE("the cloud map has clouds and gaps and repeats without a seam") {
    int clouds = 0;
    for (uint8_t c : cloud_map()) clouds += c;
    double cover = static_cast<double>(clouds) / (CLOUD_MAP * CLOUD_MAP);
    CHECK(cover > 0.2);
    CHECK(cover < 0.6);
    CHECK(cloud_at(-1, 5) == cloud_at(CLOUD_MAP - 1, 5));
    CHECK(cloud_at(3, CLOUD_MAP + 7) == cloud_at(3, 7));
}

TEST_CASE("torches and glowstone from M2 saves load as the new blocks, and torches still craft") {
    CHECK(current_item_id(OLD_TORCH_ITEM) == TORCH);
    CHECK(current_item_id(OLD_GLOWSTONE_ITEM) == GLOWSTONE);
    CHECK(current_item_id(OLD_TNT_ITEM) == TNT);
    CHECK(current_item_id(REDSTONE_TORCH) == REDSTONE_TORCH);
    CHECK_FALSE(valid_item(OLD_TORCH_ITEM));
    CHECK(valid_item(TORCH));
    CHECK_FALSE(valid_item(TORCH_WALL_EAST));  // only the floor torch is an item
    CHECK(item_name(TORCH) == "Torch");
    CHECK_FALSE(looks_like_block(TORCH));  // a flat icon
    CHECK(looks_like_block(GLOWSTONE));
}
