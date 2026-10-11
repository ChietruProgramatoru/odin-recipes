// Animals (M9): the Beta rules for drops, wool, eggs and taming, and how pigs, sheep, chickens,
// squid and wolves behave. Cows are in test_misc.cpp.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>

#include "animals.h"
#include "items.h"

namespace {

// A stone floor high in the sky (no terrain, water or trees in the way), with its top at y.
constexpr int FLOOR = HEIGHT - 20;
void make_floor(World& w, int x0, int z0, int x1, int z1) {
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) w.set_block(x, FLOOR - 1, z, STONE);
}

AnimalSenses player_at(double x, double y, double z) {
    AnimalSenses s;
    s.player = {x, y, z};
    return s;
}

// Run an animal for `seconds` at 60 frames a second, collecting what it did.
AnimalActions run(Animal& a, const AnimalSenses& senses, Rng& rng, double seconds) {
    AnimalActions all;
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i) {
        AnimalActions act;
        a.tick(1.0 / 60, senses, rng, act);
        all.bite_player += act.bite_player;
        all.bite_prey += act.bite_prey;
        all.lay_egg |= act.lay_egg;
        all.teleport |= act.teleport;
    }
    return all;
}

}  // namespace

TEST_CASE("sheep are mostly white, with Beta's odds for grey, black, brown and pink") {
    Rng rng(1);
    std::map<int, int> seen;
    const int n = 50000;
    for (int i = 0; i < n; ++i) ++seen[natural_sheep_color(rng)];
    for (const auto& [color, count] : seen) CHECK((color == 0 || color == 6 || color == 7 || color == 8 || color == 12 || color == 15));
    CHECK(seen[0] / static_cast<double>(n) == doctest::Approx(0.8184).epsilon(0.02));
    CHECK(seen[15] / static_cast<double>(n) == doctest::Approx(0.05).epsilon(0.1));
    CHECK(seen[12] / static_cast<double>(n) == doctest::Approx(0.03).epsilon(0.15));
    CHECK(seen[6] > 20);  // pink is very rare: about 1 sheep in 600
    CHECK(seen[6] < 160);
}

TEST_CASE("dyes colour wool the Beta way round, and shears give 1 to 3") {
    CHECK(dye_wool_color(INK_SAC) == 15);
    CHECK(wool_item(*dye_wool_color(INK_SAC)) == BLACK_WOOL);
    CHECK(wool_item(*dye_wool_color(ROSE_RED)) == RED_WOOL);
    CHECK(wool_item(*dye_wool_color(LAPIS_LAZULI)) == BLUE_WOOL);
    CHECK(wool_item(*dye_wool_color(DANDELION_YELLOW)) == YELLOW_WOOL);
    CHECK(wool_item(*dye_wool_color(BONE_MEAL)) == WHITE_WOOL);
    CHECK_FALSE(dye_wool_color(STICK));
    CHECK_FALSE(dye_wool_color(BONE));
    Rng rng(2);
    for (int i = 0; i < 100; ++i) {
        const int n = shear_count(rng);
        CHECK(n >= 1);
        CHECK(n <= 3);
    }
}

TEST_CASE("eggs: laid every 5 to 10 minutes, one in 8 hatches, rarely four chicks") {
    Rng rng(3);
    int hatched = 0, fours = 0;
    const int n = 64000;
    for (int i = 0; i < n; ++i) {
        const int d = egg_delay_ticks(rng);
        CHECK(d >= 6000);
        CHECK(d < 12000);
        const int c = chicks_from_egg(rng);
        CHECK((c == 0 || c == 1 || c == 4));
        hatched += c > 0;
        fours += c == 4;
    }
    CHECK(hatched / static_cast<double>(n) == doctest::Approx(1.0 / 8).epsilon(0.05));
    CHECK(fours / static_cast<double>(hatched) == doctest::Approx(1.0 / 32).epsilon(0.3));
}

TEST_CASE("a bone tames a wolf one time in three") {
    Rng rng(4);
    int tamed = 0;
    for (int i = 0; i < 30000; ++i) tamed += tame_succeeds(rng);
    CHECK(tamed / 30000.0 == doctest::Approx(1.0 / 3).epsilon(0.03));
    CHECK(wolf_food_heal(RAW_PORKCHOP) > 0);
    CHECK(wolf_food_heal(COOKED_PORKCHOP) > wolf_food_heal(RAW_PORKCHOP));
    CHECK(wolf_food_heal(BREAD) == 0);
    CHECK(wolf_food_heal(BONE) == 0);
}

