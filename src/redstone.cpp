#include "redstone.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

#include "bed.h"
#include "rails.h"

namespace {

constexpr glm::ivec3 UP(0, 1, 0);
constexpr glm::ivec3 NEIGHBOURS[6] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

BlockId at(World& w, glm::ivec3 p) { return w.get_block(p.x, p.y, p.z); }
int data_at(World& w, glm::ivec3 p) { return w.get_data(p.x, p.y, p.z); }

// A box reaching from the side of the cell opposite `d` (t0 = 0) towards `d` (t1 = 1), from a0 to a1 across.
Box along(glm::ivec3 d, float t0, float t1, float a0, float a1) {
    Box b{glm::vec3(a0), glm::vec3(a1)};
    for (int axis = 0; axis < 3; ++axis) {
        if (d[axis] > 0) b.lo[axis] = t0, b.hi[axis] = t1;
        if (d[axis] < 0) b.lo[axis] = 1 - t1, b.hi[axis] = 1 - t0;
    }
    return b;
}

// Something that dust joins to on its own: a power source (a repeater only along its line).
bool dust_joins_block(BlockId b, int data, glm::ivec3 d) {
    switch (b) {
        case REDSTONE_WIRE: case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF: case LEVER_BLOCK: case BUTTON_BLOCK:
        case STONE_PLATE_BLOCK: case WOOD_PLATE_BLOCK: case DETECTOR_RAIL:
            return true;
        case REPEATER_OFF: case REPEATER_ON: {
            const glm::ivec3 f = facing_dir(repeater_facing(data));
            return f == d || f == -d;
        }
        default: return false;
    }
}

// A piston pushes it out of the way, breaking it (it drops its item): fluids, fire, plants, torches,
// dust and the other small redstone parts, rails, ladders, snow, cactus, cake.
bool piston_breaks(BlockId b) {
    return is_fluid(b) || b == FIRE || is_plant(b) || is_torch(b) || is_redstone_thin(b) || is_repeater(b) || is_rail(b) ||
           b == LADDER || b == SNOW_LAYER || b == CACTUS || b == CAKE_BLOCK;
}

// Can a piston move it? Not unbreakable blocks, obsidian, anything that keeps things (chests,
// furnaces, dispensers, note blocks, jukeboxes, signs, spawners), blocks of two cells (doors,
// beds, paintings), trapdoors or a piston that is pushed out (Beta).
bool piston_moves(World& w, glm::ivec3 p) {
    const BlockId b = at(w, p);
    const BlockInfo* info = block_info(b);
    if (!info || !info->hardness || b == OBSIDIAN || b == PISTON_HEAD) return false;
    if (is_piston(b) && (data_at(w, p) & PISTON_EXTENDED)) return false;
    if (b == CHEST || is_furnace(b) || b == DISPENSER_BLOCK || b == NOTE_BLOCK || b == JUKEBOX || b == SPAWNER ||
        b == SIGN_POST || b == WALL_SIGN || b == PAINTING_BLOCK || is_door(b) || is_bed(b) || b == TRAPDOOR)
        return false;
    return true;
}

// What a block broken by a piston drops (fluids and fire nothing).
std::optional<ItemId> broken_drop(BlockId b, int data) {
    if (is_fluid(b) || b == FIRE) return std::nullopt;
    if (is_torch(b)) return TORCH;
    if (auto d = redstone_drop(b, data)) return d;
    if (const BlockInfo* info = block_info(b)) return info->drop;
    return std::nullopt;
}

using Key = std::tuple<int, int, int>;
Key key(glm::ivec3 p) { return {p.x, p.y, p.z}; }

}  // namespace

glm::ivec3 dir6(int d) { return NEIGHBOURS[std::clamp(d, 0, 5)]; }

int dir6_of(glm::ivec3 v) {
    for (int d = 0; d < 6; ++d)
        if (NEIGHBOURS[d] == v) return d;
    return -1;
}

