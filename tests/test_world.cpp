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

TEST_CASE("the world is 128 blocks tall with the sea at 64") {
    CHECK(HEIGHT == 128);
    CHECK(SEA_LEVEL == 64);
    World w(5);
    int lowest = HEIGHT, highest = 0;
    for (int x = -64; x < 64; x += 4)
        for (int z = -64; z < 64; z += 4) {
            int h = w.surface_y(x, z);
            lowest = std::min(lowest, h);
            highest = std::max(highest, h);
        }
    CHECK(lowest > 40);           // land and sea floor sit around the new sea level
    CHECK(highest > SEA_LEVEL);   // some dry land
    CHECK(highest < HEIGHT - 4);  // room for trees above the highest ground
    w.set_block(0, HEIGHT - 1, 0, STONE);  // the top layer can be built on
    CHECK(w.get_block(0, HEIGHT - 1, 0) == STONE);
    CHECK(w.get_block(0, HEIGHT, 0) == AIR);
}

TEST_CASE("Fast graphics draws solid leaves, Fancy draws the faces between them") {
    World w(5);
    int y = HEIGHT - 10;
    for (int dx = 0; dx < 3; ++dx) w.set_block(4 + dx, y, 4, LEAVES);  // a row of 3 leaves, high in the air
    auto leaf_quads = [&](MeshOptions o) {
        ChunkMesh m = w.build_mesh_now(0, 0, o);
        size_t quads = 0;
        for (size_t i = 0; i < m.solid.vertices.size(); i += 4)
            if (m.solid.vertices[i].y >= y - 0.01f) ++quads;
        return quads;
    };
    CHECK(leaf_quads({true, false}) == 3 * 6 - 4);  // the 2 touching pairs hide 4 faces
    CHECK(leaf_quads({true, true}) == 3 * 6);
}

TEST_CASE("smooth lighting off gives every corner of a face the same shade") {
    World w(5);
    ChunkMesh m = w.build_mesh_now(0, 0, {false, true});
    bool flat = true;
    for (size_t i = 0; i < m.solid.vertices.size(); i += 4)
        for (int c = 1; c < 4; ++c) flat &= m.solid.vertices[i + c].shade == m.solid.vertices[i].shade;
    CHECK(flat);
    ChunkMesh smooth = w.build_mesh_now(0, 0, {true, true});
    bool varied = false;
    for (size_t i = 0; i < smooth.solid.vertices.size(); i += 4)
        for (int c = 1; c < 4; ++c) varied |= smooth.solid.vertices[i + c].shade != smooth.solid.vertices[i].shade;
    CHECK(varied);
}

TEST_CASE("sneaking stops at the edge of a block instead of walking off") {
    World w(5);
    const int y = HEIGHT - 20;
    w.set_block(10, y, 10, STONE);  // one block in mid-air
    Body b(w, 10.5, y + 1, 10.5, 0.3, 1.8);
    b.on_ground = true;

    double dx = 2.0, dz = 0;
    b.clamp_to_edges(dx, dz);
    CHECK(dx > 0);
    CHECK(dx < 0.85);  // the body still overlaps the block: 10.5 + dx - 0.3 < 11
    b.move(dx, 0, dz);
    CHECK(b.x - b.half < 11);

    double back_x = -3, back_z = 3;
    b.clamp_to_edges(back_x, back_z);
    CHECK(b.x + back_x + b.half > 10);
    CHECK(b.z + back_z - b.half < 11);

    double small_x = 0.01, small_z = 0;  // a step that stays on the block is not changed
    Body c(w, 10.5, y + 1, 10.5, 0.3, 1.8);
    c.clamp_to_edges(small_x, small_z);
    CHECK(small_x == 0.01);
}
