#include "blockupdate.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "items.h"
#include "nature.h"
#include "torch.h"

namespace {

constexpr glm::ivec3 UP(0, 1, 0);
// Beta's order for the four sideways directions: -x, +x, -z, +z.
constexpr glm::ivec3 SIDES[4] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
constexpr glm::ivec3 NEIGHBOURS[6] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

int tick_rate(BlockId b) {
    if (b == WATER) return WATER_TICKS;
    if (b == LAVA) return LAVA_TICKS;
    if (b == FIRE) return FIRE_TICKS;
    return FALL_TICKS;
}

BlockId block_at(World& w, glm::ivec3 p) { return w.get_block(p.x, p.y, p.z); }

}  // namespace

void BlockEvents::rebuild(const std::vector<ChunkPos>& chunks) {
    for (ChunkPos c : chunks)
        if (std::find(dirty.begin(), dirty.end(), c) == dirty.end()) dirty.push_back(c);
}

// --- Rules ---------------------------------------------------------------------------------------

int fire_spread_chance(BlockId b) {
    if (b == PLANKS || b == LOG || b == BIRCH_LOG || b == SPRUCE_LOG) return 5;
    if (is_leaves(b)) return 30;
    if (b == TNT) return 15;
    return 0;
}

int burn_chance(BlockId b) {
    if (b == PLANKS) return 20;
    if (b == LOG || b == BIRCH_LOG || b == SPRUCE_LOG) return 5;
    if (is_leaves(b)) return 60;
    if (b == TNT) return 100;
    return 0;
}

bool is_flammable(BlockId b) { return fire_spread_chance(b) > 0; }

bool fire_can_stay(World& world, glm::ivec3 p) {
    if (is_opaque(block_at(world, p - UP))) return true;
    for (glm::ivec3 d : NEIGHBOURS)
        if (is_flammable(block_at(world, p + d))) return true;
    return false;
}

bool under_fluid(World& world, glm::dvec3 p, BlockId fluid) {
    const glm::ivec3 c(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z)));
    if (block_at(world, c) != fluid) return false;
    const double surface = c.y + 1 - (fluid_height_percent(world.get_data(c.x, c.y, c.z)) - 1.0 / 9);
    return p.y < surface;
}

bool can_fall_into(BlockId b) { return b == AIR || b == FIRE || is_fluid(b); }

int fluid_level(World& world, glm::ivec3 p, BlockId fluid) {
    if (block_at(world, p) != fluid) return -1;
    return world.get_data(p.x, p.y, p.z);
}

glm::dvec3 flow_vector(World& world, glm::ivec3 p) {
    const BlockId fluid = block_at(world, p);
    if (!is_fluid(fluid)) return glm::dvec3(0);
    // Falling fluid counts as a full source.
    auto effective = [&](glm::ivec3 q) {
        int l = fluid_level(world, q, fluid);
        return l >= 8 ? 0 : l;
    };
    const int l = effective(p);
    glm::dvec3 v(0);
    for (glm::ivec3 d : SIDES) {
        const glm::ivec3 n = p + d;
        int l1 = effective(n);
        if (l1 < 0) {
            if (!is_solid(block_at(world, n))) {
                l1 = effective(n - UP);
                if (l1 >= 0) v += glm::dvec3(d) * static_cast<double>(l1 - (l - 8));
            }
        } else {
            v += glm::dvec3(d) * static_cast<double>(l1 - l);
        }
    }
    if (world.get_data(p.x, p.y, p.z) >= 8) {
        // Falling down the side of something: mostly down.
        bool beside = false;
        for (glm::ivec3 d : SIDES)
            beside |= is_solid(block_at(world, p + d)) || is_solid(block_at(world, p + d + UP));
        if (beside) {
            if (glm::length(v) > 0) v = glm::normalize(v);
            v += glm::dvec3(0, -6, 0);
        }
    }
    return glm::length(v) > 0 ? glm::normalize(v) : v;
}

// --- Scheduling ------------------------------------------------------------------------------------

