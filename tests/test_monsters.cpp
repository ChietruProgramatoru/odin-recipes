// Monsters and combat: difficulty, being hurt, mob stats and drops, spawning rules, burning in
// daylight, mob spawners, explosions and how monsters fight.
#include <doctest/doctest.h>

#include <map>

#include "combat.h"
#include "monsters.h"
#include "survival.h"
#include "world.h"

namespace {

// A stone floor at y 99 with open air above it, around (8, 100, 8).
void flat_arena(World& w) {
    for (int x = -8; x < 24; ++x)
        for (int z = -8; z < 24; ++z) {
            for (int y = 100; y < 112; ++y) w.set_block(x, y, z, AIR);
            w.set_block(x, 99, z, STONE);
        }
}

// Run a monster for `seconds` with the player standing at `player`; collects what it did.
MonsterActions run(Monster& m, World& w, glm::dvec3 player, double seconds, Rng& rng, int sky_darkness = 11) {
    MonsterSenses s;
    s.player = player;
    s.sky_darkness = sky_darkness;
    MonsterActions all;
    for (double t = 0; t < seconds; t += 0.05) {
        MonsterActions out;
        m.tick(0.05, w, s, rng, out);
        if (out.melee) all.melee = out.melee;
        if (out.arrow_from) all.arrow_from = out.arrow_from, all.arrow_vel = out.arrow_vel;
        if (out.explode) {
            all.explode = true;
            break;
        }
    }
    return all;
}

}  // namespace

TEST_CASE("difficulty scales monster damage like Beta") {
    CHECK(scale_monster_damage(6, PEACEFUL) == 0);
    CHECK(scale_monster_damage(6, EASY) == 3);
    CHECK(scale_monster_damage(1, EASY) == 1);
    CHECK(scale_monster_damage(6, NORMAL) == 6);
    CHECK(scale_monster_damage(6, HARD) == 9);
    CHECK(std::string(difficulty_name(NORMAL)) == "Normal");
    CHECK(std::string(difficulty_name(99)) == "Hard");
}

TEST_CASE("after a hit there is half a second when only harder hits count, and only the difference") {
    HurtCooldown c;
    bool fresh = false;
    CHECK(c.take(4, fresh) == 4);
    CHECK(fresh);
    CHECK(c.take(3, fresh) == 0);
    CHECK(c.take(6, fresh) == 2);
    CHECK_FALSE(fresh);
    c.tick(0.6);
    CHECK_FALSE(c.invulnerable());
    CHECK(c.take(3, fresh) == 3);
    CHECK(fresh);
}

TEST_CASE("monster stats, drops and slimes splitting") {
    CHECK(mob_info(MobKind::Zombie).health == 20);
    CHECK(mob_info(MobKind::Zombie).attack == 5);
    CHECK(mob_info(MobKind::Zombie).burns_in_daylight);
    CHECK(mob_info(MobKind::Skeleton).burns_in_daylight);
    CHECK_FALSE(mob_info(MobKind::Creeper).burns_in_daylight);
    CHECK(mob_info(MobKind::Spider).health == 16);
    CHECK(mob_info(MobKind::Slime, 4).health == 16);
    CHECK(mob_info(MobKind::Slime, 1).health == 1);
    CHECK(mob_info(MobKind::Slime, 1).attack == 0);  // the smallest slimes are harmless
    CHECK(mob_info(MobKind::Slime, 4).half_width > mob_info(MobKind::Slime, 2).half_width);

    Rng rng(5);
    std::map<ItemId, int> seen;
    for (int i = 0; i < 300; ++i) {
        for (ItemId item : mob_drops(MobKind::Skeleton, 1, rng)) ++seen[item];
        for (ItemId item : mob_drops(MobKind::Zombie, 1, rng)) ++seen[item];
        for (ItemId item : mob_drops(MobKind::Creeper, 1, rng)) ++seen[item];
        for (ItemId item : mob_drops(MobKind::Spider, 1, rng)) ++seen[item];
        CHECK(mob_drops(MobKind::Slime, 2, rng).empty());
        CHECK(mob_drops(MobKind::Zombie, 1, rng).size() <= 2);
    }
    CHECK(seen[ARROW] > 0);
    CHECK(seen[BONE] > 0);
    CHECK(seen[FEATHER] > 0);
    CHECK(seen[GUNPOWDER] > 0);
    CHECK(seen[STRING] > 0);

    CHECK(slime_split_count(1, rng) == 0);
    for (int i = 0; i < 50; ++i) {
        const int n = slime_split_count(4, rng);
        CHECK(n >= 2);
        CHECK(n <= 4);
    }
}