bool is_redstone_torch(BlockId b) { return b == REDSTONE_TORCH_ON || b == REDSTONE_TORCH_OFF; }
bool is_repeater(BlockId b) { return b == REPEATER_OFF || b == REPEATER_ON; }
bool is_plate(BlockId b) { return b == STONE_PLATE_BLOCK || b == WOOD_PLATE_BLOCK; }
bool is_piston(BlockId b) { return b == PISTON_BASE || b == STICKY_PISTON_BASE; }
bool is_redstone_shaped(BlockId b) {
    return b == REDSTONE_WIRE || b == LEVER_BLOCK || b == BUTTON_BLOCK || is_plate(b) || is_repeater(b) || b == PISTON_HEAD;
}
bool is_redstone_thin(BlockId b) {
    return b == REDSTONE_WIRE || is_redstone_torch(b) || b == LEVER_BLOCK || b == BUTTON_BLOCK || is_plate(b);
}
bool is_redstone_component(BlockId b) {
    return b == REDSTONE_WIRE || is_redstone_torch(b) || b == LEVER_BLOCK || b == BUTTON_BLOCK || is_plate(b) ||
           is_repeater(b) || b == DETECTOR_RAIL;
}
bool is_redstone_item(ItemId item) {
    switch (item) {
        case REDSTONE: case REDSTONE_TORCH: case LEVER: case STONE_BUTTON: case STONE_PLATE: case WOOD_PLATE:
        case REPEATER: case PISTON: case STICKY_PISTON: case DISPENSER:
            return true;
        default: return false;
    }
}

int repeater_delay(int data) { return ((data >> 2) & 3) + 1; }
int repeater_facing(int data) { return data & 3; }

std::vector<Box> redstone_boxes(BlockId block, int data, bool collide) {
    constexpr float PX = 1.0f / 16;
    switch (block) {
        case REDSTONE_WIRE:
            if (collide) return {};
            return {{{0, 0, 0}, {1, PX, 1}}};
        case LEVER_BLOCK: {
            if (collide) return {};
            const int attach = data & 7;
            if (attach == 0) return {{{0.25f, 0, 0.25f}, {0.75f, 0.6f, 0.75f}}};
            return {along(-facing_dir(attach - 1), 0, 6 * PX, 5 * PX, 11 * PX)};
        }
        case BUTTON_BLOCK: {
            if (collide) return {};
            Box b = along(-facing_dir(data & 3), 0, (data & BUTTON_PRESSED) ? PX : 2 * PX, 5 * PX, 11 * PX);
            b.lo.y = 6 * PX;
            b.hi.y = 10 * PX;
            return {b};
        }
        case STONE_PLATE_BLOCK: case WOOD_PLATE_BLOCK:
            if (collide) return {};
            return {{{PX, 0, PX}, {15 * PX, (data & PLATE_PRESSED) ? PX / 2 : PX, 15 * PX}}};
        case REPEATER_OFF: case REPEATER_ON: return {{{0, 0, 0}, {1, 2 * PX, 1}}};
        case PISTON_HEAD: {
            const glm::ivec3 d = dir6(data & 7);
            return {along(d, 12 * PX, 1, 0, 1), along(d, 0, 12 * PX, 6 * PX, 10 * PX)};
        }
        default: return {{{0, 0, 0}, {1, 1, 1}}};
    }
}

glm::ivec3 redstone_support(BlockId block, int data) {
    switch (block) {
        case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF: case LEVER_BLOCK: {
            const int attach = data & 7;
            return attach == 0 ? -UP : facing_dir(attach - 1);
        }
        case BUTTON_BLOCK: return facing_dir(data & 3);
        default: return -UP;
    }
}

bool redstone_can_stay(World& world, glm::ivec3 pos) {
    const BlockId b = at(world, pos);
    if (!is_redstone_thin(b) && !is_repeater(b)) return true;
    return is_opaque(at(world, pos + redstone_support(b, data_at(world, pos))));
}

