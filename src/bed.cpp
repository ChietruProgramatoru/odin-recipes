#include "bed.h"

#include <cmath>

#include "world.h"

int facing_from_yaw(double yaw) { return static_cast<int>(std::floor(-yaw / 90.0 + 0.5)) & 3; }

std::optional<BedPlacement> bed_placement(World& world, glm::ivec3 clicked, glm::ivec3 face, int facing) {
    if (face != glm::ivec3(0, 1, 0)) return std::nullopt;  // only on top of a block, like Beta
    BedPlacement p;
    p.dir = facing & 3;
    p.foot = clicked + face;
    p.head = p.foot + FACING_DIRS[p.dir];
    for (glm::ivec3 c : {p.foot, p.head}) {
        if (c.y < 1 || c.y >= HEIGHT) return std::nullopt;
        if (world.get_block(c.x, c.y, c.z) != AIR || !is_opaque(world.get_block(c.x, c.y - 1, c.z)))
            return std::nullopt;
    }
    return p;
}

std::optional<glm::ivec3> bed_stand_spot(World& world, glm::ivec3 head) {
    BlockId b = world.get_block(head.x, head.y, head.z);
    if (!is_bed(b) || !bed_is_head(b)) return std::nullopt;
    glm::ivec3 d = FACING_DIRS[bed_dir(b)];
    // Beta's search: the 3x3 square around the head, then around the foot, at the bed's height.
    for (int half = 0; half <= 1; ++half) {
        glm::ivec3 c = head - d * half;
        for (int x = c.x - 1; x <= c.x + 1; ++x)
            for (int z = c.z - 1; z <= c.z + 1; ++z)
                if (is_opaque(world.get_block(x, head.y - 1, z)) && world.get_block(x, head.y, z) == AIR &&
                    world.get_block(x, head.y + 1, z) == AIR)
                    return glm::ivec3(x, head.y, z);
    }
    return std::nullopt;
}
