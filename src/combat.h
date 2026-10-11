// Monsters and combat: Beta's numbers and rules for difficulty, mob stats, being hurt,
// spawning, burning in daylight, mob spawners and explosions. Pure logic, no graphics.
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "items.h"
#include "rng.h"

class World;

// --- Difficulty --------------------------------------------------------------------------------

// Options.difficulty: 0 Peaceful, 1 Easy, 2 Normal (the default), 3 Hard.
enum Difficulty : int { PEACEFUL, EASY, NORMAL, HARD, DIFFICULTY_COUNT };
const char* difficulty_name(int difficulty);

// Damage a monster (a hit, its arrow or a creeper's blast) does to the player on a difficulty:
// none on Peaceful, a third plus one on Easy, as is on Normal, half as much again on Hard (Beta).
int scale_monster_damage(int amount, int difficulty);

// --- Being hurt -------------------------------------------------------------------------------

// Beta's invulnerability after a hit: for half a second more hits do nothing, except that a
// harder one does the difference between the two. Then a second hit counts in full again.
class HurtCooldown {
public:
    static constexpr double LENGTH = 1.0;  // seconds a hit is remembered (Beta: 20 ticks)
    // Damage a hit of `amount` really does now. `fresh` says whether it is a new hit (flash and
    // knockback) or only tops up an earlier one.
    int take(int amount, bool& fresh);
    void tick(double dt) { timer_ = timer_ > dt ? timer_ - dt : 0; }
    bool invulnerable() const { return timer_ > LENGTH / 2; }

private:
    double timer_ = 0;
    int last_ = 0;
};

// --- Monsters -----------------------------------------------------------------------------------

// Ghasts and zombie pigmen (M14) live in the Nether.
enum class MobKind : uint8_t { Zombie, Skeleton, Creeper, Spider, Slime, Ghast, PigZombie };
constexpr int MOB_KIND_COUNT = 7;
constexpr int OVERWORLD_MOB_KINDS = 5;  // the ones before Ghast
const char* mob_name(MobKind kind);

struct MobInfo {
    int health;          // half-hearts (a slime's depends on its size)
    double speed;        // blocks per second when walking
    double half_width, height;
    int attack;          // half-hearts a melee hit does (0: doesn't hit)
    bool burns_in_daylight;
    bool fire_immune = false;  // lava and fire don't hurt it (the Nether's monsters)
};
// Beta's stats. `size` is a slime's size (1, 2 or 4); other monsters ignore it.
MobInfo mob_info(MobKind kind, int size = 1);

// What a dead monster drops: Beta drops 0-2 of its item (a skeleton 0-2 arrows and 0-2 bones,
// only the smallest slimes drop slimeballs). Creepers that blow up drop nothing.
std::vector<ItemId> mob_drops(MobKind kind, int size, Rng& rng);

// A slime of size 2 or 4 splits into 2-4 slimes of half its size when it dies.
int slime_split_count(int size, Rng& rng);

constexpr double TARGET_RANGE = 16.0;       // monsters notice the player this close
constexpr double CREEPER_IGNITE_RANGE = 3.0;
constexpr double CREEPER_DEFUSE_RANGE = 7.0;
constexpr int CREEPER_FUSE_TICKS = 30;
constexpr int CREEPER_POWER = 3, CHARGED_CREEPER_POWER = 6, TNT_POWER = 4;
constexpr int TNT_FUSE_TICKS = 80;
constexpr double SKELETON_RANGE = 10.0;     // skeletons shoot from this close
constexpr double SKELETON_RELOAD = 1.5;     // seconds between arrows (Beta: 30 ticks)
constexpr double MELEE_RANGE = 2.0;
constexpr double MELEE_RELOAD = 1.0;        // seconds between hits (Beta: 20 ticks)
constexpr double SKELETON_ARROW_SPEED = 12.0;  // blocks per second (Beta: 0.6 per tick)

// Beta's brightness curve for a light level (0-15): 0.05 in the dark up to 1 at 15.
double light_brightness(int level);
// Spiders only go for the player in the dark (brightness under one half, light 11 or less).
bool spider_hostile(int light);

// Zombies and skeletons catch fire in daylight: rolled every tick (1/20 s) for a monster that sees
// the sky, in the day (sky darkness under 4) and with light above 11. Beta's chance grows with the
// brightness, so a sunny monster starts burning within a second or two.
bool catches_fire_in_daylight(int light, bool sees_sky, int sky_darkness, Rng& rng);
constexpr double DAYLIGHT_BURN_SECONDS = 15.0;  // Beta: 300 ticks

// --- Spawning -----------------------------------------------------------------------------------

