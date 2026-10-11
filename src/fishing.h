// Fishing (M15): the bobber a fishing rod casts, Beta's EntityFishHook.
//
// Right click with a rod casts the bobber; it flies, lands in water and floats there. Now and
// then a fish bites (1 in 500 each tick, 1 in 300 in the rain): the bobber dips and for a moment
// a right click reels in a raw fish. A bobber that hits an animal or monster hooks it, and reeling
// in pulls it towards the player. The bobber is not saved, like Beta.
#pragma once
#include <map>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "mobs.h"
#include "physics.h"
#include "rng.h"

class World;

// Models: the bobber and a piece of line (after the vehicles, see vehicles.h).
enum FishingModel : int { MODEL_BOBBER = 450, MODEL_LINE };

constexpr int BITE_CHANCE = 500;        // a fish bites 1 tick in this many
constexpr int RAIN_BITE_CHANCE = 300;   // the same with rain falling on the bobber
constexpr double CAST_SPEED = 1.5;      // blocks per tick leaving the rod (Beta)
constexpr double MAX_LINE = 32;         // the line breaks (the bobber goes) past this many blocks
constexpr int GROUND_LIFETIME = 1200;   // ticks a bobber lies on the ground before it goes

class Bobber {
public:
    // Cast from `pos` with `vel` in blocks per tick.
    Bobber(World& world, glm::dvec3 pos, glm::dvec3 vel);
    Body body;
    glm::dvec3 vel;          // blocks per tick
    bool in_ground = false;  // landed on something solid (out of water): it stays there
    int ground_ticks = 0;
    int bite_ticks = 0;      // > 0 while a fish is on: reel in now to catch it
    // The animal or monster it hooked (Game keeps the bobber on it), as ArrowTarget kind and pointer.
    int hooked_kind = -1;
    const void* hooked = nullptr;
    long age = 0;

    struct Tick {
        bool bite = false;                  // a fish bit this tick (splash)
        std::optional<ArrowTarget> hit;     // flew into a mob this tick (it is now hooked)
    };
    // One game tick. `rain` is true when rain falls on the bobber (fish bite more often).
    Tick tick(Rng& rng, bool rain, const std::vector<ArrowTarget>& targets);
    // How much of the bobber is under water, 0 .. 1 (in fifths, Beta).
    double water_fraction() const;
    glm::dvec3 pos() const { return {body.x, body.y, body.z}; }
    std::vector<ModelPart> parts() const;
};

// What reeling in does, and how much the rod wears for it (Beta): pulling a hooked mob 3,
// catching a fish 1, pulling the bobber out of the ground 2, anything else 0.
enum class Reel { Nothing, Fish, Ground, Mob };
Reel reel_result(const Bobber& b);
int reel_wear(Reel r);

// The fishing line from the rod's tip to the bobber, sagging a little: `segments` thin boxes.
std::vector<ModelPart> fishing_line(glm::dvec3 from, glm::dvec3 to, int segments = 12);

// Models for the bobber and a piece of line (called by build_models).
void add_fishing_models(std::map<int, MeshData>& models);
