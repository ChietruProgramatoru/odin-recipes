#include "rails.h"

#include <algorithm>

namespace {

// Beta's EntityMinecart matrix: the ends of each shape, relative to where a minecart rides on it
// (one block up on a slope, so the low end is at -1).
const std::array<std::array<glm::ivec3, 2>, 10> ENDS = {{
    {{{0, 0, -1}, {0, 0, 1}}},
    {{{-1, 0, 0}, {1, 0, 0}}},
    {{{-1, -1, 0}, {1, 0, 0}}},
    {{{-1, 0, 0}, {1, -1, 0}}},
    {{{0, 0, -1}, {0, -1, 1}}},
    {{{0, -1, -1}, {0, 0, 1}}},
    {{{0, 0, 1}, {1, 0, 0}}},
    {{{0, 0, 1}, {-1, 0, 0}}},
    {{{0, 0, -1}, {-1, 0, 0}}},
    {{{0, 0, -1}, {1, 0, 0}}},
}};

bool rail_at(World& w, glm::ivec3 p) { return p.y >= 0 && p.y < HEIGHT && is_rail(w.get_block(p.x, p.y, p.z)); }

// One rail and the cells it joins (Beta's RailLogic).
struct Logic {
    World& w;
    glm::ivec3 p;
    BlockId block;
    std::vector<glm::ivec3> connected;

    Logic(World& world, glm::ivec3 pos) : w(world), p(pos), block(w.get_block(pos.x, pos.y, pos.z)) {
        set_ends(rail_shape(block, w.get_data(p.x, p.y, p.z)));
    }
    bool curves() const { return rail_can_curve(block); }

    // The cells a shape joins: beside it, and one up on the high side of a slope.
    void set_ends(int shape) {
        connected.clear();
        auto add = [&](int dx, int dy, int dz) { connected.push_back(p + glm::ivec3(dx, dy, dz)); };
        switch (shape) {
            case RAIL_NORTH_SOUTH: add(0, 0, -1), add(0, 0, 1); break;
            case RAIL_EAST_WEST: add(-1, 0, 0), add(1, 0, 0); break;
            case RAIL_ASCENDING_EAST: add(-1, 0, 0), add(1, 1, 0); break;
            case RAIL_ASCENDING_WEST: add(-1, 1, 0), add(1, 0, 0); break;
            case RAIL_ASCENDING_NORTH: add(0, 1, -1), add(0, 0, 1); break;
            case RAIL_ASCENDING_SOUTH: add(0, 0, -1), add(0, 1, 1); break;
            case RAIL_SOUTH_EAST: add(1, 0, 0), add(0, 0, 1); break;
            case RAIL_SOUTH_WEST: add(-1, 0, 0), add(0, 0, 1); break;
            case RAIL_NORTH_WEST: add(-1, 0, 0), add(0, 0, -1); break;
            case RAIL_NORTH_EAST: add(1, 0, 0), add(0, 0, -1); break;
        }
    }
    // Joined to the column of `q` (at any height).
    bool joins(glm::ivec3 q) const {
        return std::any_of(connected.begin(), connected.end(), [&](glm::ivec3 c) { return c.x == q.x && c.z == q.z; });
    }
    // Forget ends with no rail joined back to this one.
    void refresh() {
        for (size_t i = 0; i < connected.size();) {
            auto other = rail_near(w, connected[i]);
            if (other && Logic(w, *other).joins(p)) {
                connected[i++] = *other;
            } else {
                connected.erase(connected.begin() + static_cast<long>(i));
            }
        }
    }
    bool can_join(const Logic& other) const { return joins(other.p) || connected.size() < 2; }

    // A shape from which sides are joined, with slopes up to rails one block higher.
    int shape_for(int shape) const {
        if (shape == RAIL_NORTH_SOUTH) {
            if (rail_at(w, p + glm::ivec3(0, 1, -1))) shape = RAIL_ASCENDING_NORTH;
            if (rail_at(w, p + glm::ivec3(0, 1, 1))) shape = RAIL_ASCENDING_SOUTH;
        }
        if (shape == RAIL_EAST_WEST) {
            if (rail_at(w, p + glm::ivec3(1, 1, 0))) shape = RAIL_ASCENDING_EAST;
            if (rail_at(w, p + glm::ivec3(-1, 1, 0))) shape = RAIL_ASCENDING_WEST;
        }
        return shape < 0 ? RAIL_NORTH_SOUTH : shape;
    }
    std::vector<ChunkPos> store(int shape) {
        set_ends(shape);
        const int old = w.get_data(p.x, p.y, p.z);
        const int data = curves() ? shape : (shape | (old & RAIL_POWERED));
        if (data == old) return {};
        return w.set_data(p.x, p.y, p.z, data);
    }

