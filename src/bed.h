// Beds: which way a bed points, where a bed item goes, and where you stand up after sleeping
// (or respawn). Beta's rules.
#pragma once
#include <array>
#include <optional>

#include <glm/vec3.hpp>

#include "blocks.h"

class World;

// Beta's facing order: 0 south (+z), 1 west (-x), 2 north (-z), 3 east (+x).
constexpr std::array<glm::ivec3, 4> FACING_DIRS = {{{0, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {1, 0, 0}}};
// The facing of a player looking along a yaw (degrees, as Game uses it).
int facing_from_yaw(double yaw);

inline int bed_dir(BlockId bed) { return (bed - BED_FOOT_SOUTH) & 3; }
inline bool bed_is_head(BlockId bed) { return bed >= BED_HEAD_SOUTH; }
inline BlockId bed_block(int dir, bool head) {
    return static_cast<BlockId>((head ? BED_HEAD_SOUTH : BED_FOOT_SOUTH) + (dir & 3));
}
// The other half of the bed at pos.
inline glm::ivec3 bed_other_half(glm::ivec3 pos, BlockId bed) {
    glm::ivec3 d = FACING_DIRS[bed_dir(bed)];
    return bed_is_head(bed) ? pos - d : pos + d;
}

struct BedPlacement {
    glm::ivec3 foot, head;
    int dir;
};
// Placing a bed item by clicking a block face while facing `facing`: the foot goes on top of the
// clicked block and the head one further on. Both cells must be empty and sit on solid blocks.
std::optional<BedPlacement> bed_placement(World& world, glm::ivec3 clicked, glm::ivec3 face, int facing);

// Where to stand next to the bed with its head at `head`: a free cell (solid floor, two blocks
// of air) around the head, then around the foot. Nothing if the bed is gone or boxed in.
std::optional<glm::ivec3> bed_stand_spot(World& world, glm::ivec3 head);
