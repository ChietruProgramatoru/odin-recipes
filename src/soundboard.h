// Which sound the game plays for what (M16): the material a block sounds like, how loud steps,
// digging and building are, what each animal and monster says, and when the music plays. Rules
// only; the game (game_sound.cpp) asks these and starts the sounds.
#pragma once
#include <cstdint>
#include <optional>

#include "animals.h"
#include "blocks.h"
#include "combat.h"
#include "rng.h"
#include "sound.h"

// --- blocks (Beta's step sounds) ----------------------------------------------------------------

// The sound of walking on, digging, breaking or placing a block: stone, wood, gravel, grass, sand
// or cloth. Metal blocks sound like stone, higher (block_pitch).
Sound block_sound(BlockId block);
// Breaking it: glass, ice and glowstone shatter; everything else makes its block sound.
Sound break_sound(BlockId block);
float block_pitch(BlockId block);  // 1, or 1.5 for metal

// Beta's volumes and pitches for one block's sound: a footstep, a hit while digging (every
// DIG_SOUND_TICKS), and breaking or placing it.
constexpr float STEP_VOLUME = 0.15f;
constexpr float DIG_VOLUME = 0.25f, DIG_PITCH = 0.5f;
constexpr float BUILD_VOLUME = 1.0f, BUILD_PITCH = 0.8f;
constexpr int DIG_SOUND_TICKS = 4;
// A step every this many blocks walked (Beta adds 0.6 of the distance and steps at each whole one).
constexpr double STEP_DISTANCE = 1.0 / 0.6;

// Counts the distance walked on the ground and says when a foot comes down.
class Footsteps {
public:
    // Walked `distance` blocks (on the ground). True when a step sound is due.
    bool walk(double distance);

private:
    double walked_ = 0, next_ = 0;
};

// --- animals and monsters ----------------------------------------------------------------------

// What a mob says now and then, when hurt, and when it dies, how loud (above 1 carries further:
// ghasts are heard from far off) and at what pitch.
struct MobSounds {
    std::optional<Sound> say, hurt, death;
    float volume = 1.0f, pitch = 1.0f;
    float death_pitch = 1.0f;  // times `pitch` for the death sound (a lower hurt sound, for most)
};
// `roll` (0, 1 or 2) picks a wolf's sound: 0 pants (or whines when tamed and hurt), else barks.
MobSounds animal_sounds(AnimalKind kind, bool angry, bool tamed, int health, int roll);
MobSounds monster_sounds(MobKind kind, int size, bool angry);

// Beta's "living sound" timer: each tick a mob says something with a chance that grows the longer it
// has been quiet (about every 6 seconds). True when it speaks; then the wait starts again.
bool mob_talks(int& talk, Rng& rng);
// A random pitch around 1, like Beta's (rand - rand) * spread + 1.
float vary_pitch(Rng& rng, float spread = 0.2f);

// --- music ------------------------------------------------------------------------------------

// When the next piece of music starts: a little while after the game starts, then a long quiet
// (10 to 20 minutes) after each piece, never the same piece twice running.
class MusicTimer {
public:
    explicit MusicTimer(uint64_t seed);
    // Time passes. `busy`: music (or a jukebox's disc) is playing, so the wait doesn't count down.
    // Returns the piece to start now, if it is time.
    std::optional<Sound> update(double dt, bool busy);
    double wait() const { return wait_; }  // seconds until the next piece
    // Wait the long quiet again (a disc started, or a piece just finished).
    void rest();

    static constexpr double FIRST_MIN = 20, FIRST_MAX = 90;
    static constexpr double QUIET_MIN = 600, QUIET_MAX = 1200;

private:
    Rng rng_;
    double wait_;
    int last_ = -1;
};
