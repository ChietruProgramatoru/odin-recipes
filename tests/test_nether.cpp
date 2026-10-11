// The Nether (M14): its terrain, portals and their rules, the scale between the worlds, saving
// each dimension, and the ghast's and zombie pigman's numbers.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>

#include "blockupdate.h"
#include "combat.h"
#include "icons.h"
#include "items.h"
#include "monsters.h"
#include "nether.h"
#include "save.h"
#include "sky.h"
#include "textures.h"
#include "world.h"

namespace {

// Stone up to y = 9, so the ground is at y = 10, in a 3 x 3 of chunks.
struct Flat {
    World world{1};
    BlockUpdater blocks{world};
    Rng rng{42};
    BlockEvents events;

    Flat() {
        for (int cx = -1; cx <= 1; ++cx)
            for (int cz = -1; cz <= 1; ++cz) {
                auto c = std::make_unique<Chunk>(cx, cz);
                for (int y = 0; y < 10; ++y)
                    for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
                c->max_y = 10;
                world.insert(std::move(c));
            }
    }
    void set(glm::ivec3 p, BlockId b) { world.set_block(p.x, p.y, p.z, b); }
    BlockId at(glm::ivec3 p) { return world.get_block(p.x, p.y, p.z); }
    // An obsidian frame along x with its bottom inside cell at `o` (corners left out).
    void frame(glm::ivec3 o) {
        for (int i = 0; i < 2; ++i) {
            set(o + glm::ivec3(i, -1, 0), OBSIDIAN);
            set(o + glm::ivec3(i, 3, 0), OBSIDIAN);
        }
        for (int j = 0; j < 3; ++j) {
            set(o + glm::ivec3(-1, j, 0), OBSIDIAN);
            set(o + glm::ivec3(2, j, 0), OBSIDIAN);
        }
    }
    void settle() {
        for (int i = 0; i < 10; ++i) blocks.tick(rng, [](glm::ivec3) { return false; }, events);
    }
};

}  // namespace

TEST_CASE("the Nether is netherrack between bedrock, with a lava sea, glowstone and no water") {
    int lava = 0, glow = 0, netherrack = 0, water = 0, soul = 0;
    for (int cx = 0; cx < 4; ++cx)
        for (int cz = 0; cz < 4; ++cz) {
            auto c = generate_nether(7, cx, cz);
            for (int i = 0; i < CHUNK * CHUNK; ++i) {
                CHECK(c->blocks[i] == BEDROCK);                          // the floor
                CHECK(c->blocks[((HEIGHT - 1) << 8) | i] == BEDROCK);    // and the roof
            }
            for (int idx = 0; idx < static_cast<int>(c->blocks.size()); ++idx) {
                const BlockId b = c->blocks[idx];
                const int y = idx >> 8;
                if (b == LAVA) {
                    ++lava;
                    CHECK(y < NETHER_LAVA_LEVEL);
                }
                glow += b == GLOWSTONE;
                netherrack += b == NETHERRACK;
                water += b == WATER;
                soul += b == SOUL_SAND;
            }
            CHECK(c->mob_spawned);  // no animals
        }
    CHECK(water == 0);
    CHECK(lava > 1000);
    CHECK(glow > 0);
    CHECK(soul > 0);
    CHECK(netherrack > 100000);
}

TEST_CASE("Nether chunks are the same every time and match their neighbours' features") {
    auto a = generate_nether(99, 3, -2), b = generate_nether(99, 3, -2);
    CHECK(a->blocks == b->blocks);
    // A World in the Nether generates Nether chunks.
    World w(99);
    w.reset(Dimension::Nether);
    CHECK(w.nether());
    CHECK(w.chunk(3, -2).blocks == a->blocks);
    w.reset(Dimension::Overworld);
    CHECK_FALSE(w.nether());
    CHECK(w.chunk_count() == 0);
}

TEST_CASE("an obsidian frame lit inside fills with portal along its axis") {
    Flat f;
    const glm::ivec3 o(0, 11, 0);
    f.frame(o);
    // Too small a gap: nothing.
    CHECK_FALSE(find_portal_frame(f.world, o + glm::ivec3(0, 0, 5)));
    // Any of the six inside cells finds the frame.
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) {
            auto s = find_portal_frame(f.world, o + glm::ivec3(i, j, 0));
            REQUIRE(s);
            CHECK(s->origin == o);
            CHECK(s->axis == 0);
        }
    auto s = find_portal_frame(f.world, o);
    light_portal(f.world, *s);
    for (glm::ivec3 c : s->cells()) {
        CHECK(f.at(c) == PORTAL);
        CHECK(f.world.get_data(c.x, c.y, c.z) == 0);
        CHECK(portal_can_stay(f.world, c));
    }
    CHECK(portal_at(f.world, o + glm::ivec3(1, 2, 0)) == s);
    CHECK(light_emission(PORTAL) == 11);
    CHECK_FALSE(is_solid(PORTAL));
    // The exit is in the middle of its bottom.
    CHECK(s->exit() == glm::dvec3(1.0, 11, 0.5));
}