std::optional<PlacedCell> redstone_placement(World& world, ItemId item, glm::ivec3 clicked, glm::ivec3 face, double yaw,
                                             double pitch) {
    const glm::ivec3 target = clicked + face;
    if (!cell_free(world, target)) return std::nullopt;
    const bool on_floor = is_opaque(at(world, target - UP));
    switch (item) {
        case REDSTONE:
            if (!on_floor) return std::nullopt;
            return PlacedCell{target, REDSTONE_WIRE, 0};
        case REDSTONE_TORCH: case LEVER: {
            // On top of or on the side of a full block, like a torch.
            if (face.y < 0 || !is_opaque(at(world, clicked))) return std::nullopt;
            const int attach = face.y > 0 ? 0 : 1 + facing_of(-face);
            return PlacedCell{target, item == LEVER ? LEVER_BLOCK : REDSTONE_TORCH_ON, attach};
        }
        case STONE_BUTTON:
            // Beta: only on the side of a full block.
            if (face.y != 0 || !is_opaque(at(world, clicked))) return std::nullopt;
            return PlacedCell{target, BUTTON_BLOCK, facing_of(-face)};
        case STONE_PLATE: case WOOD_PLATE:
            if (!on_floor) return std::nullopt;
            return PlacedCell{target, item == STONE_PLATE ? STONE_PLATE_BLOCK : WOOD_PLATE_BLOCK, 0};
        case REPEATER:
            // It sends power the way the player looks.
            if (!on_floor) return std::nullopt;
            return PlacedCell{target, REPEATER_OFF, facing_from_yaw(yaw)};
        case PISTON: case STICKY_PISTON: {
            // The front faces the player: up when they look down on it, down when they look up.
            int d = dir6_of(-facing_dir(facing_from_yaw(yaw)));
            if (pitch > 50) d = 1;
            if (pitch < -50) d = 0;
            return PlacedCell{target, item == PISTON ? PISTON_BASE : STICKY_PISTON_BASE, d};
        }
        case DISPENSER: return PlacedCell{target, DISPENSER_BLOCK, (facing_from_yaw(yaw) + 2) & 3};
        default: return std::nullopt;
    }
}

std::optional<ItemId> redstone_drop(BlockId block, int data) {
    switch (block) {
        case REDSTONE_WIRE: return REDSTONE;
        case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF: return REDSTONE_TORCH;
        case LEVER_BLOCK: return LEVER;
        case BUTTON_BLOCK: return STONE_BUTTON;
        case STONE_PLATE_BLOCK: return STONE_PLATE;
        case WOOD_PLATE_BLOCK: return WOOD_PLATE;
        case REPEATER_OFF: case REPEATER_ON: return REPEATER;
        case PISTON_BASE: return PISTON;
        case STICKY_PISTON_BASE: return STICKY_PISTON;
        case PISTON_HEAD: return (data & PISTON_HEAD_STICKY) ? STICKY_PISTON : PISTON;
        case DISPENSER_BLOCK: return DISPENSER;
        default: return std::nullopt;
    }
}

std::vector<glm::ivec3> piston_cells(World& world, glm::ivec3 pos) {
    const BlockId b = at(world, pos);
    const int data = data_at(world, pos);
    if (is_piston(b) && (data & PISTON_EXTENDED)) {
        const glm::ivec3 head = pos + dir6(data & 7);
        if (at(world, head) == PISTON_HEAD && (data_at(world, head) & 7) == (data & 7)) return {pos, head};
    }
    if (b == PISTON_HEAD) {
        const glm::ivec3 base = pos - dir6(data & 7);
        if (is_piston(at(world, base))) return {pos, base};
    }
    return {pos};
}

// --- Power -----------------------------------------------------------------------------------------

bool wire_joins(World& world, glm::ivec3 pos, glm::ivec3 d) {
    const glm::ivec3 n = pos + d;
    const BlockId b = at(world, n);
    if (dust_joins_block(b, data_at(world, n), d)) return true;
    if (!is_opaque(b) && at(world, n - UP) == REDSTONE_WIRE) return true;  // a step down
    if (!is_opaque(at(world, pos + UP)) && at(world, n + UP) == REDSTONE_WIRE) return true;  // a step up
    return false;
}

