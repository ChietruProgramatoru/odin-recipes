// Terrain, meshing, ray casting and physics.
#include <doctest/doctest.h>

#include <algorithm>

#include "physics.h"
#include "world.h"

TEST_CASE("padded chunk matches the world, including the border from its neighbours") {
    World w(3);
    Chunk& c = w.chunk(0, 0);
    Padded P = w.padded(0, 0);
    REQUIRE(P.size() == static_cast<size_t>(LAYER * (HEIGHT + 2)));
    for (int y = 0; y <= c.max_y; ++y)
        for (int z = -1; z < 17; ++z)
            for (int x = -1; x < 17; ++x)
                REQUIRE(P[(y + 1) * LAYER + (z + 1) * PW + (x + 1)] == w.get_block(x, y, z));
    CHECK(P[0] == BEDROCK);  // below the world
}

TEST_CASE("terrain has bedrock, ground above sea level somewhere, and a valid mesh") {
    World w(5);
    ChunkMesh mesh = w.build_mesh_now(0, 0);
    REQUIRE_FALSE(mesh.solid.empty());
    CHECK(mesh.solid.indices.size() % 6 == 0);
    CHECK(*std::max_element(mesh.solid.indices.begin(), mesh.solid.indices.end()) < mesh.solid.vertices.size());
    CHECK(mesh.solid.vertices.size() % 4 == 0);
    CHECK(w.surface_y(0, 0) > 1);
    CHECK(w.get_block(3, 0, 3) == BEDROCK);
}

TEST_CASE("the same seed always makes the same terrain") {
    World a(42), b(42), c(43);
    bool differs = false;
    for (int x = -20; x < 20; x += 3)
        for (int z = -20; z < 20; z += 3) {
            CHECK(a.surface_y(x, z) == b.surface_y(x, z));
            differs |= a.surface_y(x, z) != c.surface_y(x, z);
        }
    CHECK(differs);
}

TEST_CASE("a fully buried block has no visible faces") {
    World w(5);
    int y = w.surface_y(8, 8);
    // Fill a 3x3x3 cube of stone and check the centre block adds no faces to the mesh.
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz) w.set_block(8 + dx, y + 5 + dy, 8 + dz, STONE);
    size_t with_centre = w.build_mesh_now(0, 0).solid.vertices.size();
    w.set_block(8, y + 5, 8, AIR);
    size_t hollow = w.build_mesh_now(0, 0).solid.vertices.size();
    CHECK(hollow == with_centre + 6 * 4);  // the hole shows the 6 inner faces of its neighbours
}

TEST_CASE("ray cast finds the block under the player and the cell above it") {
    World w(5);
    int y = w.surface_y(4, 4);
    auto hit = w.raycast({4.5f, y + 1.5f, 4.5f}, {0, -1, 0}, 5);
    REQUIRE(hit);
    CHECK(hit->block == glm::ivec3(4, y - 1, 4));
    REQUIRE(hit->before);
    CHECK(*hit->before == glm::ivec3(4, y, 4));
    w.set_block(4, y - 1, 4, AIR);
    CHECK(w.get_block(4, y - 1, 4) == AIR);
}

TEST_CASE("editing a block on a chunk edge marks the neighbouring chunks dirty") {
    World w(5);
    for (int cx = -1; cx <= 0; ++cx)
        for (int cz = -1; cz <= 0; ++cz) w.chunk(cx, cz);
    auto dirty = w.set_block(0, 30, 0, STONE);  // corner of chunk (0, 0)
    CHECK(dirty.size() == 4);
    dirty = w.set_block(5, 30, 5, STONE);  // middle of the chunk
    CHECK(dirty.size() == 1);
}

TEST_CASE("a falling body lands exactly on the ground") {
    World w(5);
    int y = w.surface_y(8, 8);
    Body b(w, 8.5, y + 5, 8.5, 0.3, 1.8);
    for (int i = 0; i < 200; ++i) {
        b.apply_gravity(1.0 / 60);
        b.move(0, b.vy / 60, 0);
    }
    CHECK(b.on_ground);
    CHECK(b.y == doctest::Approx(y).epsilon(1e-6));
}

TEST_CASE("a body walking into a wall stops at it") {
    World w(5);
    int y = w.surface_y(8, 8);
    for (int dy = 0; dy < 3; ++dy) w.set_block(10, y + dy, 8, STONE);
    Body b(w, 8.5, y + 0.01, 8.5, 0.3, 1.8);
    auto hit = b.move(3.0, 0, 0);
    CHECK(hit.x);
    CHECK(b.x == doctest::Approx(10 - 0.3));
}