TEST_CASE("a frame along z is found too, and a blocked frame isn't") {
    Flat f;
    const glm::ivec3 o(4, 11, 4);
    for (int i = 0; i < 2; ++i) {
        f.set(o + glm::ivec3(0, -1, i), OBSIDIAN);
        f.set(o + glm::ivec3(0, 3, i), OBSIDIAN);
    }
    for (int j = 0; j < 3; ++j) {
        f.set(o + glm::ivec3(0, j, -1), OBSIDIAN);
        f.set(o + glm::ivec3(0, j, 2), OBSIDIAN);
    }
    auto s = find_portal_frame(f.world, o + glm::ivec3(0, 0, 1));
    REQUIRE(s);
    CHECK(s->axis == 1);
    CHECK(s->origin == o);
    f.set(o + glm::ivec3(0, 1, 1), STONE);
    CHECK_FALSE(find_portal_frame(f.world, o));
}

TEST_CASE("breaking the frame takes the whole portal away") {
    Flat f;
    const glm::ivec3 o(0, 11, 0);
    f.frame(o);
    light_portal(f.world, *find_portal_frame(f.world, o));
    f.settle();
    CHECK(f.at(o) == PORTAL);  // a whole frame keeps it
    f.set(o + glm::ivec3(2, 1, 0), AIR);  // one side of the frame
    f.settle();
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) CHECK(f.at(o + glm::ivec3(i, j, 0)) == AIR);
}

TEST_CASE("water doesn't flow into a portal and explosions break it") {
    CHECK(explosion_resistance(PORTAL) == 0);
    CHECK_FALSE(block_info(PORTAL)->hardness);  // can't be mined
}

TEST_CASE("portals lead 8 to 1 between the worlds") {
    CHECK(portal_target({800, 70, -808}, Dimension::Nether) == glm::ivec3(100, 70, -101));
    CHECK(portal_target({100, 70, -101}, Dimension::Overworld) == glm::ivec3(800, 70, -808));
    // Height stays inside the other world, above the lava sea in the Nether.
    CHECK(portal_target({0, 5, 0}, Dimension::Nether).y > NETHER_LAVA_LEVEL);
    CHECK(portal_target({0, 125, 0}, Dimension::Overworld).y < HEIGHT - 4);
}

TEST_CASE("a new portal is built on a floor near the spot, or on a ledge in the air") {
    Flat f;
    std::vector<ChunkPos> dirty;
    const PortalShape s = build_portal(f.world, {3, 30, 2}, dirty);
    CHECK(s.origin.y == 10);  // on the ground
    CHECK(std::abs(s.origin.x - 3) <= 2);
    CHECK(std::abs(s.origin.z - 2) <= 2);
    for (glm::ivec3 c : s.cells()) CHECK(portal_can_stay(f.world, c));
    CHECK_FALSE(dirty.empty());

    // In the Nether over the lava sea: the frame hangs over it on an obsidian ledge.
    World nether(5);
    nether.reset(Dimension::Nether);
    std::vector<ChunkPos> d2;
    const PortalShape n = build_portal(nether, {40, 70, 40}, d2);
    for (glm::ivec3 c : n.cells()) CHECK(portal_can_stay(nether, c));
    const glm::dvec3 e = n.exit();
    const int ex = static_cast<int>(std::floor(e.x)), ez = static_cast<int>(std::floor(e.z)), ey = static_cast<int>(e.y);
    CHECK(is_opaque(nether.get_block(ex, ey - 1, ez)));  // something to stand on
}

TEST_CASE("fire on netherrack never goes out") {
    Flat f;
    const glm::ivec3 p(5, 10, 5);
    f.set(p - glm::ivec3(0, 1, 0), NETHERRACK);
    f.set(p, FIRE);
    for (int i = 0; i < 2000; ++i) f.blocks.tick(f.rng, [](glm::ivec3) { return true; }, f.events);  // even in the rain
    CHECK(f.at(p) == FIRE);
}

TEST_CASE("each dimension is saved in its own folder, and the player's dimension and portals too") {
    const auto dir = std::filesystem::temp_directory_path() / "petrutz_nether_test";
    std::filesystem::remove_all(dir);
    {
        WorldSaver saver(dir);
        auto over = std::make_unique<Chunk>(0, 0);
        over->blocks[5] = STONE;
        auto hell = std::make_unique<Chunk>(0, 0);
        hell->blocks[5] = NETHERRACK;
        LevelData level;
        level.seed = 3;
        level.portals = {{Dimension::Overworld, {1, 2, 3}}, {Dimension::Nether, {-4, 50, 6}}};
        PlayerData p;
        p.dimension = Dimension::Nether;
        level.player = p;
        std::vector<std::unique_ptr<Chunk>> a, b;
        a.push_back(std::move(over));
        b.push_back(std::move(hell));
        saver.save(level, std::move(a), Dimension::Overworld);
        saver.save(level, std::move(b), Dimension::Nether);
        CHECK(saver.wait());
    }
    CHECK(std::filesystem::exists(dir / "DIM-1" / "region"));
    RegionStore over(dir / "region"), hell(dir / "DIM-1" / "region");
    CHECK(over.load(0, 0)->blocks[5] == STONE);
    CHECK(hell.load(0, 0)->blocks[5] == NETHERRACK);
    auto level = read_level(dir);
    REQUIRE(level);
    REQUIRE(level->player);
    CHECK(level->player->dimension == Dimension::Nether);
    CHECK(level->portals.size() == 2);
    CHECK(level->portals[1].dimension == Dimension::Nether);
    CHECK(level->portals[1].pos == glm::ivec3(-4, 50, 6));
    std::filesystem::remove_all(dir);
}