    // Join a neighbour that was just placed (Beta's RailLogic.func_788_d).
    std::vector<ChunkPos> join(const Logic& other) {
        connected.push_back(other.p);
        const bool n = joins(p + glm::ivec3(0, 0, -1)), s = joins(p + glm::ivec3(0, 0, 1));
        const bool west = joins(p + glm::ivec3(-1, 0, 0)), e = joins(p + glm::ivec3(1, 0, 0));
        int shape = -1;
        if (n || s) shape = RAIL_NORTH_SOUTH;
        if (west || e) shape = RAIL_EAST_WEST;
        if (curves()) {
            if (s && e && !n && !west) shape = RAIL_SOUTH_EAST;
            if (s && west && !n && !e) shape = RAIL_SOUTH_WEST;
            if (n && west && !s && !e) shape = RAIL_NORTH_WEST;
            if (n && e && !s && !west) shape = RAIL_NORTH_EAST;
        }
        return store(shape_for(shape));
    }
};

// Is there a rail beside `self` at `q` that will join it?
bool joinable(World& w, const Logic& self, glm::ivec3 q) {
    auto at = rail_near(w, q);
    if (!at) return false;
    Logic other(w, *at);
    other.refresh();
    return other.can_join(self);
}

void append(std::vector<ChunkPos>& to, const std::vector<ChunkPos>& more) { to.insert(to.end(), more.begin(), more.end()); }

}  // namespace

const std::array<glm::ivec3, 2>& rail_ends(int shape) { return ENDS[std::clamp(shape, 0, 9)]; }

std::optional<glm::ivec3> rail_near(World& world, glm::ivec3 p) {
    for (int dy : {0, 1, -1})
        if (rail_at(world, p + glm::ivec3(0, dy, 0))) return p + glm::ivec3(0, dy, 0);
    return std::nullopt;
}

bool rail_can_stay(World& world, glm::ivec3 p, int shape) {
    auto solid = [&](int dx, int dy, int dz) {
        const glm::ivec3 q = p + glm::ivec3(dx, dy, dz);
        return q.y >= 0 && q.y < HEIGHT && is_opaque(world.get_block(q.x, q.y, q.z));
    };
    if (!solid(0, -1, 0)) return false;
    switch (shape) {
        case RAIL_ASCENDING_EAST: return solid(1, 0, 0);
        case RAIL_ASCENDING_WEST: return solid(-1, 0, 0);
        case RAIL_ASCENDING_NORTH: return solid(0, 0, -1);
        case RAIL_ASCENDING_SOUTH: return solid(0, 0, 1);
        default: return true;
    }
}

bool rail_can_stay(World& world, glm::ivec3 p) {
    const BlockId b = world.get_block(p.x, p.y, p.z);
    return rail_can_stay(world, p, rail_shape(b, world.get_data(p.x, p.y, p.z)));
}

std::vector<ChunkPos> connect_rail(World& world, glm::ivec3 p) {
    Logic self(world, p);
    const bool n = joinable(world, self, p + glm::ivec3(0, 0, -1)), s = joinable(world, self, p + glm::ivec3(0, 0, 1));
    const bool w = joinable(world, self, p + glm::ivec3(-1, 0, 0)), e = joinable(world, self, p + glm::ivec3(1, 0, 0));
    // Beta's RailLogic.func_792_a, not powered by redstone.
    int shape = -1;
    if ((n || s) && !w && !e) shape = RAIL_NORTH_SOUTH;
    if ((w || e) && !n && !s) shape = RAIL_EAST_WEST;
    if (self.curves()) {
        if (s && e && !n && !w) shape = RAIL_SOUTH_EAST;
        if (s && w && !n && !e) shape = RAIL_SOUTH_WEST;
        if (n && w && !s && !e) shape = RAIL_NORTH_WEST;
        if (n && e && !s && !w) shape = RAIL_NORTH_EAST;
    }
    if (shape == -1) {
        if (n || s) shape = RAIL_NORTH_SOUTH;
        if (w || e) shape = RAIL_EAST_WEST;
        if (self.curves()) {
            if (n && w) shape = RAIL_NORTH_WEST;
            if (e && n) shape = RAIL_NORTH_EAST;
            if (w && s) shape = RAIL_SOUTH_WEST;
            if (s && e) shape = RAIL_SOUTH_EAST;
        }
    }
    shape = self.shape_for(shape);
    // A slope needs a solid block on its high side; without one the rail stays flat.
    if (rail_sloped(shape) && !rail_can_stay(world, p, shape)) shape = shape <= RAIL_ASCENDING_WEST ? RAIL_EAST_WEST : RAIL_NORTH_SOUTH;
    std::vector<ChunkPos> dirty = self.store(shape);
    // Bend the neighbours this rail points at so they join it.
    for (glm::ivec3 c : std::vector<glm::ivec3>(self.connected)) {
        auto at = rail_near(world, c);
        if (!at) continue;
        Logic other(world, *at);
        other.refresh();
        if (other.can_join(self)) append(dirty, other.join(self));
    }
    return dirty;
}

std::vector<ChunkPos> set_rail_powered(World& world, glm::ivec3 p, bool powered) {
    const BlockId b = world.get_block(p.x, p.y, p.z);
    if (!is_rail(b) || rail_can_curve(b)) return {};
    const int data = world.get_data(p.x, p.y, p.z);
    const int want = powered ? (data | RAIL_POWERED) : (data & ~RAIL_POWERED);
    if (want == data) return {};
    return world.set_data(p.x, p.y, p.z, want);
}
