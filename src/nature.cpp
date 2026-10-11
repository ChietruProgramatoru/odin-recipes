#include "nature.h"

#include <algorithm>
#include <set>
#include <tuple>

#include "items.h"

namespace {

constexpr glm::ivec3 NEIGHBOURS[6] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

BlockId block_at(World& w, glm::ivec3 p) {
    if (p.y < 0 || p.y >= HEIGHT) return AIR;
    return w.get_block(p.x, p.y, p.z);
}

}  // namespace

bool is_log(BlockId b) { return b == LOG || b == BIRCH_LOG || b == SPRUCE_LOG; }

bool log_near(World& w, glm::ivec3 start) {
    // Breadth first out through leaves: a leaf k steps from `start` with a log beside it puts the
    // log k + 1 away.
    std::vector<glm::ivec3> frontier{start};
    std::set<std::tuple<int, int, int>> seen{{start.x, start.y, start.z}};
    for (int k = 0; k < LEAF_REACH && !frontier.empty(); ++k) {
        std::vector<glm::ivec3> next;
        for (glm::ivec3 p : frontier)
            for (glm::ivec3 d : NEIGHBOURS) {
                const glm::ivec3 q = p + d;
                const BlockId b = block_at(w, q);
                if (is_log(b)) return true;
                if (is_leaves(b) && seen.insert({q.x, q.y, q.z}).second) next.push_back(q);
            }
        frontier = std::move(next);
    }
    return false;
}

int mark_leaves(World& w, glm::ivec3 pos) {
    int marked = 0;
    for (int dy = -LEAF_REACH; dy <= LEAF_REACH; ++dy) {
        const int y = pos.y + dy;
        if (y < 0 || y >= HEIGHT) continue;
        for (int dz = -LEAF_REACH; dz <= LEAF_REACH; ++dz)
            for (int dx = -LEAF_REACH; dx <= LEAF_REACH; ++dx) {
                const int x = pos.x + dx, z = pos.z + dz;
                if (!is_leaves(w.get_block(x, y, z))) continue;
                const int data = w.get_data(x, y, z);
                if (data & (LEAVES_CHECK | LEAVES_PLAYER)) continue;
                w.set_data(x, y, z, data | LEAVES_CHECK, false);
                ++marked;
            }
    }
    return marked;
}

std::vector<ItemId> leaf_drops(BlockId leaves, Rng& rng) {
    std::vector<ItemId> items;
    if (rng.below(20) == 0) items.push_back(sapling_for(leaves));
    if (leaves == LEAVES && rng.random() < 0.1) items.push_back(APPLE);
    return items;
}

bool cactus_can_stay(World& w, glm::ivec3 p) {
    if (p.y <= 0 || p.y >= HEIGHT) return false;
    for (glm::ivec3 d : {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)})
        if (is_solid(block_at(w, p + d))) return false;
    const BlockId under = block_at(w, p - glm::ivec3(0, 1, 0));
    return under == SAND || under == CACTUS;
}

bool snow_can_stay(World& w, glm::ivec3 p) {
    if (p.y <= 0 || p.y >= HEIGHT) return false;
    const BlockId under = block_at(w, p - glm::ivec3(0, 1, 0));
    return under != ICE && (is_leaves(under) || is_opaque(under));
}

bool melts(World& w, glm::ivec3 p) {
    const BlockId b = block_at(w, p);
    if (b != ICE && b != SNOW_LAYER) return false;
    return w.block_light(p.x, p.y, p.z) > MELT_LIGHT - light_opacity(b);
}

bool touches_cactus(World& w, const Body& b) {
    constexpr double IN = 1.0 / 16 - 0.001;  // the cactus box is 1/16 inside its cell
    return w.box_in(CACTUS, b.x - b.half - IN, b.y + 0.001, b.z - b.half - IN, b.x + b.half + IN - 1e-6,
                    b.y + b.height - 0.001, b.z + b.half + IN - 1e-6);
}