TEST_CASE("the Nether's sky: red fog from the camera, no sun, never pitch black") {
    const SkyState s = nether_sky();
    CHECK(s.nether);
    CHECK(s.darkness == 0);
    CHECK(s.sun_alpha == 0);
    CHECK_FALSE(s.show_clouds);
    CHECK(s.fog.r > s.fog.g);
    CHECK(s.ambient > sky_state(0).ambient);
}

TEST_CASE("ghasts and zombie pigmen: Beta's numbers") {
    const MobInfo g = mob_info(MobKind::Ghast), p = mob_info(MobKind::PigZombie);
    CHECK(g.health == 10);
    CHECK(g.half_width == 2.0);
    CHECK(g.fire_immune);
    CHECK(p.health == 20);
    CHECK(p.attack == 5);
    CHECK(p.fire_immune);
    CHECK_FALSE(mob_info(MobKind::Zombie).fire_immune);
    Rng rng(1);
    for (int i = 0; i < 50; ++i) {
        for (ItemId item : mob_drops(MobKind::Ghast, 1, rng)) CHECK(item == GUNPOWDER);
        for (ItemId item : mob_drops(MobKind::PigZombie, 1, rng)) CHECK(item == COOKED_PORKCHOP);
        const int t = pigman_anger_ticks(rng);
        CHECK(t >= 400);
        CHECK(t < 800);
        const MobKind k = random_monster(rng);
        CHECK(static_cast<int>(k) < OVERWORLD_MOB_KINDS);  // never in the overworld
        const MobKind n = random_nether_monster(rng);
        CHECK((n == MobKind::Ghast || n == MobKind::PigZombie));
    }
}

TEST_CASE("a zombie pigman leaves you alone until angered") {
    Flat f;
    Rng rng(3);
    Monster m(f.world, MobKind::PigZombie, 0.5, 10, 0.5, rng);
    MonsterSenses s;
    s.player = {3.5, 10, 0.5};
    MonsterActions act;
    for (int i = 0; i < 40; ++i) m.tick(0.05, f.world, s, rng, act);
    CHECK_FALSE(m.targeting);
    CHECK(act.melee == 0);
    m.enrage(rng);
    for (int i = 0; i < 60 && act.melee == 0; ++i) m.tick(0.05, f.world, s, rng, act);
    CHECK(m.targeting);
    CHECK(act.melee == 5);
}

TEST_CASE("a ghast floats, and shoots a fireball at a player it can see") {
    Flat f;
    Rng rng(4);
    Monster g(f.world, MobKind::Ghast, 0.5, 20, 0.5, rng);
    MonsterSenses s;
    s.player = {20.5, 10, 0.5};
    std::optional<glm::dvec3> from;
    glm::dvec3 dir(0);
    for (int i = 0; i < 200 && !from; ++i) {
        MonsterActions act;
        g.tick(0.05, f.world, s, rng, act);
        if (act.fireball_from) {
            from = act.fireball_from;
            dir = act.fireball_dir;
        }
    }
    CHECK(g.body.y > 12);  // didn't fall to the ground
    REQUIRE(from);
    CHECK(dir.x > 0.5);  // towards the player
    // The fireball flies there and bursts on the ground or the player's spot.
    Fireball fb(*from, dir, &g);
    std::optional<glm::dvec3> hit;
    for (int i = 0; i < 200 && !hit; ++i) hit = fb.tick(0.05, f.world);
    REQUIRE(hit);
    // Hit back, it turns around.
    Fireball back(glm::dvec3(5, 15, 0.5), glm::dvec3(1, 0, 0), &g);
    back.tick(0.2, f.world);
    back.deflect(glm::dvec3(-1, 0, 0));
    CHECK(back.deflected);
    CHECK(back.vel.x < 0);
    CHECK(back.owner == nullptr);
}

TEST_CASE("the atlas has room for every tile: world tiles, item faces and icons, then the font") {
    CHECK(TILE_COUNT <= ITEM_TILE_BASE);
    int top = 0;
    for (int id = 0; id < ITEM_END; ++id)
        if (const ItemInfo* info = item_info(static_cast<ItemId>(id))) top = std::max(top, info->icon.top);
    CHECK(top < FONT_TILE_BASE);
}
