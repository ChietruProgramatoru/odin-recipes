// Monsters (zombies, skeletons, creepers, spiders and slimes), primed TNT and smoke puffs:
// how they move and fight each frame, and the parts they are drawn with. The rules and
// numbers they follow are in combat.h.
#pragma once
#include <map>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "combat.h"
#include "mobs.h"
#include "physics.h"
#include "rng.h"
#include "world.h"

// Light a mob sees at a cell: sky light dimmed by the night (`sky_darkness` levels), or block
// light, whichever is brighter.
int light_at(World& world, int x, int y, int z, int sky_darkness);

// What the world looks like to a monster this frame.
struct MonsterSenses {
    glm::dvec3 player{0};          // the player's feet
    double player_height = 1.8;
    bool player_targetable = true;  // alive and not in the creative cheat
    int sky_darkness = 0;          // levels the night takes off sky light (see SkyState)
    bool raining = false;
};

// What a monster does to the player this frame, for the game to carry out.
struct MonsterActions {
    int melee = 0;                         // a hit for this much (before difficulty and armor)
    std::optional<glm::dvec3> arrow_from;  // a skeleton's arrow leaves from here...
    glm::dvec3 arrow_vel{0};               // ...at this velocity
    bool explode = false;                  // a creeper's fuse ran out
    bool hiss = false;                     // a creeper started to hiss
    std::optional<glm::dvec3> fireball_from;  // a ghast's fireball leaves from here...
    glm::dvec3 fireball_dir{0};               // ...heading this way (normalised)
};

class Monster {
public:
    Monster(World& world, MobKind kind, double x, double y, double z, Rng& rng, int size = 1);

    MobKind kind;
    int size;  // slimes: 1, 2 or 4
    Body body;
    double yaw, target_yaw;
    int health;
    HurtCooldown cooldown;
    double flash = 0, dying = 0;     // red flash after a hit; seconds since dying
    double knock_x = 0, knock_z = 0;
    double speed = 0, walk_phase = 0;
    bool targeting = false;          // going for the player
    double attack_timer = 0;         // seconds until it can hit or shoot again
    int fuse = 0;                    // creepers: ticks the fuse has burnt (explodes at 30)
    bool powered = false;            // creepers struck by lightning blow up twice as hard
    double burning = 0;              // seconds of fire left
    double age = 0;                  // seconds since it was last near the player (despawning)
    bool looted = false;             // its drops were dropped (they drop the moment it dies)
    bool exploded = false;           // a creeper that blew itself up: no drops, no body
    bool vanish = false;             // taken away without dying (despawned, or its mount was put away)
    // Spider jockeys: the skeleton rides the spider. Both pointers are cleared when one goes.
    Monster* mount = nullptr;
    Monster* rider = nullptr;
    // Zombie pigmen: ticks it stays angry (0: leaves the player alone). Ghasts: Beta's attack
    // counter (shoots at GHAST_SHOOT_TICKS; the face shows it charging above GHAST_CHARGE_TICKS).
    int anger = 0;
    int attack_counter = 0;

    bool alive() const { return health > 0; }
    MobInfo info() const { return mob_info(kind, size); }
    // Hurt it (after its invulnerability, see HurtCooldown). Pushed away from (from_x, from_z)
    // when `knock`. Returns the damage really done.
    int hurt(int amount, double from_x, double from_z, bool knock = true);
    ArrowTarget target(int kind_tag, int index) const;
    std::optional<float> ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const;
    double eye_height() const;
    // Make a zombie pigman angry (Beta's becomeAngryAt).
    void enrage(Rng& rng) { anger = pigman_anger_ticks(rng); targeting = true; }
    bool charging() const { return kind == MobKind::Ghast && attack_counter > GHAST_CHARGE_TICKS; }
    // Creepers: how far the fuse has burnt, 0..1.
    double fuse_progress() const { return std::min(1.0, fuse / static_cast<double>(CREEPER_FUSE_TICKS)); }

    // Move, look for the player and fight. Returns true when the death animation has finished.
    bool tick(double dt, World& world, const MonsterSenses& senses, Rng& rng, MonsterActions& out);
    std::vector<ModelPart> parts() const;

private:
    void face(double x, double z);
    // Ghasts fly (Beta's EntityGhast): drift to a random point nearby, shoot at the player in sight.
    void ghast_tick(World& world, const MonsterSenses& senses, Rng& rng, MonsterActions& out);
    bool ghast_fly(double dt);
    bool course_clear(World& world, glm::dvec3 to) const;
    void per_tick(World& world, const MonsterSenses& senses, Rng& rng, MonsterActions& out);
    bool sees(World& world, const MonsterSenses& senses) const;

    double tick_time_ = 0;     // part of a game tick not run yet
    double wander_timer_;
    bool wandering_ = false;
    int blocked_ = 0;
    int jump_delay_ = 0;       // slimes: ticks until the next hop
    double fire_timer_ = 0;    // seconds until the fire hurts again
    double fall_peak_;
    double want_speed_ = 0;
    glm::dvec3 waypoint_{0};   // ghasts: where it is flying to
    int course_timer_ = 0;     // ghasts: ticks until it steers again
    bool has_waypoint_ = false;
};

// A ghast's fireball (Beta's EntityFireball): speeds up towards where it was aimed and bursts into
// a small blast that sets fires when it hits anything. Hitting it sends it back the way you look.
class Fireball {
public:
    Fireball(glm::dvec3 pos, glm::dvec3 dir, const void* owner);
    glm::dvec3 pos;
    glm::dvec3 vel{0};    // blocks per tick
    glm::dvec3 accel;     // blocks per tick, added each tick
    const void* owner;    // what shot it (it never hits its owner)
    bool deflected = false;
    double age = 0;

    // Sent back along `dir` by the player's hit; it can hurt its ghast now.
    void deflect(glm::dvec3 dir);
    // Move for `dt` seconds. Returns where it hit a block (it stops there), if it did.
    std::optional<glm::dvec3> tick(double dt, World& world);
    std::optional<float> ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const;
    std::vector<ModelPart> parts() const;

private:
    double tick_time_ = 0;
};

// A monster's parts at a root transform (feet centre, facing +z). `walk` is how much it walks
// (0..1). Used for monsters in the world and the little one spinning in a mob spawner.
std::vector<ModelPart> monster_model(MobKind kind, int size, const glm::mat4& root, double walk_phase, double walk,
                                     double head_pitch = 0);

// TNT that has been lit: falls like a block and blows up when its fuse runs out.
class PrimedTnt {
public:
    PrimedTnt(World& world, double x, double y, double z, int fuse_ticks, Rng& rng);
    Body body;
    double fuse;  // seconds left

    // Returns true when it explodes.
    bool tick(double dt);
    std::vector<ModelPart> parts() const;
};

// A puff of smoke from an explosion or a monster dying.
struct Puff {
    glm::dvec3 pos, vel;
    double age = 0, life = 1, size = 0.3;
    bool tick(double dt);  // false once it has faded away
    std::vector<ModelPart> parts() const;
};
// `count` puffs around a point, flying out up to `speed` blocks per second.
void add_puffs(std::vector<Puff>& puffs, glm::dvec3 center, int count, double spread, double speed, double size, Rng& rng);

// Monster, TNT, smoke and flame models (called by build_models).
void add_monster_models(std::map<int, MeshData>& models);