void BlockUpdater::schedule(glm::ivec3 p, int delay) {
    if (p.y < 0 || p.y >= HEIGHT) return;
    auto [it, fresh] = waiting_.try_emplace(key(p), now_ + delay, block_at(world_, p));
    if (fresh) queue_.insert({now_ + delay, p.x, p.y, p.z});
}

void BlockUpdater::tick(Rng& rng, const RainCheck& rain, BlockEvents& out) {
    ++now_;
    for (glm::ivec3 p : world_.take_changes()) changed(p, out);
    for (int n = 0; n < MAX_TICKS_PER_TICK && !queue_.empty(); ++n) {
        auto [due, x, y, z] = *queue_.begin();
        if (due > now_) break;
        queue_.erase(queue_.begin());
        const glm::ivec3 p(x, y, z);
        const auto it = waiting_.find(key(p));
        const BlockId expected = it->second.second;
        waiting_.erase(it);
        if (block_at(world_, p) == expected) run(p, rng, rain, out);
    }
}

void BlockUpdater::random_ticks(ChunkPos center, int radius, Rng& rng, const RainCheck& rain, BlockEvents& out) {
    for (int cx = center.cx - radius; cx <= center.cx + radius; ++cx)
        for (int cz = center.cz - radius; cz <= center.cz + radius; ++cz) {
            const Chunk* c = world_.find(cx, cz);
            if (!c) continue;
            for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
                const uint64_t r = rng.next();
                const int x = r & 15, z = (r >> 4) & 15, y = static_cast<int>((r >> 8) % HEIGHT);
                const int at = Chunk::index(x, y, z);
                const BlockId b = c->blocks[at];
                if (b == LAVA || b == FIRE || b == ICE || b == SNOW_LAYER || b == CACTUS ||
                    (is_leaves(b) && (c->data[at] & LEAVES_CHECK)))
                    random_tick({cx * CHUNK + x, y, cz * CHUNK + z}, rng, rain, out);
            }
        }
}

void BlockUpdater::snow_ticks(ChunkPos center, int radius, bool snowing, BlockEvents& out) {
    for (int cx = center.cx - radius; cx <= center.cx + radius; ++cx)
        for (int cz = center.cz - radius; cz <= center.cz + radius; ++cz) {
            if (!world_.find(cx, cz) || snow_rng_.below(SNOW_ODDS) != 0) continue;
            const int x = cx * CHUNK + snow_rng_.below(CHUNK), z = cz * CHUNK + snow_rng_.below(CHUNK);
            const auto top = world_.rain_height(x, z);
            const auto biome = world_.biome_at(x, z);
            if (!top || !biome || !is_cold(*biome) || *top <= 0 || *top >= HEIGHT) continue;
            const glm::ivec3 p(x, *top, z), below = p - UP;
            if (world_.block_light(p.x, p.y, p.z) >= FREEZE_LIGHT) continue;
            if (block_at(world_, below) == WATER && world_.get_data(below.x, below.y, below.z) == 0) {
                set(below, ICE, 0, out);
            } else if (snowing && block_at(world_, p) == AIR && snow_can_stay(world_, p)) {
                set(p, SNOW_LAYER, 0, out);
            }
        }
}

void BlockUpdater::wake_chunk(const Chunk& c) {
    for (int i = 0; i < static_cast<int>(c.blocks.size()); ++i) {
        const BlockId b = c.blocks[i];
        if (b != FIRE && !(is_fluid(b) && c.data[i] != 0)) continue;
        schedule({c.cx * CHUNK + (i & 15), i >> 8, c.cz * CHUNK + ((i >> 4) & 15)}, tick_rate(b));
    }
}

void BlockUpdater::set(glm::ivec3 p, BlockId b, int data, BlockEvents& out) {
    out.rebuild(world_.set_block(p.x, p.y, p.z, b, data));
}

bool BlockUpdater::area_loaded(glm::ivec3 p) {
    for (int dx : {-1, 1})
        for (int dz : {-1, 1})
            if (!world_.is_loaded(p.x + dx, p.z + dz)) return false;
    return true;
}