bool wire_points(World& world, glm::ivec3 pos, glm::ivec3 d) {
    std::array<bool, 4> joins;
    bool any = false;
    for (int f = 0; f < 4; ++f) any |= joins[f] = wire_joins(world, pos, facing_dir(f));
    if (!any) return true;
    const int fd = facing_of(d);
    if (fd < 0) return false;
    const bool line = joins[fd] || joins[(fd + 2) & 3], across = joins[(fd + 1) & 3] || joins[(fd + 3) & 3];
    return line && !across;
}

int power_from(World& world, glm::ivec3 from, glm::ivec3 to, bool wires) {
    const BlockId b = at(world, from);
    const int data = data_at(world, from);
    const glm::ivec3 d = to - from;
    switch (b) {
        case REDSTONE_TORCH_ON: return d == redstone_support(b, data) ? 0 : MAX_POWER;
        case LEVER_BLOCK: return (data & LEVER_ON) ? MAX_POWER : 0;
        case BUTTON_BLOCK: return (data & BUTTON_PRESSED) ? MAX_POWER : 0;
        case STONE_PLATE_BLOCK: case WOOD_PLATE_BLOCK: return (data & PLATE_PRESSED) ? MAX_POWER : 0;
        case DETECTOR_RAIL: return rail_powered(b, data) ? MAX_POWER : 0;
        case REPEATER_ON: return d == facing_dir(repeater_facing(data)) ? MAX_POWER : 0;
        case REDSTONE_WIRE:
            if (!wires || data == 0) return 0;
            if (d == -UP || (d.y == 0 && wire_points(world, from, d))) return data;
            return 0;
        default: return 0;
    }
}

int strong_power(World& world, glm::ivec3 pos, bool wires) {
    if (!is_opaque(at(world, pos))) return 0;
    int best = 0;
    for (glm::ivec3 off : NEIGHBOURS) {
        const glm::ivec3 n = pos + off;
        const BlockId b = at(world, n);
        const int data = data_at(world, n);
        const glm::ivec3 d = -off;  // from the neighbour into this block
        int p = 0;
        switch (b) {
            case REDSTONE_TORCH_ON: p = d == UP ? MAX_POWER : 0; break;
            case LEVER_BLOCK: p = (data & LEVER_ON) && d == redstone_support(b, data) ? MAX_POWER : 0; break;
            case BUTTON_BLOCK: p = (data & BUTTON_PRESSED) && d == redstone_support(b, data) ? MAX_POWER : 0; break;
            case STONE_PLATE_BLOCK: case WOOD_PLATE_BLOCK: p = (data & PLATE_PRESSED) && d == -UP ? MAX_POWER : 0; break;
            case DETECTOR_RAIL: p = rail_powered(b, data) && d == -UP ? MAX_POWER : 0; break;
            case REPEATER_ON: p = d == facing_dir(repeater_facing(data)) ? MAX_POWER : 0; break;
            case REDSTONE_WIRE: p = wires ? power_from(world, n, pos, true) : 0; break;
            default: break;
        }
        best = std::max(best, p);
        if (best == MAX_POWER) break;
    }
    return best;
}

int power_into(World& world, glm::ivec3 pos, bool wires, std::optional<glm::ivec3> except) {
    int best = 0;
    for (glm::ivec3 d : NEIGHBOURS) {
        if (except && d == *except) continue;
        const glm::ivec3 n = pos + d;
        best = std::max(best, power_from(world, n, pos, wires));
        if (best < MAX_POWER && is_opaque(at(world, n))) best = std::max(best, strong_power(world, n, wires));
        if (best == MAX_POWER) break;
    }
    return best;
}

