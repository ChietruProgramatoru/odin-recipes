// Chests (M11): a chest's 27 slots are kept with its chunk (Chunk::chests); two chests side by side
// are one double chest. Also the loot in dungeon chests. Beta 1.7.3's rules.
#pragma once
#include <optional>

#include <glm/vec3.hpp>

#include "rng.h"
#include "tiles.h"

class World;

// The chest joined to the one at `pos` (beside it), if any.
std::optional<glm::ivec3> chest_partner(World& world, glm::ivec3 pos);
// A double chest's two halves in the order its screen shows them: Beta puts the one further west
// (or north) on top. A single chest is just `first`.
struct ChestPair {
    glm::ivec3 first;
    std::optional<glm::ivec3> second;
};
ChestPair chest_pair(World& world, glm::ivec3 pos);

// The slots of the chest at `pos`, made empty if it had none yet (the chunk is marked changed).
// nullptr if the chunk isn't loaded.
ChestSlots* chest_slots(World& world, glm::ivec3 pos);

// Fill a dungeon chest like Beta: 8 tries, each putting one random thing (saddle, iron, bread,
// wheat, gunpowder, string, a bucket, now and then a golden apple, redstone, a music disc or cocoa
// beans) in a random slot.
void dungeon_loot(Rng& rng, ChestSlots& slots);