// Beta's darkness test for a monster spawning in a cell: `raw_sky` is the cell's sky light before
// night dims it, `light` the light there now (dimmed sky light or block light, the brighter).
// Too much open sky fails at random, and the light must be at most a random 0-7.
bool monster_spawn_light_ok(int raw_sky, int light, Rng& rng);
// Monsters spawned at random in the chunks around the player: up to 70 for every 256 chunks.
int monster_cap(int chunks);
constexpr double SPAWN_MIN_DISTANCE = 24.0;   // never closer to the player than this
constexpr double DESPAWN_DISTANCE = 128.0;    // monsters this far away vanish at once
constexpr double DESPAWN_SAFE_DISTANCE = 32.0;  // closer than this they never despawn
// Which monster a random spawn tries: zombie, skeleton, creeper, spider and slime equally often.
MobKind random_monster(Rng& rng);
// In the Nether: ghasts and zombie pigmen equally often (Beta's Hell spawn list). A ghast then only
// spawns one try in 20 (Beta), so pigmen are far more common.
MobKind random_nether_monster(Rng& rng);
constexpr int GHAST_SPAWN_ODDS = 20;

// --- The Nether's monsters (M14) ----------------------------------------------------------------

constexpr double GHAST_RANGE = 64.0;        // ghasts shoot at a player they can see this close
constexpr int GHAST_CHARGE_TICKS = 10;      // its face turns angry this many ticks before it shoots
constexpr int GHAST_SHOOT_TICKS = 20;       // ticks seeing the player before a fireball
constexpr int GHAST_RELOAD_TICKS = 40;      // ...and the wait after one (Beta: the counter goes back to -40)
constexpr double GHAST_WANDER = 16.0;       // it flies to points this far away in each direction
constexpr int FIREBALL_HIT_DAMAGE = 4;      // a fireball hitting something straight on (Beta)
constexpr double FIREBALL_POWER = 1.0;      // its blast, which sets fires
constexpr double FIREBALL_LIFE = 30.0;      // seconds before one that hits nothing fizzles out
constexpr double PIGMAN_ANGER_RANGE = 32.0; // hitting a zombie pigman angers every one this close
// How long a zombie pigman stays angry (Beta: 400 + 0-399 ticks).
int pigman_anger_ticks(Rng& rng);
// One chunk in ten is a slime chunk, where slimes spawn below y 16 whatever the light.
bool is_slime_chunk(uint64_t seed, int cx, int cz);
constexpr int SLIME_MAX_Y = 16;
// Slimes spawn in sizes 1, 2 or 4.
int random_slime_size(Rng& rng);
// One spider in a hundred spawns with a skeleton riding it (a spider jockey).
bool spawns_as_jockey(Rng& rng);

// --- Mob spawners --------------------------------------------------------------------------------

constexpr double SPAWNER_RANGE = 16.0;  // a spawner works while the player is this close
constexpr int SPAWNER_MAX_NEARBY = 6;   // ...and fewer of its monsters than this are near it
// The monster a dungeon's spawner makes: skeleton 1 in 4, zombie 2 in 4, spider 1 in 4 (Beta).
// Picked from the spawner's position and the world seed, so it needs no saving.
MobKind spawner_mob(uint64_t seed, glm::ivec3 pos);
// Ticks until a spawner's next try: 200 to 799 (Beta).
int spawner_delay(Rng& rng);

// --- Explosions -----------------------------------------------------------------------------------

// How well a block stands up to explosions (Beta: its hardness, or the explosion resistance some
// blocks have on top: stone and bricks 6, water and lava 100, bedrock practically endless).
float explosion_resistance(uint8_t block);

// The blocks an explosion of `power` at `center` destroys: Beta's 1352 rays out from the centre,
// each with a random strength that is used up by distance and by the blocks it passes. Air isn't
// listed. Every block appears once.
std::vector<glm::ivec3> explosion_blocks(World& world, glm::dvec3 center, double power, Rng& rng);
// Beta drops 30% of the blocks an explosion destroys.
constexpr double EXPLOSION_DROP_CHANCE = 0.3;

// How much of a box the explosion sees (0..1): the share of points spread over the box that
// have no solid block between them and the centre.
double explosion_exposure(World& world, glm::dvec3 center, glm::dvec3 box_lo, glm::dvec3 box_hi);
// Damage (half-hearts) to something `distance` from the centre with `exposure` (Beta's formula:
// up to 8 x power x 2 + 1 point blank, nothing beyond twice the power).
int explosion_damage(double power, double distance, double exposure);
// How hard it pushes, 0..1 (multiply by the direction away from the centre).
double explosion_push(double power, double distance, double exposure);