// A cell was set: the new block settles in (Beta's onBlockAdded), and its neighbours react.
void BlockUpdater::changed(glm::ivec3 p, BlockEvents& out) {
    const BlockId b = block_at(world_, p);
    if (is_fluid(b)) {
        if (!harden(p, out)) schedule(p, tick_rate(b));
    } else if (falls(b)) {
        schedule(p, FALL_TICKS);
    } else if (b == FIRE) {
        if (!fire_can_stay(world_, p)) set(p, AIR, 0, out);
        else schedule(p, FIRE_TICKS);
    } else if (b == CACTUS && !cactus_can_stay(world_, p)) {
        set(p, AIR, 0, out);
        out.drops.push_back({p, CACTUS});
    }
    // A block went (a log chopped, burnt or blown up): the leaves around look for another log.
    if (block_at(world_, p) == AIR || block_at(world_, p) == FIRE) mark_leaves(world_, p);
    for (glm::ivec3 d : NEIGHBOURS) neighbour_changed(p + d, out);
}

void BlockUpdater::neighbour_changed(glm::ivec3 p, BlockEvents& out) {
    if (p.y < 0 || p.y >= HEIGHT || !world_.is_loaded(p.x, p.z)) return;
    const BlockId b = block_at(world_, p);
    if (is_fluid(b)) {
        if (!harden(p, out)) schedule(p, tick_rate(b));
    } else if (falls(b)) {
        schedule(p, FALL_TICKS);
    } else if (b == FIRE) {
        if (!fire_can_stay(world_, p)) set(p, AIR, 0, out);
    } else if (b == CACTUS) {
        // Cactus breaks off when something solid goes next to it or the sand under it goes (Beta).
        if (!cactus_can_stay(world_, p)) {
            set(p, AIR, 0, out);
            out.drops.push_back({p, CACTUS});
        }
    } else if (b == SNOW_LAYER) {
        if (!snow_can_stay(world_, p)) set(p, AIR, 0, out);
    } else if (is_torch(b)) {
        // A torch whose block went (sand falling away under it, say) drops off.
        const glm::ivec3 s = p + torch_support(b);
        if (!is_opaque(block_at(world_, s))) {
            set(p, AIR, 0, out);
            out.drops.push_back({p, TORCH});
        }
    }
}

void BlockUpdater::run(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out) {
    const BlockId b = block_at(world_, p);
    if (!area_loaded(p)) {
        schedule(p, tick_rate(b));  // wait for the chunks around it
        return;
    }
    if (is_fluid(b)) tick_fluid(p, b, rng, out);
    else if (b == FIRE) tick_fire(p, rng, rain, out);
    else if (falls(b)) tick_falling(p, out);
}

void BlockUpdater::random_tick(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out) {
    if (!area_loaded(p)) return;
    const BlockId here = block_at(world_, p);
    if (here == FIRE) {
        tick_fire(p, rng, rain, out);
        return;
    }
    if (here == ICE || here == SNOW_LAYER) {
        // Ice melts back into water, snow just goes (Beta).
        if (melts(world_, p)) set(p, here == ICE ? WATER : AIR, 0, out);
        return;
    }
    if (is_leaves(here)) {
        decay(p, rng, out);
        return;
    }
    if (here == CACTUS) {
        grow_cactus(p, out);
        return;
    }
    // Lava sets fire to air next to something that burns, up to two blocks above it (Beta's
    // BlockStationary).
    const int tries = rng.below(3);
    for (int i = 0; i < tries; ++i) {
        p += glm::ivec3(rng.below(3) - 1, 1, rng.below(3) - 1);
        if (p.y >= HEIGHT || !world_.is_loaded(p.x, p.z)) return;
        const BlockId b = block_at(world_, p);
        if (b == AIR) {
            for (glm::ivec3 d : NEIGHBOURS)
                if (is_flammable(block_at(world_, p + d))) {
                    set(p, FIRE, 0, out);
                    return;
                }
        } else if (is_solid(b)) {
            return;
        }
    }
}

