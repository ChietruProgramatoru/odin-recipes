#include "furnace.h"

std::optional<Stack> smelt_result(ItemId input) {
    switch (input) {
        case IRON_ORE: return Stack{IRON_INGOT, 1};
        case GOLD_ORE: return Stack{GOLD_INGOT, 1};
        case DIAMOND_ORE: return Stack{DIAMOND, 1};
        case SAND: return Stack{GLASS, 1};
        case COBBLE: return Stack{STONE, 1};
        case CLAY_BALL: return Stack{CLAY_BRICK, 1};
        case LOG: case BIRCH_LOG: case SPRUCE_LOG: return Stack{CHARCOAL, 1};
        case RAW_PORKCHOP: return Stack{COOKED_PORKCHOP, 1};
        case RAW_FISH: return Stack{COOKED_FISH, 1};
        case BEEF: return Stack{STEAK, 1};
        default: return std::nullopt;
    }
}

int fuel_ticks(ItemId item) {
    switch (item) {
        case COAL: case CHARCOAL: return 1600;
        // Beta burns every block made of wood for 15 seconds.
        case LOG: case BIRCH_LOG: case SPRUCE_LOG: case PLANKS: case CRAFTING_TABLE: case CHEST:
        case BOOKSHELF: case NOTE_BLOCK: case JUKEBOX: case WOOD_STAIRS: case FENCE: case TRAPDOOR: case WOOD_PLATE:
            return 300;
        case STICK: return 100;
        case LAVA_BUCKET: return 20000;
        default: return 0;
    }
}

bool Furnace::can_smelt() const {
    if (!input) return false;
    auto made = smelt_result(input->item);
    if (!made) return false;
    if (!output) return true;
    return output->item == made->item && output->count + made->count <= max_stack(made->item);
}

bool Furnace::tick() {
    const bool was_burning = burning();
    bool changed = false;
    if (burn_left > 0) {
        --burn_left;
        changed = true;
    }
    if (burn_left == 0 && can_smelt() && fuel) {
        const int ticks = fuel_ticks(fuel->item);
        if (ticks > 0) {
            burn_left = burn_total = ticks;
            // The lava goes and the empty bucket stays behind (Beta lost the bucket too; decided 2026-10-10).
            if (fuel->item == LAVA_BUCKET)
                fuel = Stack{BUCKET, 1};
            else if (--fuel->count == 0)
                fuel.reset();
            changed = true;
        }
    }
    if (burning() && can_smelt()) {
        if (++cook >= SMELT_TICKS) {
            cook = 0;
            Stack made = *smelt_result(input->item);
            if (output)
                output->count += made.count;
            else
                output = made;
            if (--input->count == 0) input.reset();
        }
        changed = true;
    } else if (cook != 0) {
        cook = 0;
        changed = true;
    }
    return changed || was_burning != burning();
}