TEST_CASE("light: spiders hunt in the dark, the undead burn in sunshine") {
    CHECK(light_brightness(15) == doctest::Approx(1.0));
    CHECK(light_brightness(0) == doctest::Approx(0.05));
    CHECK(spider_hostile(0));
    CHECK(spider_hostile(7));
    CHECK_FALSE(spider_hostile(15));

    Rng rng(3);
    int burnt = 0;
    for (int i = 0; i < 400; ++i) {
        burnt += catches_fire_in_daylight(15, true, 0, rng);
        CHECK_FALSE(catches_fire_in_daylight(15, false, 0, rng));  // under a roof
        CHECK_FALSE(catches_fire_in_daylight(15, true, 11, rng));  // at night
        CHECK_FALSE(catches_fire_in_daylight(5, true, 0, rng));    // in the shade
    }
    CHECK(burnt > 0);
}

TEST_CASE("monsters spawn only in the dark, up to a cap") {
    Rng rng(8);
    int lit = 0, dark = 0, open_sky = 0;
    for (int i = 0; i < 1000; ++i) {
        lit += monster_spawn_light_ok(0, 12, rng);
        dark += monster_spawn_light_ok(0, 0, rng);
        open_sky += monster_spawn_light_ok(15, 0, rng);
    }
    CHECK(lit == 0);
    CHECK(dark == 1000);
    CHECK(open_sky > 300);  // night under the open sky: about half the tries
    CHECK(open_sky < 700);
    CHECK(monster_cap(256) == 70);
    CHECK(monster_cap(289) == 79);

    int slime_chunks = 0;
    for (int cx = -50; cx < 50; ++cx)
        for (int cz = -50; cz < 50; ++cz) slime_chunks += is_slime_chunk(7, cx, cz);
    CHECK(slime_chunks > 800);
    CHECK(slime_chunks < 1200);
    CHECK(is_slime_chunk(7, 3, 4) == is_slime_chunk(7, 3, 4));

    int jockeys = 0;
    std::map<int, int> sizes;
    for (int i = 0; i < 10000; ++i) {
        jockeys += spawns_as_jockey(rng);
        ++sizes[random_slime_size(rng)];
    }
    CHECK(jockeys > 50);
    CHECK(jockeys < 160);
    CHECK(sizes.size() == 3);
    CHECK(sizes.count(1));
    CHECK(sizes.count(2));
    CHECK(sizes.count(4));
}

TEST_CASE("mob spawners pick zombies half the time, and wait 200-799 ticks") {
    std::map<MobKind, int> kinds;
    for (int x = 0; x < 40; ++x)
        for (int z = 0; z < 40; ++z) ++kinds[spawner_mob(11, {x * 17, 30, z * 13})];
    CHECK(kinds.size() == 3);
    CHECK(kinds[MobKind::Zombie] > kinds[MobKind::Skeleton]);
    CHECK(kinds[MobKind::Zombie] > kinds[MobKind::Spider]);
    CHECK(spawner_mob(11, {5, 30, 9}) == spawner_mob(11, {5, 30, 9}));
    Rng rng(1);
    for (int i = 0; i < 100; ++i) {
        const int d = spawner_delay(rng);
        CHECK(d >= 200);
        CHECK(d < 800);
    }
}

