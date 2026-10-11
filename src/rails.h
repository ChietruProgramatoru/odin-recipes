// Rails (M12), as Beta's BlockRail and RailLogic: the ten shapes a rail can take, how a new rail
// joins the rails next to it, and the line through a rail that a minecart follows. Pure logic on
// the World, no graphics.
#pragma once
#include <array>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "world.h"

// A rail's data value (Beta's metadata). North is -z, south +z, east +x, west -x. A slope rises
// towards the side it names. Curves join the two sides they name.
enum RailShape : int {
    RAIL_NORTH_SOUTH, RAIL_EAST_WEST,
    RAIL_ASCENDING_EAST, RAIL_ASCENDING_WEST, RAIL_ASCENDING_NORTH, RAIL_ASCENDING_SOUTH,
    RAIL_SOUTH_EAST, RAIL_SOUTH_WEST, RAIL_NORTH_WEST, RAIL_NORTH_EAST,
};
constexpr int RAIL_POWERED = 8;  // powered rails: powered; detector rails: a minecart is on it

// Only plain rails curve; powered and detector rails keep the low 3 bits for the shape.
inline bool rail_can_curve(BlockId b) { return b == RAIL; }
inline int rail_shape(BlockId b, int data) { return rail_can_curve(b) ? (data <= RAIL_NORTH_EAST ? data : 0) : (data & 7) % 6; }
inline bool rail_powered(BlockId b, int data) { return !rail_can_curve(b) && (data & RAIL_POWERED); }
inline bool rail_sloped(int shape) { return shape >= RAIL_ASCENDING_EAST && shape <= RAIL_ASCENDING_SOUTH; }

// The two ends of a rail's shape, as offsets from its cell (Beta's EntityMinecart matrix): x and z
// say which side, y is -1 at the low end of a slope.
const std::array<glm::ivec3, 2>& rail_ends(int shape);

// The rail at a cell, or at the cell above or below it (a rail can join one on a slope): the cell
// it is in, or nothing.
std::optional<glm::ivec3> rail_near(World& world, glm::ivec3 p);

// Can a rail stay here: a solid block under it, and for a slope a solid block on its high side
// (Beta's BlockRail.onNeighborBlockChange)?
bool rail_can_stay(World& world, glm::ivec3 p);
bool rail_can_stay(World& world, glm::ivec3 p, int shape);

// A rail was just put at p (the block is already set): pick its shape from the rails around it,
// then bend those rails to join it (Beta's RailLogic). Returns the chunks to mesh again.
std::vector<ChunkPos> connect_rail(World& world, glm::ivec3 p);

// Set a powered or detector rail's powered bit. Returns the chunks to mesh again (none if it
// didn't change).
std::vector<ChunkPos> set_rail_powered(World& world, glm::ivec3 p, bool powered);
