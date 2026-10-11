#include "building.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "bed.h"
#include "painting.h"
#include "world.h"

namespace {

const glm::ivec3 UP(0, 1, 0);

BlockId at(World& w, glm::ivec3 p) { return w.get_block(p.x, p.y, p.z); }
int data_at(World& w, glm::ivec3 p) { return w.get_data(p.x, p.y, p.z); }

// A box flush with one side of the cell, `t` thick, from y0 to y1.
Box against(int facing, float t, float y0 = 0, float y1 = 1) {
    const glm::ivec3 d = facing_dir(facing);
    Box b{{0, y0, 0}, {1, y1, 1}};
    if (d.x > 0) b.lo.x = 1 - t;
    if (d.x < 0) b.hi.x = t;
    if (d.z > 0) b.lo.z = 1 - t;
    if (d.z < 0) b.hi.z = t;
    return b;
}

constexpr float DOOR_THICKNESS = 3.0f / 16;

// The side a door's panel is on: next to the player who placed it while shut, swung against the
// hinge side (on their left) when open, like Beta.
int door_side(int data) { return door_open(data) ? ((data & 3) + 3) & 3 : ((data & 3) + 2) & 3; }

// The facing of a sign post, in 16ths of a turn, that turns its front towards a player looking
// along `yaw`.
int sign_rotation(double yaw) {
    const int look = static_cast<int>(std::floor(-yaw / 22.5 + 0.5)) & 15;
    return (look + 8) & 15;
}

bool is_chest(World& w, glm::ivec3 p) { return at(w, p) == CHEST; }

// The chests right beside a cell (left, right, front, back).
std::vector<glm::ivec3> chest_neighbours(World& w, glm::ivec3 p) {
    std::vector<glm::ivec3> out;
    for (int f = 0; f < 4; ++f)
        if (is_chest(w, p + facing_dir(f))) out.push_back(p + facing_dir(f));
    return out;
}

}  // namespace

glm::ivec3 facing_dir(int facing) { return FACING_DIRS[facing & 3]; }

int facing_of(glm::ivec3 dir) {
    for (int f = 0; f < 4; ++f)
        if (FACING_DIRS[f] == dir) return f;
    return -1;
}

bool door_open(int data) { return (data & DOOR_OPEN) != 0; }

glm::ivec3 door_other_half(glm::ivec3 pos, int data) { return (data & DOOR_UPPER) ? pos - UP : pos + UP; }

FenceLinks fence_links(World& world, glm::ivec3 pos) {
    FenceLinks links{};
    for (int f = 0; f < 4; ++f) links[f] = at(world, pos + facing_dir(f)) == FENCE;
    return links;
}

std::vector<Box> shape_boxes(BlockId block, int data, const FenceLinks& links, bool collide) {
    switch (block) {
        case SLAB: return {{{0, 0, 0}, {1, 0.5f, 1}}};
        case WOOD_STAIRS: case COBBLE_STAIRS: return {{{0, 0, 0}, {1, 0.5f, 1}}, against(data, 0.5f, 0.5f, 1)};
        case FENCE: {
            constexpr float A = 6.0f / 16, B = 10.0f / 16;
            Box b{{A, 0, A}, {B, collide ? 1.5f : 1.0f, B}};
            for (int f = 0; f < 4; ++f) {
                if (!links[f]) continue;
                const glm::ivec3 d = facing_dir(f);
                if (d.x > 0) b.hi.x = 1;
                if (d.x < 0) b.lo.x = 0;
                if (d.z > 0) b.hi.z = 1;
                if (d.z < 0) b.lo.z = 0;
            }
            return {b};
        }
        case LADDER: return {against(data, 2.0f / 16)};
        case WOOD_DOOR_BLOCK: case IRON_DOOR_BLOCK: return {against(door_side(data), DOOR_THICKNESS)};
        case TRAPDOOR:
            if (data & TRAPDOOR_OPEN) return {against(data, DOOR_THICKNESS)};
            return {{{0, 0, 0}, {1, DOOR_THICKNESS, 1}}};
        case SIGN_POST:
            if (collide) return {};
            return {{{0.25f, 0, 0.25f}, {0.75f, 1, 0.75f}}};
        case WALL_SIGN:
            if (collide) return {};
            return {against(data, 2.0f / 16, 0.28125f, 0.78125f)};
        case PAINTING_BLOCK:
            if (collide) return {};
            return {against(data, 1.0f / 16)};
        default: return {{{0, 0, 0}, {1, 1, 1}}};
    }
}

