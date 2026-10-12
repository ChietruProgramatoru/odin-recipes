// Sound and music (M16): every sound is made, sounds sane, and the game's rules for which sound
// plays when.
#include <doctest/doctest.h>

#include <cmath>
#include <set>

#include "rng.h"
#include "sound.h"
#include "soundboard.h"

namespace {

double loudness(const std::vector<float>& s, size_t from, size_t to) {
    double sum = 0;
    for (size_t i = from; i < to && i < s.size(); ++i) sum += s[i] * s[i];
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, to - from)));
}

}  // namespace

TEST_CASE("every sound effect is made, short, audible and never clips") {
    for (int i = 0; i < SOUND_COUNT; ++i) {
        const Sound sound = static_cast<Sound>(i);
        if (is_music(sound) || sound == Sound::Record13 || sound == Sound::RecordCat) continue;
        CAPTURE(i);
        const auto s = make_sound(sound);
        REQUIRE(s.size() > static_cast<size_t>(SOUND_RATE / 50));
        CHECK(s.size() <= static_cast<size_t>(SOUND_RATE * 6));
        float peak = 0;
        for (float v : s) {
            REQUIRE(std::isfinite(v));
            peak = std::max(peak, std::abs(v));
        }
        CHECK(peak <= 1.0f);
        CHECK(peak > 0.2f);
    }
    CHECK(make_sound(Sound::CowSay) == make_sound(Sound::CowSay));  // the same every time
    CHECK(make_sound(Sound::StepStone) != make_sound(Sound::StepGrass));
}

TEST_CASE("music: four calm pieces over a minute long, fading in and out") {
    std::set<size_t> lengths;
    for (int i = 0; i < MUSIC_COUNT; ++i) {
        const Sound piece = static_cast<Sound>(static_cast<int>(FIRST_MUSIC) + i);
        CAPTURE(i);
        REQUIRE(is_music(piece));
        const auto s = make_sound(piece);
        CHECK(s.size() > static_cast<size_t>(SOUND_RATE * 60));
        CHECK(s.size() < static_cast<size_t>(SOUND_RATE * 150));
        for (float v : s) REQUIRE(std::abs(v) <= 1.0f);
        CHECK(std::abs(s.front()) < 0.01f);
        CHECK(std::abs(s.back()) < 0.01f);
        CHECK(loudness(s, s.size() / 2, s.size() / 2 + SOUND_RATE * 4) > 0.03);  // something is playing in the middle
        lengths.insert(s.size());
    }
    CHECK(lengths.size() == MUSIC_COUNT);
    CHECK_FALSE(is_music(Sound::RecordCat));
    CHECK_FALSE(is_music(Sound::UiClick));
}

TEST_CASE("louder sounds carry further but are never louder than full") {
    const glm::dvec3 ear(0, 64, 0);
    CHECK(sound_gain(Sound::GhastMoan, ear, 0, ear + glm::dvec3(0, 0, 40), 1.0f).left == 0);
    CHECK(sound_gain(Sound::GhastMoan, ear, 0, ear + glm::dvec3(0, 0, 40), 10.0f).left > 0);
    CHECK(sound_gain(Sound::GhastMoan, ear, 0, ear, 10.0f).left == doctest::Approx(1.0));
    CHECK(sound_gain(Sound::StepStone, ear, 0, ear, 0.15f).left == doctest::Approx(0.15));
    CHECK(sound_gain(Sound::Thunder, ear, 0, ear + glm::dvec3(150, 0, 0)).left > 0.5);  // heard all over
}

TEST_CASE("blocks sound like what they are made of") {
    CHECK(block_sound(STONE) == Sound::StepStone);
    CHECK(block_sound(COBBLE) == Sound::StepStone);
    CHECK(block_sound(PLANKS) == Sound::StepWood);
    CHECK(block_sound(CHEST) == Sound::StepWood);
    CHECK(block_sound(DIRT) == Sound::StepGravel);
    CHECK(block_sound(GRAVEL) == Sound::StepGravel);
    CHECK(block_sound(GRASS) == Sound::StepGrass);
    CHECK(block_sound(LEAVES) == Sound::StepGrass);
    CHECK(block_sound(SAND) == Sound::StepSand);
    CHECK(block_sound(WOOL) == Sound::StepCloth);
    CHECK(block_sound(SNOW_LAYER) == Sound::StepCloth);
    CHECK(block_sound(GLASS) == Sound::StepStone);
    CHECK(break_sound(GLASS) == Sound::GlassBreak);
    CHECK(break_sound(ICE) == Sound::GlassBreak);
    CHECK(break_sound(STONE) == Sound::StepStone);
    CHECK(block_pitch(IRON_BLOCK) > 1.0f);
    CHECK(block_pitch(STONE) == 1.0f);
}

