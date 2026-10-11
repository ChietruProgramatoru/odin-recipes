#include "farming.h"

#include <algorithm>

#include "items.h"
#include "nature.h"

namespace {

glm::ivec3 above(glm::ivec3 p) { return p + glm::ivec3(0, 1, 0); }
glm::ivec3 below(glm::ivec3 p) { return p - glm::ivec3(0, 1, 0); }

BlockId block_at(World& w, glm::ivec3 p) {
    if (p.y < 0 || p.y >= HEIGHT) return AIR;
    return w.get_block(p.x, p.y, p.z);
}

bool is_water(BlockId b) { return b == WATER; }

// Flowers, tall grass and wheat need light (or open sky), like Beta.
bool lit_enough(World& w, glm::ivec3 p) {
    return w.block_light(p.x, p.y, p.z) >= 8 || w.sky_light(p.x, p.y, p.z) >= 8;
}

void pop(World& w, glm::ivec3 p, BlockId b, Rng& rng, FarmEvents& out) {
    for (ItemId item : farm_drops(b, w.get_data(p.x, p.y, p.z), rng)) out.drops.push_back({p, item});
    out.set(w, p, AIR);
}

}  // namespace

void FarmEvents::set(World& world, glm::ivec3 pos, BlockId block, int data) {
    for (ChunkPos c : world.set_block(pos.x, pos.y, pos.z, block, data))
        if (std::find(rebuild.begin(), rebuild.end(), c) == rebuild.end()) rebuild.push_back(c);
}

int plant_light(World& world, glm::ivec3 pos, int sky_darkness) {
    return std::max(world.block_light(pos.x, pos.y, pos.z), world.sky_light(pos.x, pos.y, pos.z) - sky_darkness);
}

bool can_stay(World& w, glm::ivec3 p, BlockId block) {
    if (p.y <= 0 || p.y >= HEIGHT) return false;
    const BlockId under = block_at(w, below(p));
    switch (block) {
        case WHEAT_CROP: return under == FARMLAND && lit_enough(w, p);
        case TALL_GRASS: case FERN: case YELLOW_FLOWER: case RED_ROSE:
            return (under == GRASS || under == DIRT || under == FARMLAND) && lit_enough(w, p);
        case BROWN_MUSHROOM: case RED_MUSHROOM:
            return is_opaque(under) && std::max(w.block_light(p.x, p.y, p.z), w.sky_light(p.x, p.y, p.z)) <= MUSHROOM_MAX_LIGHT;
        case SUGAR_CANE_PLANT: {
            if (under == SUGAR_CANE_PLANT) return true;
            if (under != GRASS && under != DIRT && under != SAND) return false;
            const glm::ivec3 b = below(p);
            for (glm::ivec3 d : {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0), glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)})
                if (is_water(block_at(w, b + d))) return true;
            return false;
        }
        case CAKE_BLOCK: return is_solid(under);
        case SAPLING: case SPRUCE_SAPLING: case BIRCH_SAPLING:
            return (under == GRASS || under == DIRT || under == FARMLAND) && lit_enough(w, p);
        case CACTUS: return cactus_can_stay(w, p);
        default: return true;
    }
}

bool water_near_farmland(World& w, glm::ivec3 p) {
    for (int dy = 0; dy <= 1; ++dy)
        for (int dz = -4; dz <= 4; ++dz)
            for (int dx = -4; dx <= 4; ++dx)
                if (is_water(block_at(w, p + glm::ivec3(dx, dy, dz)))) return true;
    return false;
}

float wheat_growth_rate(World& w, glm::ivec3 p) {
    auto crop = [&](int dx, int dz) { return block_at(w, p + glm::ivec3(dx, 0, dz)) == WHEAT_CROP; };
    const bool along_z = crop(0, -1) || crop(0, 1), along_x = crop(-1, 0) || crop(1, 0);
    const bool diagonal = crop(-1, -1) || crop(1, -1) || crop(1, 1) || crop(-1, 1);
    float rate = 1;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            glm::ivec3 s = below(p) + glm::ivec3(dx, 0, dz);
            float f = 0;
            if (block_at(w, s) == FARMLAND) f = w.get_data(s.x, s.y, s.z) > 0 ? 3.0f : 1.0f;
            if (dx != 0 || dz != 0) f /= 4;
            rate += f;
        }
    if (diagonal || (along_x && along_z)) rate /= 2;
    return rate;
}

