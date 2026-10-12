// Animals (M9): cows, pigs, sheep, chickens, squid and wolves. How they move each frame, what
// they drop, the Beta rules for shearing, saddles, eggs and taming, and the parts they are drawn with.
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "biome.h"
#include "mobs.h"
#include "physics.h"
#include "rng.h"
#include "sound.h"
#include "world.h"

// Saved as EntityRecord::mob, so the order never changes (old saves only have cows, which are 0).
enum class AnimalKind : uint8_t { Cow, Pig, Sheep, Chicken, Squid, Wolf };
constexpr int ANIMAL_KIND_COUNT = 6;

struct AnimalInfo {
    const char* name;
    int health;          // Beta's health (wolves: wild)
    double half, height;  // hit box
    double walk_speed;   // blocks per second when wandering
};
const AnimalInfo& animal_info(AnimalKind kind);

constexpr int TAMED_WOLF_HEALTH = 20;   // Beta: a tamed wolf has 20 health, a wild one 8
constexpr int WILD_WOLF_BITE = 2;       // Beta's wolf bites: wild and angry 2, tamed 4
constexpr int TAMED_WOLF_BITE = 4;
constexpr double WOLF_FOLLOW_DISTANCE = 5;     // a tamed wolf comes back when its owner is further than this...
constexpr double WOLF_TELEPORT_DISTANCE = 12;  // ...and jumps to them past this (Beta)
constexpr double WOLF_ANGER_RANGE = 16;        // hitting a wild wolf angers the wild wolves this close
constexpr double ANIMAL_BURN_SECONDS = 8;      // set alight by fire (lava: twice as long)
constexpr int SHEEP_COLOR_WHITE = 0;

// Beta's chances for a new sheep's wool: mostly white, some greys, black and brown, very rarely pink.
int natural_sheep_color(Rng& rng);
// The wool colour (0 white .. 15 black, like the wool items) a dye turns a sheep, or nothing if
// the item isn't a dye.
std::optional<int> dye_wool_color(ItemId item);
ItemId wool_item(int color);
// Shears take 1 to 3 wool off a sheep (Beta). It doesn't grow back.
int shear_count(Rng& rng);
// Ticks until a chicken lays its next egg (Beta: 6000 to 11999).
int egg_delay_ticks(Rng& rng);
// Chickens hatched from a thrown egg: one in 8 eggs hatches, and one in 32 of those makes 4.
int chicks_from_egg(Rng& rng);
// A bone tames a wild wolf one time in 3.
bool tame_succeeds(Rng& rng);
// Health a tamed wolf gets back from food it likes (meat), or 0 if it won't eat it.
int wolf_food_heal(ItemId item);
// What an animal drops when it dies. Burning pigs (and cows) drop their meat cooked.
struct AnimalLootState {
    bool burning = false, sheared = false, saddled = false;
    int wool = SHEEP_COLOR_WHITE;
};
std::vector<ItemId> animal_drops(AnimalKind kind, const AnimalLootState& state, Rng& rng);
// The kind of a group of land animals in a biome, picked by Beta's weights (sheep 12, pig 10,
// chicken 10, cow 8, and wolves 2 in forests and taiga), or nothing (deserts have no animals).
std::optional<AnimalKind> pick_land_animal(Biome biome, Rng& rng);

// What the world looks like to an animal this frame.
struct AnimalSenses {
    glm::dvec3 player{0};
    bool player_targetable = true;      // alive and not in the creative cheat (angry wolves bite)
    bool peaceful = false;              // angry wolves leave the player alone on Peaceful
    std::optional<glm::dvec3> prey;     // a wolf's prey: its feet...
    double prey_half = 0.3;             // ...and size
};

// What an animal does this frame, for the game to carry out.
struct AnimalActions {
    int bite_player = 0;           // an angry wolf bit the player for this much
    int bite_prey = 0;             // a wolf bit its prey for this much
    bool lay_egg = false;          // a chicken laid an egg
    bool teleport = false;         // a tamed wolf is too far from its owner: put it next to them
};