std::vector<Box> block_boxes(World& world, glm::ivec3 pos, bool collide) {
    const BlockId b = at(world, pos);
    if (b == AIR || is_fluid(b)) return {};
    if (is_shaped(b)) {
        const FenceLinks links = b == FENCE ? fence_links(world, pos) : FenceLinks{};
        return shape_boxes(b, data_at(world, pos), links, collide);
    }
    if (collide) {
        if (!is_solid(b)) return {};
        return {{{0, 0, 0}, {1, block_height(b), 1}}};
    }
    return {{{0, 0, 0}, {1, 1, 1}}};
}

std::optional<float> ray_boxes(const std::vector<Box>& boxes, glm::vec3 origin, glm::vec3 dir, glm::ivec3* face) {
    std::optional<float> best;
    for (const Box& b : boxes) {
        float t0 = 0, t1 = std::numeric_limits<float>::infinity();
        int axis = -1;  // the side the ray comes in through
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            if (std::abs(dir[a]) < 1e-9f) {
                hit = origin[a] >= b.lo[a] && origin[a] <= b.hi[a];
                continue;
            }
            float ta = (b.lo[a] - origin[a]) / dir[a], tb = (b.hi[a] - origin[a]) / dir[a];
            if (ta > tb) std::swap(ta, tb);
            if (ta > t0) {
                t0 = ta;
                axis = a;
            }
            t1 = std::min(t1, tb);
            hit = t0 <= t1;
        }
        if (hit && (!best || t0 < *best)) {
            best = t0;
            if (face) {
                *face = glm::ivec3(0);
                if (axis >= 0) (*face)[axis] = dir[axis] > 0 ? -1 : 1;
            }
        }
    }
    return best;
}

// --- Placing ---------------------------------------------------------------------------------

bool cell_free(World& world, glm::ivec3 pos) {
    if (pos.y < 0 || pos.y >= HEIGHT) return false;
    const BlockId b = at(world, pos);
    return b == AIR || is_fluid(b) || b == FIRE;
}

bool supported(World& world, glm::ivec3 pos, BlockId block, int data) {
    switch (block) {
        case LADDER: case TRAPDOOR: return is_opaque(at(world, pos + facing_dir(data)));
        case WALL_SIGN: case PAINTING_BLOCK: return is_solid(at(world, pos + facing_dir(data)));
        case SIGN_POST: return is_solid(at(world, pos - UP));
        case WOOD_DOOR_BLOCK: case IRON_DOOR_BLOCK:
            if (data & DOOR_UPPER) return at(world, pos - UP) == block;
            return is_opaque(at(world, pos - UP));
        default: return true;
    }
}

