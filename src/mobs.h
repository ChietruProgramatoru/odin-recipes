// Cows, dropped items and arrows: behaviour plus the box geometry used to draw them.
// Monsters are in monsters.h.
#pragma once
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "physics.h"
#include "rng.h"
#include "survival.h"
#include "world.h"

constexpr double COW_HALF_WIDTH = 0.45;
constexpr double COW_HEIGHT = 1.3;
constexpr int COW_MAX_HEALTH = 10;

// Models drawn for cows and dropped items. Item models are ITEM_MODEL_BASE + item id.
enum Model : int {
    MODEL_COW_BODY, MODEL_COW_BODY_HURT, MODEL_COW_LEG, MODEL_COW_LEG_HURT, MODEL_COW_HEAD, MODEL_COW_HEAD_HURT,
    MODEL_PLAYER_HEAD, MODEL_PLAYER_BODY, MODEL_PLAYER_ARM, MODEL_PLAYER_LEG,
    MODEL_ARROW,
    // Monsters (see monsters.cpp), primed TNT, smoke puffs and the flames on a burning mob.
    MODEL_ZOMBIE_HEAD = 200, MODEL_ZOMBIE_BODY, MODEL_ZOMBIE_ARM, MODEL_ZOMBIE_LEG,
    MODEL_SKELETON_HEAD, MODEL_SKELETON_BODY, MODEL_SKELETON_ARM, MODEL_SKELETON_LEG,
    MODEL_CREEPER_HEAD, MODEL_CREEPER_BODY, MODEL_CREEPER_LEG,
    MODEL_SPIDER_HEAD, MODEL_SPIDER_BODY, MODEL_SPIDER_LEG,
    MODEL_SLIME,
    MODEL_TNT, MODEL_PUFF, MODEL_FLAMES,
    // Armor on the player, one model per material (leather, chain, iron, gold, diamond) from each base.
    MODEL_ARMOR_HELMET = 100, MODEL_ARMOR_CHEST = 105, MODEL_ARMOR_SLEEVE = 110, MODEL_ARMOR_WAIST = 115,
    MODEL_ARMOR_LEG = 120, MODEL_ARMOR_BOOT = 125,
    ITEM_MODEL_BASE = 1000
};

struct ModelPart {
    int model;
    glm::mat4 transform;
    float sky = 1, block = 0;  // light levels 0..1 where the model stands
    glm::vec4 overlay{0};      // colour mixed over the model by its alpha (red when hurt, white for a fuse)
};

// Add a textured box of `size` centred at `center` (in the frame of matrix m) to a model.
void add_box(MeshData& mesh, const glm::mat4& m, glm::vec3 size, glm::vec3 center, int tile, float brightness = 1.0f);

// Distance along a ray to a box, or nothing.
std::optional<float> ray_box(glm::vec3 origin, glm::vec3 direction, float max_dist, glm::vec3 lo, glm::vec3 hi);

// Something an arrow can hit: its box and who it is (Game's numbering).
struct ArrowTarget {
    glm::vec3 lo, hi;
    int kind = 0, index = 0;
    const void* who = nullptr;  // an arrow never hits the monster that shot it
};

class Cow {
public:
    enum class State { Idle, Walk, Panic };

    Cow(World& world, double x, double y, double z, Rng& rng);

    Body body;
    double yaw, target_yaw;
    int health = COW_MAX_HEALTH;
    State state = State::Idle;
    double state_timer;
    double speed = 0, walk_phase = 0;
    double knock_x = 0, knock_z = 0;
    double flash = 0, dying = 0;
    int blocked = 0;
    bool graze = false;
    double head_pitch = 0, roll = 0;

    bool alive() const { return health > 0; }
    void hurt(int amount, double from_x, double from_z);
    // Distance along a ray to this cow's hit box, or nothing.
    std::optional<float> ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const;
    ArrowTarget target(int kind, int index) const;
    // Returns true when the cow has finished dying and should drop loot.
    bool tick(double dt, double player_x, double player_z, Rng& rng);
    std::vector<ModelPart> parts() const;
};

class ItemDrop {
public:
    ItemDrop(World& world, Stack stack, double x, double y, double z, Rng& rng);
    Stack stack;  // a whole stack lies on the ground as one item
    Body body;
    double age = 0;
    double pickup_delay = 0.5;  // seconds before it can be picked up (longer for items you throw)

    void tick(double dt);
    std::vector<ModelPart> parts() const;
};

// An arrow shot from a bow. It flies with gravity and drag, hurts the first mob (or player) it hits,
// and sticks in the first solid block until it is picked up or a minute has passed.
class Arrow {
public:
    static constexpr double SPEED = 30.0;        // blocks per second leaving the bow (Beta: 1.5 per tick)
    static constexpr double GRAVITY = 12.0;      // Beta: 0.03 per tick per tick
    static constexpr double STUCK_LIFETIME = 60.0;

    Arrow(glm::dvec3 pos, glm::dvec3 vel) : pos(pos), vel(vel) { aim(); }
    glm::dvec3 pos, vel;
    double yaw = 0, pitch = 0;  // which way it points (pitch > 0 points down)
    bool stuck = false;
    glm::ivec3 stuck_in{0};
    double age = 0, stuck_time = 0;
    // Shot by a skeleton: it can't be picked up (Beta), and it doesn't hit the skeleton.
    const void* shooter = nullptr;
    bool from_player = true;

    // Fly for dt seconds. Returns what it hit (the arrow is then used up), if anything.
    std::optional<ArrowTarget> tick(double dt, World& world, const std::vector<ArrowTarget>& targets);
    bool expired() const { return stuck && stuck_time > STUCK_LIFETIME; }
    std::vector<ModelPart> parts() const;

private:
    void aim();
};

// The player as seen in third person (F5): an original blocky figure, 1.8 blocks tall.
struct PlayerPose {
    double x = 0, y = 0, z = 0;  // feet centre
    double yaw = 0, pitch = 0;   // same angles as the camera
    double walk_phase = 0;       // advances with distance walked
    double walk_amount = 0;      // 0 standing still .. 1 walking
    bool sneaking = false;
    bool sitting = false;        // riding a minecart or boat: legs out in front
    // Armor worn (helmet, chestplate, leggings, boots): the material, or nothing.
    std::array<std::optional<Material>, 4> armor{};
};
// The material of each armor piece in an inventory's armor slots, for the model.
std::array<std::optional<Material>, 4> worn_armor(const Inventory& inv);
std::vector<ModelPart> player_parts(const PlayerPose& pose);

// Static meshes for cows, the player and dropped items, each in its own local frame.
std::map<int, MeshData> build_models();

int drop_beef_count(Rng& rng);
