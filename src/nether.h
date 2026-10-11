// The Nether (M14): its terrain, and the rules of portals. Beta 1.7.3's, with the world's own
// generator style (see worldgen.cpp). Pure logic on the World, no graphics.
#pragma once
#include <memory>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "world.h"

constexpr int NETHER_LAVA_LEVEL = 32;  // the lava sea fills the open space below this (Beta)
constexpr int PORTAL_TICKS = 80;       // game ticks standing in a portal before it takes you (Beta: 4 seconds)

// One chunk of the Nether: netherrack between bedrock floor and ceiling, a lava sea, soul sand and
// gravel along its shores, glowstone hanging from the roof, fires burning and a few mushrooms.
std::unique_ptr<Chunk> generate_nether(uint64_t seed, int cx, int cz);

// --- Portals -----------------------------------------------------------------------------------
// A portal is 2 wide and 3 tall inside an obsidian frame (the corners don't matter). Its data value
// is the axis it runs along: 0 along x (you walk through it along z), 1 along z.

struct PortalShape {
    glm::ivec3 origin;  // the bottom inside cell with the lowest x (axis 0) or z (axis 1)
    int axis = 0;
    glm::ivec3 dir() const { return axis == 0 ? glm::ivec3(1, 0, 0) : glm::ivec3(0, 0, 1); }
    // The six inside cells.
    std::vector<glm::ivec3> cells() const;
    // Where someone coming out of it stands: in the middle of its bottom, on the floor.
    glm::dvec3 exit() const;
    bool operator==(const PortalShape&) const = default;
};

// Fire lit at `inside` (or anything else there): is it inside an empty 2 x 3 obsidian frame?
// Tries a frame along x first, like Beta.
std::optional<PortalShape> find_portal_frame(World& world, glm::ivec3 inside);
// Fill a frame with portal blocks. Returns the chunks to mesh again.
std::vector<ChunkPos> light_portal(World& world, const PortalShape& shape);
// Can the portal block at `p` stay: obsidian or portal on every side in its plane, a column of
// exactly three between obsidian, and another portal beside it? Breaking the frame (or any portal
// block) takes the whole portal away, one neighbour at a time.
bool portal_can_stay(World& world, glm::ivec3 p);
// The portal around a portal block (nullptr-like nothing if `p` isn't in one).
std::optional<PortalShape> portal_at(World& world, glm::ivec3 p);

// Where a portal at `pos` in one dimension leads in the other: x and z divided (going to the
// Nether) or multiplied (coming back) by 8; y kept inside the other world.
glm::ivec3 portal_target(glm::ivec3 pos, Dimension to);

// Build a new portal near `target` in the world (Beta's makePortal): on the nearest flat floor with
// room around it within 16 blocks, or hanging in the air on an obsidian ledge if there is none.
// Clears the space in front and behind it. Generates the chunks it needs. Adds the chunks to mesh
// again to `dirty`.
PortalShape build_portal(World& world, glm::ivec3 target, std::vector<ChunkPos>& dirty);