bool rail_gets_power(World& world, glm::ivec3 pos) {
    if (is_powered(world, pos)) return true;
    const int shape = rail_shape(POWERED_RAIL, data_at(world, pos));
    for (glm::ivec3 end : rail_ends(shape)) {
        const glm::ivec3 step(end.x, 0, end.z);
        glm::ivec3 q = pos;
        for (int i = 0; i < 8; ++i) {
            std::optional<glm::ivec3> next;
            for (int dy : {0, 1, -1})
                if (at(world, q + step + glm::ivec3(0, dy, 0)) == POWERED_RAIL) {
                    next = q + step + glm::ivec3(0, dy, 0);
                    break;
                }
            if (!next) break;
            q = *next;
            const int s = rail_shape(POWERED_RAIL, data_at(world, q));
            const bool same_axis = (step.x != 0) == (s == RAIL_EAST_WEST || s == RAIL_ASCENDING_EAST || s == RAIL_ASCENDING_WEST);
            if (!same_axis) break;
            if (is_powered(world, q)) return true;
        }
    }
    return false;
}

// --- Pistons ---------------------------------------------------------------------------------------

bool extend_piston(World& world, glm::ivec3 pos, PistonMove& out) {
    const BlockId b = at(world, pos);
    const int data = data_at(world, pos);
    if (!is_piston(b) || (data & PISTON_EXTENDED)) return false;
    const glm::ivec3 d = dir6(data & 7);
    std::vector<glm::ivec3> line;
    glm::ivec3 c = pos + d;
    while (true) {
        if (c.y < 0 || c.y >= HEIGHT || !world.is_loaded(c.x, c.z)) return false;
        const BlockId here = at(world, c);
        if (here == AIR || piston_breaks(here)) break;
        if (!piston_moves(world, c)) return false;
        line.push_back(c);
        if (static_cast<int>(line.size()) > PISTON_PUSH_LIMIT) return false;
        c += d;
    }
    auto keep = [&](const std::vector<ChunkPos>& chunks) {
        for (ChunkPos p : chunks)
            if (std::find(out.dirty.begin(), out.dirty.end(), p) == out.dirty.end()) out.dirty.push_back(p);
    };
    if (const BlockId end = at(world, c); end != AIR) {
        if (auto drop = broken_drop(end, data_at(world, c))) out.drops.push_back({c, *drop});
        keep(world.set_block(c.x, c.y, c.z, AIR));
    }
    for (size_t i = line.size(); i-- > 0;) {
        const glm::ivec3 from = line[i], to = from + d;
        keep(world.set_block(to.x, to.y, to.z, at(world, from), data_at(world, from)));
        out.moved_to.push_back(to);
    }
    const glm::ivec3 head = pos + d;
    keep(world.set_block(head.x, head.y, head.z, PISTON_HEAD, (data & 7) | (b == STICKY_PISTON_BASE ? PISTON_HEAD_STICKY : 0)));
    out.moved_to.push_back(head);
    keep(world.set_data(pos.x, pos.y, pos.z, data | PISTON_EXTENDED));
    out.dir = d;
    out.piston = pos;
    out.extended = true;
    return true;
}

void retract_piston(World& world, glm::ivec3 pos, PistonMove& out) {
    const BlockId b = at(world, pos);
    const int data = data_at(world, pos);
    if (!is_piston(b) || !(data & PISTON_EXTENDED)) return;
    auto keep = [&](const std::vector<ChunkPos>& chunks) {
        for (ChunkPos p : chunks)
            if (std::find(out.dirty.begin(), out.dirty.end(), p) == out.dirty.end()) out.dirty.push_back(p);
    };
    const glm::ivec3 d = dir6(data & 7), head = pos + d;
    keep(world.set_data(pos.x, pos.y, pos.z, data & ~PISTON_EXTENDED));
    if (at(world, head) == PISTON_HEAD) keep(world.set_block(head.x, head.y, head.z, AIR));
    out.dir = -d;
    out.piston = pos;
    if (b != STICKY_PISTON_BASE) return;
    // A sticky piston pulls the block in front of its head back with it.
    const glm::ivec3 q = head + d;
    const BlockId pulled = at(world, q);
    if (q.y < 0 || q.y >= HEIGHT || pulled == AIR || piston_breaks(pulled) || !piston_moves(world, q)) return;
    if (at(world, head) != AIR) return;
    keep(world.set_block(head.x, head.y, head.z, pulled, data_at(world, q)));
    keep(world.set_block(q.x, q.y, q.z, AIR));
    out.moved_to.push_back(head);
}