// --- Leaves and cactus (nature.h) ------------------------------------------------------------------

void BlockUpdater::decay(glm::ivec3 p, Rng& rng, BlockEvents& out) {
    const int data = world_.get_data(p.x, p.y, p.z);
    if (!(data & LEAVES_CHECK)) return;
    if ((data & LEAVES_PLAYER) || log_near(world_, p)) {
        world_.set_data(p.x, p.y, p.z, data & ~LEAVES_CHECK, false);
        return;
    }
    const BlockId leaves = block_at(world_, p);
    set(p, AIR, 0, out);
    for (ItemId item : leaf_drops(leaves, rng)) out.drops.push_back({p, item});
}

// Beta's BlockCactus.updateTick: the top of a stalk shorter than 3 ages, and grows one more at 15.
void BlockUpdater::grow_cactus(glm::ivec3 p, BlockEvents& out) {
    if (p.y + 1 >= HEIGHT || block_at(world_, p + UP) != AIR) return;
    int height = 1;
    while (block_at(world_, p - glm::ivec3(0, height, 0)) == CACTUS) ++height;
    if (height >= CACTUS_MAX) return;
    const int age = world_.get_data(p.x, p.y, p.z);
    if (age >= CACTUS_AGE) {
        world_.set_data(p.x, p.y, p.z, 0, false);
        set(p + UP, CACTUS, 0, out);
    } else {
        world_.set_data(p.x, p.y, p.z, age + 1, false);
    }
}

// --- Falling sand and gravel ---------------------------------------------------------------------

void BlockUpdater::tick_falling(glm::ivec3 p, BlockEvents& out) {
    if (p.y <= 0 || !can_fall_into(block_at(world_, p - UP))) return;
    const BlockId b = block_at(world_, p);
    set(p, AIR, 0, out);
    out.falling.push_back({p, b});
}

FallingBlock::FallingBlock(World& world, BlockId b, glm::ivec3 from)
    : block(b), body(world, from.x + 0.5, from.y, from.z + 0.5, 0.49, 0.98) {}

bool FallingBlock::tick(double dt) {
    age += dt;
    Body& b = body;
    b.check_water();
    b.vy -= 16.0 * dt;  // Beta: 0.04 per tick per tick, and 2% drag
    const double drag = std::pow(0.98, dt * 20);
    b.vx *= drag;
    b.vy *= drag;
    b.vz *= drag;
    b.vx += b.current.x * CURRENT_PUSH * dt;
    b.vz += b.current.z * CURRENT_PUSH * dt;
    b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    return b.on_ground || age > 5.0 || b.y < -16;
}

glm::ivec3 FallingBlock::cell() const {
    return {static_cast<int>(std::floor(body.x)), static_cast<int>(std::floor(body.y + 0.01)),
            static_cast<int>(std::floor(body.z))};
}

bool FallingBlock::can_land(World& world) const {
    const glm::ivec3 c = cell();
    if (c.y < 0 || c.y >= HEIGHT) return false;
    return can_fall_into(world.get_block(c.x, c.y, c.z)) && !can_fall_into(world.get_block(c.x, c.y - 1, c.z));
}

// --- Fluids (Beta's BlockFlowing) -------------------------------------------------------------------

bool BlockUpdater::harden(glm::ivec3 p, BlockEvents& out) {
    if (block_at(world_, p) != LAVA) return false;
    bool water = false;
    for (glm::ivec3 d : {glm::ivec3(0, 0, -1), glm::ivec3(0, 0, 1), glm::ivec3(-1, 0, 0), glm::ivec3(1, 0, 0), UP})
        water |= block_at(world_, p + d) == WATER;
    if (!water) return false;
    const int level = world_.get_data(p.x, p.y, p.z);
    if (level == 0) set(p, OBSIDIAN, 0, out);
    else if (level <= 4) set(p, COBBLE, 0, out);
    else return false;
    out.fizz.push_back(p);
    return true;
}