TEST_CASE("what each animal drops") {
    Rng rng(5);
    for (int i = 0; i < 200; ++i) {
        auto pig = animal_drops(AnimalKind::Pig, {}, rng);
        CHECK(pig.size() <= 2);
        CHECK(std::all_of(pig.begin(), pig.end(), [](ItemId d) { return d == RAW_PORKCHOP; }));
        auto burnt = animal_drops(AnimalKind::Pig, {.burning = true}, rng);
        CHECK(std::all_of(burnt.begin(), burnt.end(), [](ItemId d) { return d == COOKED_PORKCHOP; }));
        auto saddled = animal_drops(AnimalKind::Pig, {.saddled = true}, rng);
        CHECK(std::count(saddled.begin(), saddled.end(), SADDLE) == 1);

        CHECK(animal_drops(AnimalKind::Sheep, {.wool = 14}, rng) == std::vector<ItemId>{RED_WOOL});
        CHECK(animal_drops(AnimalKind::Sheep, {.sheared = true, .wool = 14}, rng).empty());

        auto chicken = animal_drops(AnimalKind::Chicken, {}, rng);
        CHECK(chicken.size() <= 2);
        CHECK(std::all_of(chicken.begin(), chicken.end(), [](ItemId d) { return d == FEATHER; }));
        auto squid = animal_drops(AnimalKind::Squid, {}, rng);
        CHECK(squid.size() >= 1);
        CHECK(squid.size() <= 3);
        CHECK(std::all_of(squid.begin(), squid.end(), [](ItemId d) { return d == INK_SAC; }));
        CHECK(animal_drops(AnimalKind::Wolf, {}, rng).empty());
        auto cow = animal_drops(AnimalKind::Cow, {.burning = true}, rng);
        CHECK(std::count(cow.begin(), cow.end(), STEAK) >= 1);
    }
}

TEST_CASE("which animals live where") {
    Rng rng(6);
    for (int i = 0; i < 100; ++i) CHECK_FALSE(pick_land_animal(Biome::DESERT, rng));
    std::map<AnimalKind, int> plains, forest;
    for (int i = 0; i < 20000; ++i) {
        ++plains[*pick_land_animal(Biome::PLAINS, rng)];
        ++forest[*pick_land_animal(Biome::FOREST, rng)];
    }
    CHECK(plains[AnimalKind::Wolf] == 0);
    CHECK(plains[AnimalKind::Squid] == 0);
    CHECK(forest[AnimalKind::Wolf] > 0);
    CHECK(plains[AnimalKind::Sheep] > plains[AnimalKind::Pig]);
    CHECK(plains[AnimalKind::Pig] > plains[AnimalKind::Cow]);
    CHECK(plains[AnimalKind::Sheep] / 20000.0 == doctest::Approx(12.0 / 40).epsilon(0.05));
}

TEST_CASE("a chicken lays an egg when its time comes, and flutters down slowly") {
    World w(5);
    make_floor(w, 0, 0, 16, 16);
    Rng rng(7);
    Animal hen(w, AnimalKind::Chicken, 8.5, FLOOR + 20, 8.5, rng);
    CHECK(hen.egg_timer >= 6000);
    double lowest_vy = 0;
    AnimalActions act;
    for (int i = 0; i < 120; ++i) {
        hen.tick(1.0 / 60, player_at(100, FLOOR, 100), rng, act);
        lowest_vy = std::min(lowest_vy, hen.body.vy);
    }
    CHECK(lowest_vy >= -3.01);
    CHECK_FALSE(act.lay_egg);
    hen.egg_timer = 1;
    CHECK(run(hen, player_at(100, FLOOR, 100), rng, 0.1).lay_egg);
    CHECK(hen.egg_timer >= 6000);
}

