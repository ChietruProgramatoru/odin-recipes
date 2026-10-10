// Crafting recipes (Beta 1.7.3's, as a table in code) and matching a crafting grid against them.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "survival.h"

// One cell of a recipe: any of these items.
using Ingredient = std::vector<ItemId>;

struct Recipe {
    // Shaped: rows of key letters, ' ' for an empty cell. Matches anywhere in the grid, and mirrored.
    std::vector<std::string> shape;
    std::vector<std::pair<char, Ingredient>> key;
    // Shapeless (when shape is empty): these ingredients in any cells.
    std::vector<Ingredient> ingredients;
    Stack result;
};

const std::vector<Recipe>& recipes();

// A square crafting grid (2x2 in the inventory, 3x3 on a crafting table), row by row from the top left.
using Grid = std::vector<std::optional<Stack>>;

// The recipe the grid makes, or nullptr.
const Recipe* match_recipe(const Grid& grid, int size);
std::optional<Stack> craft_result(const Grid& grid, int size);
// Take one item from every filled cell, after crafting. Buckets of milk leave an empty bucket behind.
void consume_ingredients(Grid& grid);