void random_tick(World& w, glm::ivec3 p, Rng& rng, const TickContext& ctx, FarmEvents& out) {
    const BlockId b = block_at(w, p);
    if (is_plant(b) && !can_stay(w, p, b)) {
        pop(w, p, b, rng, out);
        return;
    }
    const int data = w.get_data(p.x, p.y, p.z);
    switch (b) {
        case FARMLAND: {
            if (rng.below(5) != 0) return;
            const auto rain_top = w.rain_height(p.x, p.z);
            const bool rained_on = ctx.raining && rain_top && *rain_top <= p.y + 1;
            if (water_near_farmland(w, p) || rained_on) {
                if (data != FARMLAND_WET) out.set(w, p, FARMLAND, FARMLAND_WET);
            } else if (data > 0) {
                out.set(w, p, FARMLAND, data - 1);
            } else if (block_at(w, above(p)) != WHEAT_CROP) {
                out.set(w, p, DIRT);  // dried out with nothing growing on it
            }
            return;
        }
        case WHEAT_CROP: {
            if (data >= WHEAT_RIPE || plant_light(w, above(p), ctx.sky_darkness) < CROP_LIGHT) return;
            const int odds = static_cast<int>(100.0f / wheat_growth_rate(w, p));
            if (rng.below(std::max(1, odds)) == 0) out.set(w, p, WHEAT_CROP, data + 1);
            return;
        }
        case SUGAR_CANE_PLANT: {
            if (block_at(w, above(p)) != AIR || p.y + 1 >= HEIGHT) return;
            int height = 1;
            while (block_at(w, p - glm::ivec3(0, height, 0)) == SUGAR_CANE_PLANT) ++height;
            if (height >= SUGAR_CANE_MAX) return;
            if (data >= SUGAR_CANE_AGE) {
                out.set(w, p, SUGAR_CANE_PLANT, 0);
                out.set(w, above(p), SUGAR_CANE_PLANT, 0);
            } else {
                out.set(w, p, SUGAR_CANE_PLANT, data + 1);
            }
            return;
        }
        case SAPLING: case SPRUCE_SAPLING: case BIRCH_SAPLING: {
            // Beta: with light, 1 in 30 ticks gets it ready, and the next such tick grows the tree.
            if (plant_light(w, above(p), ctx.sky_darkness) < SAPLING_LIGHT || rng.below(SAPLING_ODDS) != 0) return;
            if (!(data & SAPLING_READY)) out.set(w, p, b, data | SAPLING_READY);
            else grow_tree(w, p, rng, out.rebuild);
            return;
        }
        case BROWN_MUSHROOM: case RED_MUSHROOM: {
            // Now and then a mushroom spreads to a dark spot next to it.
            if (rng.below(100) != 0) return;
            glm::ivec3 to = p + glm::ivec3(rng.below(3) - 1, rng.below(2) - rng.below(2), rng.below(3) - 1);
            if (block_at(w, to) == AIR && can_stay(w, to, b)) out.set(w, to, b);
            return;
        }
        default: return;
    }
}

void tick_chunk(World& w, const Chunk& chunk, Rng& rng, const TickContext& ctx, FarmEvents& out) {
    const int top = std::min(HEIGHT, chunk.max_y + 2);
    for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
        const int x = rng.below(CHUNK), z = rng.below(CHUNK), y = rng.below(HEIGHT);
        if (y >= top) continue;  // only air up there
        const BlockId b = chunk.blocks[Chunk::index(x, y, z)];
        if (b != FARMLAND && !is_plant(b)) continue;
        random_tick(w, {chunk.cx * CHUNK + x, y, chunk.cz * CHUNK + z}, rng, ctx, out);
    }
}