bool BlockUpdater::blocks_flow(glm::ivec3 p) { return is_solid(block_at(world_, p)); }

bool BlockUpdater::can_displace(glm::ivec3 p, BlockId fluid) {
    if (p.y < 0 || p.y >= HEIGHT || !world_.is_loaded(p.x, p.z)) return false;
    const BlockId b = block_at(world_, p);
    if (b == fluid || b == LAVA) return false;
    return !blocks_flow(p);
}

void BlockUpdater::flow_into(glm::ivec3 p, BlockId fluid, int level, BlockEvents& out) {
    if (!can_displace(p, fluid)) return;
    const BlockId b = block_at(world_, p);
    if (b != AIR) {
        if (fluid == LAVA) {
            out.fizz.push_back(p);
        } else if (const BlockInfo* info = block_info(b); info && info->drop && !is_fluid(b)) {
            out.drops.push_back({p, *info->drop});  // water washes torches away
        }
    }
    set(p, fluid, level, out);
}

int BlockUpdater::flow_cost(glm::ivec3 p, BlockId fluid, int depth, int from) {
    int best = 1000;
    for (int k = 0; k < 4; ++k) {
        if ((k ^ 1) == from) continue;  // not back the way it came
        const glm::ivec3 n = p + SIDES[k];
        if (blocks_flow(n) || fluid_level(world_, n, fluid) == 0) continue;
        if (!blocks_flow(n - UP)) return depth;
        if (depth >= 4) continue;
        best = std::min(best, flow_cost(n, fluid, depth + 1, k));
    }
    return best;
}

// The sideways directions with the shortest way to somewhere the fluid can fall down.
std::array<bool, 4> BlockUpdater::flow_directions(glm::ivec3 p, BlockId fluid) {
    int cost[4];
    for (int k = 0; k < 4; ++k) {
        cost[k] = 1000;
        const glm::ivec3 n = p + SIDES[k];
        if (blocks_flow(n) || fluid_level(world_, n, fluid) == 0) continue;
        cost[k] = blocks_flow(n - UP) ? flow_cost(n, fluid, 1, k) : 0;
    }
    const int best = *std::min_element(cost, cost + 4);
    return {cost[0] == best, cost[1] == best, cost[2] == best, cost[3] == best};
}

void BlockUpdater::tick_fluid(glm::ivec3 p, BlockId fluid, Rng& rng, BlockEvents& out) {
    const int step = fluid == LAVA ? 2 : 1;  // lava runs out after 3 blocks, water after 7
    const int rate = tick_rate(fluid);
    int level = fluid_level(world_, p, fluid);
    if (level > 0) {
        // Fed by the lowest level beside it, or by fluid falling in from above.
        int smallest = -100, sources = 0;
        for (glm::ivec3 d : SIDES) {
            int j = fluid_level(world_, p + d, fluid);
            if (j < 0) continue;
            if (j == 0) ++sources;
            if (j >= 8) j = 0;
            smallest = (smallest >= 0 && j >= smallest) ? smallest : j;
        }
        int next = smallest + step;
        if (next >= 8 || smallest < 0) next = -1;
        if (const int above = fluid_level(world_, p + UP, fluid); above >= 0) next = above >= 8 ? above : above + 8;
        // Two water sources beside it make a new source (on solid ground or another source).
        if (sources >= 2 && fluid == WATER) {
            const glm::ivec3 below = p - UP;
            if (is_solid(block_at(world_, below)) || fluid_level(world_, below, fluid) == 0) next = 0;
        }
        bool settled = true;
        if (fluid == LAVA && level < 8 && next < 8 && next > level && rng.below(4) != 0) {
            next = level;  // lava dries up slowly
            settled = false;
        }
        if (next != level) {
            level = next;
            if (level < 0) {
                set(p, AIR, 0, out);
                return;
            }
            out.rebuild(world_.set_data(p.x, p.y, p.z, level));
            schedule(p, rate);
        } else if (!settled) {
            schedule(p, rate);
        }
    }
    const glm::ivec3 below = p - UP;
    if (can_displace(below, fluid)) {
        if (fluid == LAVA && block_at(world_, below) == WATER) {
            set(below, STONE, 0, out);  // lava pouring onto water
            out.fizz.push_back(below);
            return;
        }
        flow_into(below, fluid, level >= 8 ? level : level + 8, out);
    } else if (level >= 0 && (level == 0 || blocks_flow(below))) {
        const int spread = level >= 8 ? 1 : level + step;
        if (spread >= 8) return;
        const std::array<bool, 4> dirs = flow_directions(p, fluid);
        for (int k = 0; k < 4; ++k)
            if (dirs[k]) flow_into(p + SIDES[k], fluid, spread, out);
    }
}