TEST_CASE("wolves: tamed ones sit, follow and jump to their owner; wild ones bite when angry") {
    World w(5);
    make_floor(w, -8, -8, 40, 40);
    Rng rng(8);
    Animal wolf(w, AnimalKind::Wolf, 4.5, FLOOR, 4.5, rng);
    CHECK(wolf.health == 8);
    wolf.tame();
    CHECK(wolf.tamed);
    CHECK(wolf.sitting);
    CHECK(wolf.health == TAMED_WOLF_HEALTH);

    // Sitting: it stays put however far its owner goes.
    run(wolf, player_at(12.5, FLOOR, 4.5), rng, 3);
    CHECK(std::hypot(wolf.body.x - 4.5, wolf.body.z - 4.5) < 0.2);
    // Standing: it comes after its owner.
    wolf.sitting = false;
    run(wolf, player_at(12.5, FLOOR, 4.5), rng, 4);
    CHECK(std::hypot(wolf.body.x - 12.5, wolf.body.z - 4.5) <= WOLF_FOLLOW_DISTANCE + 0.5);
    // Far away: it asks to be put next to them.
    CHECK(run(wolf, player_at(36.5, FLOOR, 36.5), rng, 0.1).teleport);
    // A hit makes a sitting wolf stand up.
    wolf.sitting = true;
    wolf.hurt(1, 0, 0);
    CHECK_FALSE(wolf.sitting);

    // A wild wolf only bites when angry, and never on Peaceful.
    Animal wild(w, AnimalKind::Wolf, 20.5, FLOOR, 20.5, rng);
    CHECK(run(wild, player_at(21.5, FLOOR, 20.5), rng, 3).bite_player == 0);
    wild.angry = true;
    AnimalSenses peaceful = player_at(21.5, FLOOR, 20.5);
    peaceful.peaceful = true;
    CHECK(run(wild, peaceful, rng, 3).bite_player == 0);
    const int bites = run(wild, player_at(wild.body.x + 1, FLOOR, wild.body.z), rng, 3).bite_player;
    CHECK(bites >= WILD_WOLF_BITE * 2);
    CHECK(bites % WILD_WOLF_BITE == 0);

    // It runs after its prey and bites that instead.
    Animal hunter(w, AnimalKind::Wolf, 30.5, FLOOR, 10.5, rng);
    AnimalSenses s = player_at(0.5, FLOOR, 0.5);
    s.prey = glm::dvec3(30.5, FLOOR, 16.5);
    s.prey_half = 0.45;
    const AnimalActions hunt = run(hunter, s, rng, 4);
    CHECK(hunter.body.z > 13);
    CHECK(hunt.bite_prey > 0);
}

TEST_CASE("a squid swims about in the water and stays in it") {
    World w(5);
    // A tank of water in the sky, 6 x 4 x 6, walled with glass.
    const int y0 = FLOOR;
    for (int x = 0; x <= 7; ++x)
        for (int z = 0; z <= 7; ++z)
            for (int y = y0 - 1; y <= y0 + 5; ++y) {
                const bool wall = x == 0 || x == 7 || z == 0 || z == 7 || y == y0 - 1;
                w.set_block(x, y, z, wall ? GLASS : (y < y0 + 4 ? WATER : AIR));
            }
    Rng rng(9);
    Animal squid(w, AnimalKind::Squid, 3.5, y0 + 1, 3.5, rng);
    const double x0 = squid.body.x, z0 = squid.body.z;
    double moved = 0;
    for (int i = 0; i < 600; ++i) {
        AnimalActions act;
        squid.tick(1.0 / 60, player_at(50, y0, 50), rng, act);
        moved = std::max(moved, std::hypot(squid.body.x - x0, squid.body.z - z0));
        CHECK_FALSE(squid.body.collides());
    }
    CHECK(moved > 0.5);
    CHECK(squid.body.check_water());
}

TEST_CASE("a burning animal loses health until it dies or finds water") {
    World w(5);
    make_floor(w, 0, 0, 16, 16);
    Rng rng(10);
    Animal pig(w, AnimalKind::Pig, 8.5, FLOOR, 8.5, rng);
    pig.burning = 3.5;
    run(pig, player_at(100, FLOOR, 100), rng, 4);
    CHECK(pig.health == 10 - 4);
    CHECK(pig.burning <= 0);
    CHECK(pig.loot_state().burning == false);
    pig.burning = 100;
    run(pig, player_at(100, FLOOR, 100), rng, 12);
    CHECK_FALSE(pig.alive());
    CHECK(pig.loot_state().burning);
}

TEST_CASE("models exist for every animal and every wool colour") {
    auto models = build_models();
    for (int m : {MODEL_PIG_BODY, MODEL_PIG_HEAD, MODEL_SADDLE, MODEL_SHEEP_BODY, MODEL_CHICKEN_HEAD, MODEL_SQUID_BODY,
                  MODEL_SQUID_TENTACLE, MODEL_WOLF_HEAD, MODEL_WOLF_HEAD_ANGRY, MODEL_WOLF_TAIL})
        CHECK(models.count(m));
    for (int c = 0; c < 16; ++c) {
        CHECK(models.count(MODEL_WOOL_BODY + c));
        CHECK(models.count(MODEL_WOOL_HEAD + c));
    }
    World w(5);
    Rng rng(11);
    for (int k = 0; k < ANIMAL_KIND_COUNT; ++k) {
        Animal a(w, static_cast<AnimalKind>(k), 0.5, FLOOR, 0.5, rng);
        a.saddled = true;
        for (const ModelPart& p : a.parts()) CHECK(models.count(p.model));
    }
}