TEST_CASE("a footstep every 1.67 blocks walked") {
    Footsteps f;
    int steps = 0;
    for (int i = 0; i < 1000; ++i) steps += f.walk(0.01);  // 10 blocks
    CHECK(steps >= 5);
    CHECK(steps <= 7);
    Footsteps g;
    CHECK(g.walk(0.05));  // the first step comes straight away
}

TEST_CASE("what animals and monsters say") {
    CHECK(animal_sounds(AnimalKind::Cow, false, false, 10, 1).say == Sound::CowSay);
    CHECK(animal_sounds(AnimalKind::Pig, false, false, 10, 1).death == Sound::PigDeath);
    CHECK_FALSE(animal_sounds(AnimalKind::Squid, false, false, 10, 1).say);
    CHECK(animal_sounds(AnimalKind::Wolf, true, false, 8, 1).say == Sound::WolfGrowl);
    CHECK(animal_sounds(AnimalKind::Wolf, false, true, 6, 0).say == Sound::WolfWhine);
    CHECK(animal_sounds(AnimalKind::Wolf, false, true, 20, 0).say == Sound::WolfPant);
    CHECK(animal_sounds(AnimalKind::Wolf, false, false, 8, 2).say == Sound::WolfBark);
    CHECK(monster_sounds(MobKind::Zombie, 1, false).say == Sound::ZombieSay);
    CHECK_FALSE(monster_sounds(MobKind::Creeper, 1, false).say);  // creepers creep quietly
    CHECK(monster_sounds(MobKind::Creeper, 1, false).hurt == Sound::CreeperHurt);
    CHECK(monster_sounds(MobKind::PigZombie, 1, true).say == Sound::PigmanAngry);
    CHECK(monster_sounds(MobKind::PigZombie, 1, false).say == Sound::PigmanSay);
    CHECK(monster_sounds(MobKind::Ghast, 4, false).volume > 1.0f);
    CHECK(monster_sounds(MobKind::Slime, 4, false).pitch < monster_sounds(MobKind::Slime, 1, false).pitch);
}

TEST_CASE("mobs speak about every few seconds") {
    Rng rng(7);
    int talk = 0, said = 0;
    const int ticks = 20 * 60 * 10;  // ten minutes
    for (int i = 0; i < ticks; ++i) said += mob_talks(talk, rng);
    const double seconds_between = ticks / 20.0 / said;
    CHECK(seconds_between > 4);
    CHECK(seconds_between < 10);
    for (int i = 0; i < 100; ++i) {
        const float p = vary_pitch(rng);
        CHECK(p >= 0.8f);
        CHECK(p <= 1.2f);
    }
}

TEST_CASE("music plays now and then, never the same piece twice running") {
    MusicTimer timer(3);
    CHECK(timer.wait() >= MusicTimer::FIRST_MIN);
    CHECK(timer.wait() <= MusicTimer::FIRST_MAX);
    // Nothing while something is playing.
    CHECK_FALSE(timer.update(1000, true));
    std::optional<Sound> last;
    int pieces = 0;
    double t = 0;
    for (int step = 0; step < 20 * 60 * 60 * 3; ++step) {  // three hours at 20 updates a second
        const auto piece = timer.update(0.05, false);
        t += 0.05;
        if (!piece) continue;
        REQUIRE(is_music(*piece));
        CHECK(piece != last);
        if (last) CHECK(timer.wait() >= MusicTimer::QUIET_MIN);
        last = piece;
        ++pieces;
    }
    CHECK(pieces >= 9);
    CHECK(pieces <= 19);
}