// --- The ticking -------------------------------------------------------------------------------------

void Redstone::schedule(glm::ivec3 p, long due) {
    if (p.y < 0 || p.y >= HEIGHT) return;
    auto [it, fresh] = waiting_.try_emplace(key(p), due);
    if (fresh) queue_.insert({due, p.x, p.y, p.z});
}

void Redstone::set(glm::ivec3 p, BlockId b, int data, std::vector<ChunkPos>& dirty) {
    for (ChunkPos c : world_.set_block(p.x, p.y, p.z, b, data))
        if (std::find(dirty.begin(), dirty.end(), c) == dirty.end()) dirty.push_back(c);
}

void Redstone::set_data(glm::ivec3 p, int data, std::vector<ChunkPos>& dirty) {
    if (world_.get_data(p.x, p.y, p.z) == data) return;
    for (ChunkPos c : world_.set_data(p.x, p.y, p.z, data))
        if (std::find(dirty.begin(), dirty.end(), c) == dirty.end()) dirty.push_back(c);
}

void Redstone::update_wires(const std::vector<glm::ivec3>& around, std::vector<ChunkPos>& dirty) {
    // The dust near the changes, and all the dust joined to it.
    std::map<Key, int> level;
    std::deque<glm::ivec3> todo;
    auto add = [&](glm::ivec3 p) {
        if (level.size() >= 4096 || p.y < 0 || p.y >= HEIGHT || at(world_, p) != REDSTONE_WIRE) return;
        if (level.emplace(key(p), 0).second) todo.push_back(p);
    };
    for (glm::ivec3 c : around)
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy)
                for (int dz = -2; dz <= 2; ++dz) add(c + glm::ivec3(dx, dy, dz));
    if (todo.empty()) return;
    std::vector<glm::ivec3> cells;
    auto joined = [&](glm::ivec3 p, auto&& fn) {
        const bool open_above = !is_opaque(at(world_, p + UP));
        for (int f = 0; f < 4; ++f) {
            const glm::ivec3 n = p + facing_dir(f);
            fn(n);
            if (!is_opaque(at(world_, n))) fn(n - UP);
            if (open_above) fn(n + UP);
        }
    };
    while (!todo.empty()) {
        const glm::ivec3 p = todo.front();
        todo.pop_front();
        cells.push_back(p);
        joined(p, add);
    }
    // Each piece of dust gets the power that reaches it from outside the dust, then the power runs
    // along the dust losing one a block, strongest first.
    std::array<std::vector<glm::ivec3>, MAX_POWER + 1> by_level;
    for (glm::ivec3 p : cells) {
        const int base = std::min(MAX_POWER, power_into(world_, p, false));
        level[key(p)] = base;
        if (base > 0) by_level[base].push_back(p);
    }
    for (int l = MAX_POWER; l > 1; --l)
        for (size_t i = 0; i < by_level[l].size(); ++i) {
            const glm::ivec3 p = by_level[l][i];
            if (level[key(p)] != l) continue;
            joined(p, [&](glm::ivec3 n) {
                auto it = level.find(key(n));
                if (it == level.end() || it->second >= l - 1) return;
                it->second = l - 1;
                by_level[l - 1].push_back(n);
            });
        }
    for (glm::ivec3 p : cells) set_data(p, level[key(p)], dirty);
}