TEST_CASE("explosions break soft blocks, not bedrock, and hurt less behind walls") {
    World w(3);
    flat_arena(w);
    for (int x = 6; x <= 10; ++x)
        for (int y = 100; y <= 104; ++y)
            for (int z = 6; z <= 10; ++z) w.set_block(x, y, z, DIRT);
    Rng rng(4);
    auto broken = explosion_blocks(w, {8.5, 102.5, 8.5}, TNT_POWER, rng);
    CHECK(broken.size() > 50);
    bool centre = false;
    for (auto b : broken) {
        CHECK(w.get_block(b.x, b.y, b.z) != AIR);
        centre |= b == glm::ivec3(8, 102, 8);
    }
    CHECK(centre);

    for (int x = 6; x <= 10; ++x)
        for (int y = 100; y <= 104; ++y)
            for (int z = 6; z <= 10; ++z) w.set_block(x, y, z, BEDROCK);
    w.set_block(8, 102, 8, AIR);
    CHECK(explosion_blocks(w, {8.5, 102.5, 8.5}, TNT_POWER, rng).empty());
    CHECK(explosion_resistance(WATER) == 100);
    CHECK(explosion_resistance(STONE) > explosion_resistance(DIRT));

    // In the open the blast sees all of a box; behind a wall none of it.
    flat_arena(w);
    CHECK(explosion_exposure(w, {8, 100, 8}, {11.7, 100, 7.7}, {12.3, 101.8, 8.3}) == doctest::Approx(1.0));
    for (int y = 100; y < 104; ++y)
        for (int z = 5; z < 12; ++z) w.set_block(10, y, z, STONE);
    CHECK(explosion_exposure(w, {8, 100.5, 8}, {11.7, 100, 7.7}, {12.3, 101.8, 8.3}) == doctest::Approx(0.0));

    CHECK(explosion_damage(CREEPER_POWER, 0, 1) == 49);
    CHECK(explosion_damage(CREEPER_POWER, 3, 1) == 19);
    CHECK(explosion_damage(CREEPER_POWER, 7, 1) == 0);
    CHECK(explosion_damage(CREEPER_POWER, 0, 0) == 1);
    CHECK(explosion_push(CREEPER_POWER, 3, 1) == doctest::Approx(0.5));
}

TEST_CASE("a creeper next to the player hisses for a second and a half, then explodes") {
    World w(3);
    flat_arena(w);
    Rng rng(2);
    Monster c(w, MobKind::Creeper, 8.5, 100, 8.5, rng);
    MonsterActions far = run(c, w, {20.5, 100, 8.5}, 3, rng);
    CHECK_FALSE(far.explode);
    CHECK(c.fuse == 0);

    Monster c2(w, MobKind::Creeper, 8.5, 100, 8.5, rng);
    MonsterActions once = run(c2, w, {10.5, 100, 8.5}, 1.2, rng);
    CHECK_FALSE(once.explode);
    CHECK(c2.fuse > 15);
    CHECK(run(c2, w, {10.5, 100, 8.5}, 1.0, rng).explode);
}

TEST_CASE("a zombie hits the player next to it, a skeleton shoots from afar") {
    World w(3);
    flat_arena(w);
    Rng rng(2);
    Monster z(w, MobKind::Zombie, 8.5, 100, 8.5, rng);
    CHECK(run(z, w, {9.7, 100, 8.5}, 0.5, rng).melee == 5);

    Monster s(w, MobKind::Skeleton, 8.5, 100, 8.5, rng);
    MonsterActions shot = run(s, w, {16.5, 100, 8.5}, 2, rng);
    REQUIRE(shot.arrow_from);
    CHECK(shot.arrow_vel.x > 0);  // towards the player
    CHECK(glm::length(shot.arrow_vel) == doctest::Approx(SKELETON_ARROW_SPEED));
}

TEST_CASE("slimes hop; hurting a monster flashes it and knocks it back; it dies at no health") {
    World w(3);
    flat_arena(w);
    Rng rng(6);
    Monster slime(w, MobKind::Slime, 8.5, 100, 8.5, rng, 2);
    double top = 100;
    MonsterSenses s;
    s.player = {12.5, 100, 8.5};
    for (int i = 0; i < 60; ++i) {
        MonsterActions out;
        slime.tick(0.05, w, s, rng, out);
        top = std::max(top, slime.body.y);
    }
    CHECK(top > 100.3);

    Monster z(w, MobKind::Zombie, 8.5, 100, 8.5, rng);
    CHECK(z.hurt(7, 6.5, 8.5) == 7);
    CHECK(z.flash > 0);
    CHECK(z.knock_x > 0);  // pushed away from the hit
    CHECK(z.hurt(7, 6.5, 8.5) == 0);  // still invulnerable
    z.cooldown.tick(1.0);
    CHECK(z.hurt(20, 6.5, 8.5) == 20);
    CHECK_FALSE(z.alive());
}

TEST_CASE("Peaceful: health comes back, hunger stands still") {
    Vitals v;
    v.peaceful = true;
    v.health = 10;
    v.hunger = 10;
    for (int i = 0; i < 245; ++i) v.tick(0.05);  // a little over 12 seconds
    CHECK(v.health == 11);
    v.exhaust(40);
    CHECK(v.hunger == 10);
    v.hunger = 0;
    for (int i = 0; i < 400; ++i) v.tick(0.05);
    CHECK(v.health > 11);  // no starving

    Vitals normal;
    normal.health = 10;
    normal.hunger = 10;
    for (int i = 0; i < 245; ++i) normal.tick(0.05);
    CHECK(normal.health == 10);
}