std::optional<std::vector<PlacedCell>> placement(World& world, ItemId item, glm::ivec3 clicked, glm::ivec3 face, double yaw) {
    const glm::ivec3 target = clicked + face;
    const bool side = face.y == 0;
    // Slabs: a slab of the same kind under the new one makes a double slab of the two (Beta).
    if (item >= STONE_SLAB && item <= COBBLE_SLAB) {
        const int kind = item - STONE_SLAB;
        if (!cell_free(world, target)) return std::nullopt;
        const glm::ivec3 below = target - UP;
        if (at(world, below) == SLAB && data_at(world, below) == kind) return std::vector<PlacedCell>{{below, DOUBLE_SLAB, kind}};
        return std::vector<PlacedCell>{{target, SLAB, kind}};
    }
    if (!cell_free(world, target)) return std::nullopt;
    if (item >= WHITE_WOOL && item <= BLACK_WOOL) return std::vector<PlacedCell>{{target, WOOL, item - WHITE_WOOL}};
    std::optional<PlacedCell> cell;
    switch (item) {
        case WOOD_STAIRS: case COBBLE_STAIRS:
            cell = PlacedCell{target, static_cast<BlockId>(item), facing_from_yaw(yaw)};
            break;
        case FENCE:
            // Beta: a fence goes on something solid (another fence is solid).
            if (!is_solid(at(world, target - UP))) return std::nullopt;
            cell = PlacedCell{target, FENCE, 0};
            break;
        case LADDER: case TRAPDOOR:
            // On the side of a full block only, like Beta 1.7.3.
            if (!side) return std::nullopt;
            cell = PlacedCell{target, static_cast<BlockId>(item), facing_of(-face)};
            break;
        case SIGN:
            if (face.y < 0) return std::nullopt;
            cell = side ? PlacedCell{target, WALL_SIGN, facing_of(-face)} : PlacedCell{target, SIGN_POST, sign_rotation(yaw)};
            break;
        case WOOD_DOOR: case IRON_DOOR: {
            if (face != UP || !cell_free(world, target + UP)) return std::nullopt;
            const BlockId door = item == WOOD_DOOR ? WOOD_DOOR_BLOCK : IRON_DOOR_BLOCK;
            const int dir = facing_from_yaw(yaw);
            if (!is_opaque(at(world, target - UP))) return std::nullopt;
            return std::vector<PlacedCell>{{target, door, dir}, {target + UP, door, dir | DOOR_UPPER}};
        }
        case CHEST: {
            // Beta: two chests side by side join into a double chest, but never three in a row.
            const auto next = chest_neighbours(world, target);
            if (next.size() > 1) return std::nullopt;
            int front = (facing_from_yaw(yaw) + 2) & 3;
            if (next.empty()) return std::vector<PlacedCell>{{target, CHEST, front}};
            const glm::ivec3 other = next[0];
            if (!chest_neighbours(world, other).empty() && chest_neighbours(world, other) != std::vector<glm::ivec3>{target})
                return std::nullopt;
            // A double chest's front is along its long side; both halves face the same way.
            const glm::ivec3 axis = other - target;
            if (facing_dir(front) == axis || facing_dir(front) == -axis) {
                const int theirs = data_at(world, other) & 3;
                front = (facing_dir(theirs) == axis || facing_dir(theirs) == -axis) ? (front + 1) & 3 : theirs;
            }
            return std::vector<PlacedCell>{{target, CHEST, front}, {other, CHEST, front}};
        }
        default:
            if (!is_block(item)) return std::nullopt;
            cell = PlacedCell{target, static_cast<BlockId>(item), 0};
            break;
    }
    if (!supported(world, cell->pos, cell->block, cell->data)) return std::nullopt;
    return std::vector<PlacedCell>{*cell};
}

// --- Using ---------------------------------------------------------------------------------

std::optional<std::vector<PlacedCell>> toggle_door(World& world, glm::ivec3 pos) {
    const BlockId b = at(world, pos);
    const int data = data_at(world, pos);
    if (b == TRAPDOOR) return std::vector<PlacedCell>{{pos, TRAPDOOR, data ^ TRAPDOOR_OPEN}};
    if (b == IRON_DOOR_BLOCK) return std::vector<PlacedCell>{};  // needs redstone
    if (b != WOOD_DOOR_BLOCK) return std::nullopt;
    std::vector<PlacedCell> out{{pos, b, data ^ DOOR_OPEN}};
    const glm::ivec3 other = door_other_half(pos, data);
    if (at(world, other) == b) out.push_back({other, b, data_at(world, other) ^ DOOR_OPEN});
    return out;
}