std::vector<ItemId> farm_drops(BlockId block, int data, Rng& rng) {
    std::vector<ItemId> items;
    switch (block) {
        case WHEAT_CROP:
            if (data >= WHEAT_RIPE) items.push_back(WHEAT);
            for (int i = 0; i < 3; ++i)
                if (rng.below(15) <= data) items.push_back(SEEDS);
            break;
        case TALL_GRASS: case FERN:
            if (rng.below(8) == 0) items.push_back(SEEDS);
            break;
        case SUGAR_CANE_PLANT: items.push_back(SUGAR_CANE); break;
        case FARMLAND: items.push_back(DIRT); break;
        case YELLOW_FLOWER: case RED_ROSE: case BROWN_MUSHROOM: case RED_MUSHROOM:
        case SAPLING: case SPRUCE_SAPLING: case BIRCH_SAPLING:
            items.push_back(block);
            break;
        default: break;  // cake and cobwebs drop nothing when knocked off, like Beta
    }
    return items;
}

void break_unsupported(World& w, glm::ivec3 pos, Rng& rng, FarmEvents& out) {
    for (glm::ivec3 p = above(pos); p.y < HEIGHT; p = above(p)) {
        const BlockId b = block_at(w, p);
        if ((!is_plant(b) && b != CAKE_BLOCK) || can_stay(w, p, b)) return;
        pop(w, p, b, rng, out);
        if (b != SUGAR_CANE_PLANT) return;  // a cane stalk breaks all the way up
    }
}

bool till(World& w, glm::ivec3 p, FarmEvents& out) {
    const BlockId b = block_at(w, p);
    if ((b != DIRT && b != GRASS) || block_at(w, above(p)) != AIR) return false;
    out.set(w, p, FARMLAND, 0);
    return true;
}

bool trample(World& w, glm::ivec3 p, double fall, Rng& rng, FarmEvents& out) {
    if (block_at(w, p) != FARMLAND || rng.random() >= fall - 0.5) return false;
    out.set(w, p, DIRT);
    break_unsupported(w, p, rng, out);
    return true;
}

bool bone_meal(World& w, glm::ivec3 p, Rng& rng, FarmEvents& out) {
    const BlockId b = block_at(w, p);
    if (b == WHEAT_CROP) {
        if (w.get_data(p.x, p.y, p.z) >= WHEAT_RIPE) return false;
        out.set(w, p, WHEAT_CROP, WHEAT_RIPE);
        return true;
    }
    if (is_sapling(b)) {
        grow_tree(w, p, rng, out.rebuild);  // used up even when there is no room for the tree (Beta)
        return true;
    }
    if (b != GRASS) return false;
    // Beta: random walks out from the block, each step staying on grass; tall grass mostly, now and then a flower.
    for (int i = 0; i < BONE_MEAL_TRIES; ++i) {
        glm::ivec3 at = above(p);
        bool ok = true;
        for (int step = 0; step < i / 16 && ok; ++step) {
            at += glm::ivec3(rng.below(3) - 1, (rng.below(3) - 1) * rng.below(3) / 2, rng.below(3) - 1);
            ok = block_at(w, below(at)) == GRASS && !is_opaque(block_at(w, at));
        }
        if (!ok || block_at(w, at) != AIR) continue;
        BlockId plant = rng.below(10) != 0 ? TALL_GRASS : rng.below(3) != 0 ? YELLOW_FLOWER : RED_ROSE;
        if (can_stay(w, at, plant)) out.set(w, at, plant);
    }
    return true;
}

BlockId planted_block(ItemId item) {
    switch (item) {
        case SEEDS: return WHEAT_CROP;
        case SUGAR_CANE: return SUGAR_CANE_PLANT;
        case CAKE: return CAKE_BLOCK;
        case YELLOW_FLOWER: case RED_ROSE: case BROWN_MUSHROOM: case RED_MUSHROOM:
        case SAPLING: case SPRUCE_SAPLING: case BIRCH_SAPLING: case CACTUS:
            return static_cast<BlockId>(item);
        default: return AIR;
    }
}
