// Farming (M8): farmland, wheat, sugar cane, wild plants, cake and bone meal, and Beta's random
// block ticks that make them grow. Rules only: the game applies the block changes and drops.
#pragma once
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "blockupdate.h"  // RANDOM_TICKS_PER_CHUNK
#include "rng.h"
#include "survival.h"
#include "world.h"

constexpr int RANDOM_TICK_RADIUS = 9;       // chunks around the player that get random ticks (Beta)
constexpr int FARMLAND_WET = 7;             // farmland data: 7 next to water, drying down to 0
constexpr int WHEAT_RIPE = 7;               // wheat data: stage 0 (just planted) .. 7 (ripe)
constexpr int CAKE_SLICES = 6;              // cake data: slices eaten, 0 .. 5
constexpr int SUGAR_CANE_MAX = 3;           // sugar cane grows this tall by itself
constexpr int SUGAR_CANE_AGE = 15;          // random ticks (on the top piece) before it grows one more
constexpr int CROP_LIGHT = 9;               // wheat grows with at least this much light above it
constexpr int MUSHROOM_MAX_LIGHT = 12;      // mushrooms need it darker than 13
constexpr int BONE_MEAL_TRIES = 128;        // Beta: tries at tall grass and flowers around grass
const FoodInfo CAKE_SLICE{2, 0.4f};         // one slice of cake

// The blocks a farming rule changed and the items it dropped. The game applies these.
struct FarmEvents {
    std::vector<ChunkPos> rebuild;                     // chunks to mesh again
    std::vector<std::pair<glm::ivec3, ItemId>> drops;  // one item each, dropped in the middle of the block
    // Set a block (and its data) and remember the chunks to mesh again.
    void set(World& world, glm::ivec3 pos, BlockId block, int data = 0);
};

// What the random tick needs to know about the world beyond the blocks.
struct TickContext {
    int sky_darkness = 0;  // how many levels sky light is down at this time of day (0 by day, 11 at night)
    bool raining = false;
};

// Could `block` stay at `pos` with what is around it (soil underneath, light, water)?
bool can_stay(World& world, glm::ivec3 pos, BlockId block);
// Light at a block as plants see it: block light, or sky light dimmed by the time of day.
int plant_light(World& world, glm::ivec3 pos, int sky_darkness);
// Farmland has water within 4 blocks across, at its own height or one above.
bool water_near_farmland(World& world, glm::ivec3 pos);
// Beta's wheat growth rate: 1, plus 1 for the farmland below (3 if wet) and a quarter of that for
// each farmland around it, halved when wheat grows on two sides or a diagonal. Chance per tick is 1 in 100/rate.
float wheat_growth_rate(World& world, glm::ivec3 pos);

// One random tick of the block at `pos`: wheat and sugar cane grow, farmland wets or dries out,
// mushrooms spread, and plants that can't stay there any more pop off.
void random_tick(World& world, glm::ivec3 pos, Rng& rng, const TickContext& ctx, FarmEvents& out);
// Beta's random ticks for one chunk: RANDOM_TICKS_PER_CHUNK blocks picked at random.
void tick_chunk(World& world, const Chunk& chunk, Rng& rng, const TickContext& ctx, FarmEvents& out);

// What a farming block drops when broken (wheat, seeds, sugar cane...), with Beta's chances.
// Nothing for blocks that aren't part of farming.
std::vector<ItemId> farm_drops(BlockId block, int data, Rng& rng);
// The block at `pos` changed: plants (and cake) above it that lost their footing break and drop,
// all the way up a stalk of sugar cane.
void break_unsupported(World& world, glm::ivec3 pos, Rng& rng, FarmEvents& out);

// A hoe on dirt or grass with air above makes dry farmland. Returns false if it can't.
bool till(World& world, glm::ivec3 pos, FarmEvents& out);
// Something lands on farmland after falling `fall` blocks: with a chance of fall - 0.5 (Minecraft 1.0's
// rule; walking over it is safe) it turns back to dirt and the crop on it pops off. Returns true if it did.
bool trample(World& world, glm::ivec3 pos, double fall, Rng& rng, FarmEvents& out);
// Use bone meal on a block: wheat ripens, grass sprouts tall grass and flowers around it.
// Returns false if it does nothing there (and isn't used up).
bool bone_meal(World& world, glm::ivec3 pos, Rng& rng, FarmEvents& out);
// The block an item places as a plant or cake (seeds, sugar cane, flowers...), or AIR.
BlockId planted_block(ItemId item);
