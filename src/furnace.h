// Furnaces: smelting recipes, fuel and what one furnace does each game tick (Beta 1.7.3's rules).
// Pure logic; the furnaces themselves are kept with their chunk (see Chunk::furnaces in world.h).
#pragma once
#include <optional>

#include "survival.h"

constexpr int SMELT_TICKS = 200;  // 10 seconds per item

// What an item smelts into, or nothing.
std::optional<Stack> smelt_result(ItemId input);
// How many ticks an item burns for as fuel (0: not a fuel).
int fuel_ticks(ItemId item);

struct Furnace {
    std::optional<Stack> input, fuel, output;
    int burn_left = 0;   // ticks the fire burns on for
    int burn_total = 0;  // ticks the last fuel item burns in all (for the flame on the screen)
    int cook = 0;        // ticks the item in the input slot has been cooking (SMELT_TICKS finishes it)

    bool burning() const { return burn_left > 0; }
    // There is something to smelt and room for what it makes.
    bool can_smelt() const;
    // One game tick. Returns true if anything changed.
    bool tick();
    bool empty() const { return !input && !fuel && !output && burn_left == 0; }
    bool operator==(const Furnace&) const = default;
};
