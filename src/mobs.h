// Cows and dropped items: behaviour plus the box geometry used to draw them.
#pragma once
#include <map>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>

#include "blocks.h"
#include "physics.h"
#include "rng.h"
#include "world.h"

constexpr double COW_HALF_WIDTH = 0.45;
constexpr double COW_HEIGHT = 1.3;
constexpr int COW_MAX_HEALTH = 10;

// Models drawn for cows and dropped items. Item models are ITEM_MODEL_BASE + item id.
enum Model : int {
    MODEL_COW_BODY, MODEL_COW_BODY_HURT, MODEL_COW_LEG, MODEL_COW_LEG_HURT, MODEL_COW_HEAD, MODEL_COW_HEAD_HURT,
    MODEL_PLAYER_HEAD, MODEL_PLAYER_BODY, MODEL_PLAYER_ARM, MODEL_PLAYER_LEG,
    ITEM_MODEL_BASE = 1000
};

struct ModelPart {
    int model;
    glm::mat4 transform;
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
    // Returns true when the cow has finished dying and should drop loot.
    bool tick(double dt, double player_x, double player_z, Rng& rng);
    std::vector<ModelPart> parts() const;
};

class ItemDrop {
public:
    ItemDrop(World& world, ItemId item, double x, double y, double z, Rng& rng);
    ItemId item;
    Body body;
    double age = 0;
    double pickup_delay = 0.5;  // seconds before it can be picked up (longer for items you throw)

    void tick(double dt);
    std::vector<ModelPart> parts() const;
};

// The player as seen in third person (F5): an original blocky figure, 1.8 blocks tall.
struct PlayerPose {
    double x = 0, y = 0, z = 0;  // feet centre
    double yaw = 0, pitch = 0;   // same angles as the camera
    double walk_phase = 0;       // advances with distance walked
    double walk_amount = 0;      // 0 standing still .. 1 walking
    bool sneaking = false;
};
std::vector<ModelPart> player_parts(const PlayerPose& pose);

// Static meshes for cows, the player and dropped items, each in its own local frame.
std::map<int, MeshData> build_models();

int drop_beef_count(Rng& rng);
