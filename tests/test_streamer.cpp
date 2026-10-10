// Background chunk generation and meshing.
#include <doctest/doctest.h>

#include <set>
#include <utility>

#include "streamer.h"

TEST_CASE("streaming meshes every chunk in range, and the meshes match meshing on the main thread") {
    World world(7);
    World reference(7);
    ChunkStreamer streamer(world, 3);
    streamer.finish(8, 8, 3);
    auto ready = streamer.take_ready();
    auto wanted = ChunkStreamer::wanted(8, 8, 3);
    CHECK(ready.size() == wanted.size());
    std::set<std::pair<int, int>> seen;
    for (const ReadyMesh& r : ready) {
        seen.insert({r.pos.cx, r.pos.cz});
        ChunkMesh expected = reference.build_mesh_now(r.pos.cx, r.pos.cz);
        CHECK(r.mesh.solid.vertices.size() == expected.solid.vertices.size());
        CHECK(r.mesh.water.vertices.size() == expected.water.vertices.size());
    }
    for (ChunkPos k : wanted) CHECK(seen.count({k.cx, k.cz}) == 1);
}

TEST_CASE("wanted chunks are sorted nearest first") {
    auto keys = ChunkStreamer::wanted(-5, 40, 4);
    CHECK(keys.front() == ChunkPos{-1, 2});
    auto d = [](ChunkPos k) { return (k.cx + 1) * (k.cx + 1) + (k.cz - 2) * (k.cz - 2); };
    for (size_t i = 1; i < keys.size(); ++i) CHECK(d(keys[i - 1]) <= d(keys[i]));
}

TEST_CASE("a rebuild after an edit replaces the background mesh") {
    World world(7);
    ChunkStreamer streamer(world, 2);
    streamer.finish(8, 8, 1);
    streamer.take_ready();
    int y = world.surface_y(8, 8);
    world.set_block(8, y, 8, BRICK);
    ReadyMesh fresh = streamer.rebuild_now({0, 0});
    CHECK(streamer.has_mesh({0, 0}));
    CHECK(fresh.mesh.solid.vertices.size() == world.build_mesh_now(0, 0).solid.vertices.size());
    streamer.forget({0, 0});
    CHECK_FALSE(streamer.has_mesh({0, 0}));
    streamer.finish(8, 8, 1);  // comes back when in range again
    auto again = streamer.take_ready();
    REQUIRE(again.size() == 1);
    CHECK(again[0].pos == ChunkPos{0, 0});
}
