// Nature (M6 part 2): leaves that decay when no log holds them up, ice and snow melting in the
// light, snow settling and water freezing in cold biomes, cactus growing on sand, and saplings
// growing into trees. Rules on the World only; BlockUpdater (blockupdate.h) and farming run them.
#pragma once
#include <vector>

#include <glm/vec3.hpp>

#include "physics.h"
#include "rng.h"
#include "world.h"

constexpr int LEAVES_PLAYER = 4;  // leaves data: placed by a player, never decay
constexpr int LEAVES_CHECK = 8;   // leaves data: a log near them went, so look for another at the next random tick
constexpr int LEAF_REACH = 4;     // leaves stay up within this many leaves of a log (Beta)
constexpr int SAPLING_READY = 8;  // sapling data: the next growth tick grows the tree
constexpr int SAPLING_LIGHT = 9;  // saplings grow with at least this much light above them
constexpr int SAPLING_ODDS = 30;  // 1 in this many random ticks moves a sapling on (Beta)
constexpr int CACTUS_MAX = 3;     // cactus grows this tall by itself
constexpr int CACTUS_AGE = 15;    // random ticks (on the top piece) before it grows one more
constexpr int MELT_LIGHT = 11;    // ice and snow melt in block light above this less their opacity (Beta)
constexpr int FREEZE_LIGHT = 10;  // water freezes and snow settles only where block light is below this
constexpr int SNOW_ODDS = 16;     // each tick, 1 chunk in this many near the player may snow or freeze (Beta)

bool is_log(BlockId b);
// Is there a log within LEAF_REACH steps through leaves (Beta's BlockLeaves distance search)?
bool log_near(World& world, glm::ivec3 leaves);
// A block at `pos` went: leaves within LEAF_REACH get LEAVES_CHECK (Beta's BlockLog.onBlockRemoval).
// Returns how many were marked.
int mark_leaves(World& world, glm::ivec3 pos);
// What leaves drop when they decay or are broken without shears: a sapling 1 time in 20 (Beta),
// and an apple 1 time in 10 from oak leaves (this game's apples).
std::vector<ItemId> leaf_drops(BlockId leaves, Rng& rng);

// Cactus stays on sand or cactus, with nothing solid on its four sides (Beta).
bool cactus_can_stay(World& world, glm::ivec3 pos);
// A snow layer lies on top of leaves or an opaque block (not ice).
bool snow_can_stay(World& world, glm::ivec3 pos);
// Ice turns to water and snow layers go in enough block light (torches, glowstone, lava).
bool melts(World& world, glm::ivec3 pos);
// Does a body touch a cactus (Beta hurts anything inside its 1/16 inset, on top too)?
bool touches_cactus(World& world, const Body& body);

// Grow the sapling at `pos` into its tree (oak, now and then a big oak; birch; spruce), Beta's
// BlockSapling.growTree. The tree generators are world generation's (worldgen.cpp). Returns false,
// leaving the sapling, when there is no room. Chunks to mesh again are added to `rebuild`.
bool grow_tree(World& world, glm::ivec3 pos, Rng& rng, std::vector<ChunkPos>& rebuild);