class Animal {
public:
    enum class State { Idle, Walk, Panic };

    Animal(World& world, AnimalKind kind, double x, double y, double z, Rng& rng);

    AnimalKind kind;
    Body body;
    double yaw, target_yaw;
    int health;
    State state = State::Idle;
    double state_timer;
    double speed = 0, walk_phase = 0;
    double knock_x = 0, knock_z = 0;
    double flash = 0, dying = 0;
    int blocked = 0;
    bool graze = false;
    double head_pitch = 0, roll = 0;
    double burning = 0;  // seconds of fire left

    // Sheep: the wool colour, and whether it has been sheared (Beta: the wool never grows back).
    int wool = SHEEP_COLOR_WHITE;
    bool sheared = false;
    // Pigs: wearing a saddle, and being ridden by the player.
    bool saddled = false, ridden = false;
    // Chickens: ticks until the next egg, and the wings' flapping.
    double egg_timer = 0, flap = 0;
    // Squid: which way it swims and the stroke of its tentacles.
    glm::dvec3 swim{0};
    double swim_timer = 0, stroke = 0, pitch = 0;
    // Wolves: tamed by the player (it follows them and fights for them), sitting, angry at the
    // player, and what it is hunting. The game clears the prey pointers when they go.
    bool tamed = false, sitting = false, angry = false;
    const void* prey = nullptr;
    double bite_timer = 0;
    MobVoice voice;  // what the game's sounds remember of it

    bool alive() const { return health > 0; }
    AnimalInfo info() const { return animal_info(kind); }
    int max_health() const { return kind == AnimalKind::Wolf && tamed ? TAMED_WOLF_HEALTH : info().health; }
    // Hurt it, pushed away from (from_x, from_z). Animals run off in a panic; a hit sitting wolf stands up.
    void hurt(int amount, double from_x, double from_z);
    void tame();
    ArrowTarget target(int kind_tag, int index) const;
    std::optional<float> ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const;
    AnimalLootState loot_state() const { return {burning > 0, sheared, saddled, wool}; }

    // Wander, flee, swim, hunt. Returns true when the death animation has finished and it should drop loot.
    bool tick(double dt, const AnimalSenses& senses, Rng& rng, AnimalActions& out);
    std::vector<ModelPart> parts() const;

private:
    void wander(double dt, Rng& rng, double player_x, double player_z);
    void chase(double dt, glm::dvec3 to, double run);
    void walk(double dt, Rng& rng);
    void swim_about(double dt, Rng& rng);

    double want_ = 0;       // speed it wants this frame
    double burn_hurt_ = 0;  // seconds until the fire hurts again
};

// Models drawn for animals (called by build_models).
enum AnimalModel : int {
    MODEL_COW_BODY = 300, MODEL_COW_LEG, MODEL_COW_HEAD,
    MODEL_PIG_BODY, MODEL_PIG_LEG, MODEL_PIG_HEAD, MODEL_SADDLE,
    MODEL_SHEEP_BODY, MODEL_SHEEP_LEG, MODEL_SHEEP_HEAD,
    MODEL_CHICKEN_BODY, MODEL_CHICKEN_HEAD, MODEL_CHICKEN_WING, MODEL_CHICKEN_LEG,
    MODEL_SQUID_BODY, MODEL_SQUID_TENTACLE,
    MODEL_WOLF_BODY, MODEL_WOLF_MANE, MODEL_WOLF_LEG, MODEL_WOLF_HEAD, MODEL_WOLF_HEAD_ANGRY, MODEL_WOLF_TAIL,
    // Wool on a sheep, one per colour: body, then leg, then head.
    MODEL_WOOL_BODY = 340, MODEL_WOOL_LEG = MODEL_WOOL_BODY + 16, MODEL_WOOL_HEAD = MODEL_WOOL_LEG + 16,
};
void add_animal_models(std::map<int, MeshData>& models);