// --- Fire (Beta's BlockFire) -------------------------------------------------------------------------

void BlockUpdater::catch_fire(glm::ivec3 p, int chance, int age, Rng& rng, const RainCheck& rain, BlockEvents& out) {
    const BlockId b = block_at(world_, p);
    if (rng.below(chance) >= burn_chance(b)) return;
    if (rng.below(age + 10) < 5 && !rain(p)) set(p, FIRE, std::min(15, age + rng.below(5) / 4), out);
    else set(p, AIR, 0, out);
    if (b == TNT) out.tnt.push_back(p);
}

void BlockUpdater::tick_fire(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out) {
    if (!fire_can_stay(world_, p)) {
        set(p, AIR, 0, out);
        return;
    }
    auto rained_on = [&](glm::ivec3 q) {
        return rain(q) || rain(q + glm::ivec3(1, 0, 0)) || rain(q - glm::ivec3(1, 0, 0)) || rain(q + glm::ivec3(0, 0, 1)) ||
               rain(q - glm::ivec3(0, 0, 1));
    };
    if (rained_on(p)) {
        set(p, AIR, 0, out);
        return;
    }
    // The fire's age goes up now and then; old fires burn out.
    int age = world_.get_data(p.x, p.y, p.z);
    if (age < 15) {
        age += rng.below(3) / 2;
        world_.set_data(p.x, p.y, p.z, age, false);
    }
    schedule(p, FIRE_TICKS);
    bool fuel = false;
    for (glm::ivec3 d : NEIGHBOURS) fuel |= is_flammable(block_at(world_, p + d));
    if (!fuel) {
        if (!is_opaque(block_at(world_, p - UP)) || age > 3) set(p, AIR, 0, out);
        return;
    }
    if (!is_flammable(block_at(world_, p - UP)) && age == 15 && rng.below(4) == 0) {
        set(p, AIR, 0, out);
        return;
    }
    // The blocks next to it may catch fire or burn away...
    catch_fire(p + glm::ivec3(1, 0, 0), 300, age, rng, rain, out);
    catch_fire(p - glm::ivec3(1, 0, 0), 300, age, rng, rain, out);
    catch_fire(p - UP, 250, age, rng, rain, out);
    catch_fire(p + UP, 250, age, rng, rain, out);
    catch_fire(p - glm::ivec3(0, 0, 1), 300, age, rng, rain, out);
    catch_fire(p + glm::ivec3(0, 0, 1), 300, age, rng, rain, out);
    // ...and fire jumps to the air around it next to things that burn, more easily upwards.
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 4; ++dy) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                const glm::ivec3 q = p + glm::ivec3(dx, dy, dz);
                if (q.y < 0 || q.y >= HEIGHT || block_at(world_, q) != AIR || !world_.is_loaded(q.x, q.z)) continue;
                const int odds = 100 + (dy > 1 ? (dy - 1) * 100 : 0);
                int encouragement = 0;
                for (glm::ivec3 d : NEIGHBOURS) encouragement = std::max(encouragement, fire_spread_chance(block_at(world_, q + d)));
                if (encouragement <= 0) continue;
                const int chance = (encouragement + 40) / (age + 30);
                if (chance > 0 && rng.below(odds) <= chance && !rained_on(q))
                    set(q, FIRE, std::min(15, age + rng.below(5) / 4), out);
            }
}