// --- Breaking ------------------------------------------------------------------------------

std::optional<std::vector<ItemId>> data_drops(BlockId block, int data) {
    switch (block) {
        case WOOL: return std::vector<ItemId>{WHITE_WOOL + (data & 15)};
        case SLAB: return std::vector<ItemId>{STONE_SLAB + (data & 3)};
        case DOUBLE_SLAB: return std::vector<ItemId>(2, STONE_SLAB + (data & 3));
        case WOOD_DOOR_BLOCK: return std::vector<ItemId>{WOOD_DOOR};
        case IRON_DOOR_BLOCK: return std::vector<ItemId>{IRON_DOOR};
        case SIGN_POST: case WALL_SIGN: return std::vector<ItemId>{SIGN};
        case PAINTING_BLOCK: return std::vector<ItemId>{PAINTING};
        default: return std::nullopt;
    }
}

std::vector<glm::ivec3> joined_cells(World& world, glm::ivec3 pos) {
    const BlockId b = at(world, pos);
    if (is_door(b)) {
        const glm::ivec3 other = door_other_half(pos, data_at(world, pos));
        if (at(world, other) == b) return {pos, other};
        return {pos};
    }
    if (b == PAINTING_BLOCK) return painting_cells(world, pos);
    return {pos};
}

std::vector<std::pair<glm::ivec3, ItemId>> fall_off(World& world, glm::ivec3 pos) {
    static const glm::ivec3 NEIGHBOURS[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    std::vector<std::pair<glm::ivec3, ItemId>> out;
    for (glm::ivec3 d : NEIGHBOURS) {
        const glm::ivec3 n = pos + d;
        const BlockId b = at(world, n);
        if (b != LADDER && b != TRAPDOOR && b != WALL_SIGN && b != SIGN_POST && b != PAINTING_BLOCK && !is_door(b)) continue;
        const int data = data_at(world, n);
        if (supported(world, n, b, data)) continue;
        // A door's top half without its bottom (or the other way round) goes too, dropping nothing more.
        ItemId drop = b;
        if (auto items = data_drops(b, data)) drop = items->front();
        out.push_back({n, drop});
    }
    // A door half whose other half went.
    for (glm::ivec3 d : {UP, -UP}) {
        const glm::ivec3 n = pos + d;
        const BlockId b = at(world, n);
        if (!is_door(b)) continue;
        const int data = data_at(world, n);
        if (door_other_half(n, data) == pos && at(world, pos) != b &&
            std::none_of(out.begin(), out.end(), [&](const auto& o) { return o.first == n; }))
            out.push_back({n, b == WOOD_DOOR_BLOCK ? WOOD_DOOR : IRON_DOOR});
    }
    return out;
}

int note_instrument(BlockId below) {
    if (is_furnace(below)) return 1;
    switch (below) {
        case STONE: case COBBLE: case BEDROCK: case BRICK: case SANDSTONE: case MOSSY_COBBLE: case SPAWNER: case OBSIDIAN:
        case COAL_ORE: case IRON_ORE: case GOLD_ORE: case DIAMOND_ORE: case REDSTONE_ORE: case LAPIS_ORE:
        case NETHERRACK: case SLAB: case DOUBLE_SLAB: case COBBLE_STAIRS:
            return 1;
        case SAND: case GRAVEL: case SOUL_SAND: return 2;
        case GLASS: case GLOWSTONE: return 3;
        case PLANKS: case LOG: case BIRCH_LOG: case SPRUCE_LOG: case CRAFTING_TABLE: case CHEST: case BOOKSHELF:
        case NOTE_BLOCK: case JUKEBOX: case WOOD_STAIRS: case FENCE: case WOOD_DOOR_BLOCK: case TRAPDOOR:
        case SIGN_POST: case WALL_SIGN:
            return 4;
        default: return 0;
    }
}

bool climbable(BlockId block) { return block == LADDER; }