void Redstone::check(glm::ivec3 p, long now, RedstoneEvents& out, std::vector<ChunkPos>& dirty) {
    if (p.y < 0 || p.y >= HEIGHT || !world_.is_loaded(p.x, p.z)) return;
    const BlockId b = at(world_, p);
    const int data = data_at(world_, p);
    // Edges: did this thing's power just come on (or go)?
    auto edge = [&](glm::ivec3 k, bool powered) {
        const bool was = powered_.count(key(k)) > 0;
        if (powered) powered_.insert(key(k));
        else powered_.erase(key(k));
        return was != powered;
    };
    if ((is_redstone_thin(b) || is_repeater(b)) && !redstone_can_stay(world_, p)) {
        if (auto drop = redstone_drop(b, data)) out.drops.push_back({p, *drop});
        set(p, AIR, 0, dirty);
        return;
    }
    switch (b) {
        case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF: {
            const glm::ivec3 s = p + redstone_support(b, data);
            const bool off = strong_power(world_, s, true) > 0;
            if (off == (b == REDSTONE_TORCH_ON)) schedule(p, now + TORCH_TICKS);
            return;
        }
        case REPEATER_OFF: case REPEATER_ON: {
            const glm::ivec3 back = p - facing_dir(repeater_facing(data));
            const bool in = power_from(world_, back, p, true) > 0 || strong_power(world_, back, true) > 0;
            if (in != (b == REPEATER_ON)) schedule(p, now + repeater_delay(data) * REPEATER_STEP_TICKS);
            return;
        }
        case WOOD_DOOR_BLOCK: case IRON_DOOR_BLOCK: {
            const glm::ivec3 lower = (data & DOOR_UPPER) ? p - UP : p;
            if (!is_door(at(world_, lower))) return;
            const bool on = is_powered(world_, lower) || is_powered(world_, lower + UP);
            if (!edge(lower, on)) return;
            for (glm::ivec3 c : {lower, lower + UP}) {
                if (!is_door(at(world_, c))) continue;
                const int cd = data_at(world_, c);
                set_data(c, on ? (cd | DOOR_OPEN) : (cd & ~DOOR_OPEN), dirty);
            }
            out.doors.push_back({lower, on});
            return;
        }
        case TRAPDOOR: {
            const bool on = is_powered(world_, p);
            if (edge(p, on)) {
                set_data(p, on ? (data | TRAPDOOR_OPEN) : (data & ~TRAPDOOR_OPEN), dirty);
                out.doors.push_back({p, on});
            }
            return;
        }
        case TNT:
            if (is_powered(world_, p)) {
                set(p, AIR, 0, dirty);
                out.tnt.push_back(p);
            }
            return;
        case NOTE_BLOCK: {
            const bool on = is_powered(world_, p);
            if (edge(p, on) && on) out.notes.push_back(p);
            return;
        }
        case DISPENSER_BLOCK: {
            const bool on = is_powered(world_, p);
            if (edge(p, on) && on) schedule(p, now + DISPENSER_TICKS);
            return;
        }
        case POWERED_RAIL: set_data(p, rail_gets_power(world_, p) ? (data | RAIL_POWERED) : (data & ~RAIL_POWERED), dirty); return;
        case PISTON_BASE: case STICKY_PISTON_BASE: {
            const glm::ivec3 front = dir6(data & 7);
            const bool on = power_into(world_, p, true, front) > 0, out_now = data & PISTON_EXTENDED;
            PistonMove move;
            if (on && !out_now && extend_piston(world_, p, move)) out.pistons.push_back(move);
            if (!on && out_now) {
                retract_piston(world_, p, move);
                out.pistons.push_back(move);
            }
            return;
        }
        case PISTON_HEAD: {
            // A head with no pushed-out piston behind it goes.
            const glm::ivec3 base = p - dir6(data & 7);
            if (!is_piston(at(world_, base)) || !(data_at(world_, base) & PISTON_EXTENDED)) set(p, AIR, 0, dirty);
            return;
        }
        default: return;
    }
}

