// What the mouse does in the inventory and crafting table screens, without any drawing:
// picking up, placing, splitting and shift-clicking stacks, and taking crafted items.
#pragma once
#include <optional>
#include <vector>

#include "crafting.h"
#include "survival.h"

struct Slot {
    enum Kind { Result, Grid, Armor, Inv } kind;
    int index;  // cell of the grid, armor piece (0 helmet..3 boots) or inventory slot (0-8 hotbar, 9-35 storage)
    bool operator==(const Slot&) const = default;
};

class CraftingMenu {
public:
    // grid_size 2 is the inventory screen (with armor slots), 3 is a crafting table.
    CraftingMenu(Inventory& inventory, int grid_size);

    Inventory& inv;
    const int grid_size;
    Grid grid;
    std::optional<Stack> result;  // what the grid makes right now
    std::optional<Stack> cursor;  // the stack held by the mouse

    std::optional<Stack>& at(Slot s);
    // Mouse button 0 (left) or 1 (right) on a slot, with or without Shift.
    void click(Slot s, int button, bool shift);
    // A click outside the window throws the held stack (left) or one item (right). Returns what to throw.
    std::optional<Stack> click_outside(int button);
    // Q over a slot throws one item from it.
    std::optional<Stack> drop_one(Slot s);
    // The screen closes: the grid and the held stack go back into the inventory.
    // Returns whatever didn't fit, to be dropped on the ground.
    std::vector<Stack> close();

private:
    void update_result();
    bool take_result();  // one craft onto the cursor; false if it doesn't fit
    // Move a stack into inventory slots [from, to) (reversed: from the end), filling matching stacks first.
    void merge(Stack& stack, int from, int to, bool reversed);
    bool accepts(Slot s, const Stack& stack) const;
    int limit(Slot s, const Stack& stack) const;
};

// The creative item picker: every item, plus the creative hotbar below it.
class CreativeMenu {
public:
    explicit CreativeMenu(Inventory& hotbar) : inv(hotbar) {}
    Inventory& inv;
    std::optional<Stack> cursor;

    // Click item number `index` of all_items(): a full stack on the cursor, or Shift puts one in the hotbar.
    // Clicking the picker while holding something deletes it.
    void click_item(int index, int button, bool shift);
    // Click hotbar slot 0-8. Shift clears it.
    void click_hotbar(int slot, int button, bool shift);
    // Closing deletes the held stack, since creative items are free.
    void close() { cursor.reset(); }
};
