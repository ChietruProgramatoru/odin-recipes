// Which block a torch hangs on, and which torch block a click on a block face places.
#pragma once
#include <optional>

#include <glm/vec3.hpp>

#include "blocks.h"

// Offset from a torch to the block holding it up.
inline glm::ivec3 torch_support(BlockId torch) {
    switch (torch) {
        case TORCH_WALL_WEST: return {-1, 0, 0};
        case TORCH_WALL_EAST: return {1, 0, 0};
        case TORCH_WALL_NORTH: return {0, 0, -1};
        case TORCH_WALL_SOUTH: return {0, 0, 1};
        default: return {0, -1, 0};
    }
}

// The torch placed against a block face, given the face's outward direction (from the clicked
// block towards the cell the torch goes in). Nothing for the underside of a block, like Beta.
inline std::optional<BlockId> torch_for_face(glm::ivec3 face) {
    if (face.y > 0) return TORCH;
    if (face.x > 0) return TORCH_WALL_WEST;
    if (face.x < 0) return TORCH_WALL_EAST;
    if (face.z > 0) return TORCH_WALL_NORTH;
    if (face.z < 0) return TORCH_WALL_SOUTH;
    return std::nullopt;
}