void Redstone::run(glm::ivec3 p, long now, const PlateSensor& sensor, RedstoneEvents& out, std::vector<ChunkPos>& dirty) {
    const BlockId b = at(world_, p);
    const int data = data_at(world_, p);
    switch (b) {
        case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF: {
            const bool off = strong_power(world_, p + redstone_support(b, data), true) > 0;
            const BlockId want = off ? REDSTONE_TORCH_OFF : REDSTONE_TORCH_ON;
            if (want != b) set(p, want, data, dirty);
            return;
        }
        case REPEATER_OFF: case REPEATER_ON: {
            const glm::ivec3 back = p - facing_dir(repeater_facing(data));
            const bool in = power_from(world_, back, p, true) > 0 || strong_power(world_, back, true) > 0;
            const BlockId want = in ? REPEATER_ON : REPEATER_OFF;
            if (want != b) set(p, want, data, dirty);
            return;
        }
        case BUTTON_BLOCK:
            if (data & BUTTON_PRESSED) {
                set_data(p, data & ~BUTTON_PRESSED, dirty);
                out.clicks.push_back(p);
            }
            return;
        case STONE_PLATE_BLOCK: case WOOD_PLATE_BLOCK:
            if (!(data & PLATE_PRESSED)) return;
            if (sensor && sensor(p, b)) {
                schedule(p, now + PLATE_TICKS);
            } else {
                set_data(p, 0, dirty);
                out.clicks.push_back(p);
            }
            return;
        case DISPENSER_BLOCK: out.dispense.push_back(p); return;
        default: return;
    }
}

void Redstone::tick(long now, const PlateSensor& sensor, RedstoneEvents& out, std::vector<ChunkPos>& dirty) {
    std::vector<glm::ivec3> changed;
    changed.swap(changed_);
    if (!changed.empty()) {
        update_wires(changed, dirty);
        std::set<Key> seen;
        for (glm::ivec3 c : changed)
            for (int dx = -2; dx <= 2; ++dx)
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dz = -2; dz <= 2; ++dz) {
                        if (std::abs(dx) + std::abs(dy) + std::abs(dz) > 3) continue;
                        const glm::ivec3 q = c + glm::ivec3(dx, dy, dz);
                        if (seen.insert(key(q)).second) check(q, now, out, dirty);
                    }
    }
    for (int n = 0; n < MAX_REDSTONE_TICKS && !queue_.empty(); ++n) {
        auto [due, x, y, z] = *queue_.begin();
        if (due > now) break;
        queue_.erase(queue_.begin());
        waiting_.erase({x, y, z});
        run({x, y, z}, now, sensor, out, dirty);
    }
}

bool Redstone::use(glm::ivec3 p, long now, std::vector<ChunkPos>& dirty, RedstoneEvents& out) {
    const BlockId b = at(world_, p);
    const int data = data_at(world_, p);
    switch (b) {
        case LEVER_BLOCK:
            set_data(p, data ^ LEVER_ON, dirty);
            out.clicks.push_back(p);
            return true;
        case BUTTON_BLOCK:
            if (!(data & BUTTON_PRESSED)) {
                set_data(p, data | BUTTON_PRESSED, dirty);
                out.clicks.push_back(p);
                schedule(p, now + BUTTON_TICKS);
            }
            return true;
        case REPEATER_OFF: case REPEATER_ON:
            set_data(p, (data & 3) | ((((data >> 2) + 1) & 3) << 2), dirty);
            return true;
        default: return false;
    }
}

void Redstone::press_plate(glm::ivec3 p, long now, std::vector<ChunkPos>& dirty, RedstoneEvents& out) {
    const BlockId b = at(world_, p);
    if (!is_plate(b)) return;
    if (!(data_at(world_, p) & PLATE_PRESSED)) {
        set_data(p, PLATE_PRESSED, dirty);
        out.clicks.push_back(p);
    }
    schedule(p, now + PLATE_TICKS);
}

void Redstone::wake_chunk(const Chunk& c, long now) {
    for (int y = 0; y <= c.max_y && y < HEIGHT; ++y)
        for (int z = 0; z < CHUNK; ++z)
            for (int x = 0; x < CHUNK; ++x) {
                const int i = Chunk::index(x, y, z);
                const BlockId b = c.blocks[i];
                const glm::ivec3 p(c.cx * CHUNK + x, y, c.cz * CHUNK + z);
                if ((b == BUTTON_BLOCK && (c.data[i] & BUTTON_PRESSED)) || (is_plate(b) && (c.data[i] & PLATE_PRESSED)))
                    schedule(p, now + PLATE_TICKS);
            }
}
