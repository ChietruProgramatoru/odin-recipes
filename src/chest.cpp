#include "chest.h"

#include "building.h"
#include "world.h"

std::optional<glm::ivec3> chest_partner(World& world, glm::ivec3 pos) {
    for (int f = 0; f < 4; ++f) {
        const glm::ivec3 n = pos + facing_dir(f);
        if (world.get_block(n.x, n.y, n.z) == CHEST) return n;
    }
    return std::nullopt;
}

ChestPair chest_pair(World& world, glm::ivec3 pos) {
    auto other = chest_partner(world, pos);
    if (!other) return {pos, std::nullopt};
    if (other->x < pos.x || other->z < pos.z) return {*other, pos};
    return {pos, other};
}

ChestSlots* chest_slots(World& world, glm::ivec3 pos) {
    Chunk* c = world.chunk_at(pos);
    if (!c) return nullptr;
    c->dirty = true;
    return &c->chests[cell_index(pos)];
}

void dungeon_loot(Rng& rng, ChestSlots& slots) {
    for (int i = 0; i < 8; ++i) {
        std::optional<Stack> s;
        switch (rng.below(11)) {
            case 0: s = Stack{SADDLE, 1}; break;
            case 1: s = Stack{IRON_INGOT, rng.randint(1, 4)}; break;
            case 2: s = Stack{BREAD, 1}; break;
            case 3: s = Stack{WHEAT, rng.randint(1, 4)}; break;
            case 4: s = Stack{GUNPOWDER, rng.randint(1, 4)}; break;
            case 5: s = Stack{STRING, rng.randint(1, 4)}; break;
            case 6: s = Stack{BUCKET, 1}; break;
            case 7: if (rng.below(100) == 0) s = Stack{GOLDEN_APPLE, 1}; break;
            case 8: if (rng.below(2) == 0) s = Stack{REDSTONE, rng.randint(1, 4)}; break;
            case 9: if (rng.below(10) == 0) s = Stack{rng.below(2) ? RECORD_CAT : RECORD_13, 1}; break;
            default: s = Stack{COCOA_BEANS, 1}; break;
        }
        if (s) slots[rng.below(CHEST_SLOTS)] = s;
    }
}
