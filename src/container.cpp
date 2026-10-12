#include "container.h"

#include <algorithm>

CraftingMenu::CraftingMenu(Inventory& inventory, int size)
    : inv(inventory), grid_size(size), grid(static_cast<size_t>(size * size)) {}

CraftingMenu::CraftingMenu(Inventory& inventory, Furnace& f) : inv(inventory), grid_size(0), furnace(&f) {}

CraftingMenu::CraftingMenu(Inventory& inventory, ChestSlots& top, ChestSlots* bottom) : inv(inventory), grid_size(0) {
    chest[0] = &top;
    chest[1] = bottom;
}

std::optional<Stack>& CraftingMenu::at(Slot s) {
    switch (s.kind) {
    case Slot::Furnace: return s.index == FURNACE_INPUT ? furnace->input : s.index == FURNACE_FUEL ? furnace->fuel : furnace->output;
    case Slot::Chest: return (*chest[s.index / CHEST_SLOTS])[s.index % CHEST_SLOTS];
    case Slot::Result: return result;
    case Slot::Grid: return grid[s.index];
    case Slot::Armor: return inv.armor[s.index];
    case Slot::Inv: break;
    }
    return inv.slots[s.index];
}

bool CraftingMenu::accepts(Slot s, const Stack& stack) const {
    if (s.kind == Slot::Result) return false;
    if (s.kind == Slot::Furnace) return s.index != FURNACE_OUTPUT;  // Beta's input and fuel slots take anything
    if (s.kind == Slot::Armor) return armor_slot(stack.item) == s.index;
    return true;
}

int CraftingMenu::limit(Slot s, const Stack& stack) const { return s.kind == Slot::Armor ? 1 : max_stack(stack.item); }

void CraftingMenu::update_result() { result = craft_result(grid, grid_size); }

bool CraftingMenu::take_result() {
    if (!result) return false;
    if (cursor && (!stackable(*cursor, *result) || cursor->count + result->count > max_stack(result->item)))
        return false;
    if (cursor)
        cursor->count += result->count;
    else
        cursor = result;
    crafted.push_back(*result);
    consume_ingredients(grid);
    update_result();
    return true;
}

void CraftingMenu::merge(Stack& stack, int from, int to, bool reversed) {
    const int limit = max_stack(stack.item);
    auto each = [&](auto fn) {
        if (reversed)
            for (int i = to - 1; i >= from && stack.count > 0; --i) fn(inv.slots[i]);
        else
            for (int i = from; i < to && stack.count > 0; ++i) fn(inv.slots[i]);
    };
    each([&](std::optional<Stack>& slot) {
        if (slot && stackable(*slot, stack) && slot->count < limit) {
            int moved = std::min(stack.count, limit - slot->count);
            slot->count += moved;
            stack.count -= moved;
        }
    });
    each([&](std::optional<Stack>& slot) {
        if (!slot) {
            slot = stack;
            slot->count = std::min(stack.count, limit);
            stack.count -= slot->count;
        }
    });
}

void CraftingMenu::merge_into_chest(Stack& stack) {
    const int limit = max_stack(stack.item), n = chest_slots();
    for (int pass = 0; pass < 2 && stack.count > 0; ++pass)
        for (int i = 0; i < n && stack.count > 0; ++i) {
            auto& slot = at({Slot::Chest, i});
            if (pass == 0 && slot && stackable(*slot, stack) && slot->count < limit) {
                const int moved = std::min(stack.count, limit - slot->count);
                slot->count += moved;
                stack.count -= moved;
            } else if (pass == 1 && !slot) {
                slot = stack;
                slot->count = std::min(stack.count, limit);
                stack.count -= slot->count;
            }
        }
}

void CraftingMenu::click(Slot s, int button, bool shift) {
    std::optional<Stack>& slot = at(s);
    if (s.kind == Slot::Result) {
        if (shift) {
            // Craft as many as fit, into the hotbar from the right, then the storage rows from the bottom.
            while (result) {
                Stack made = *result;
                Inventory before = inv;
                merge(made, 0, Inventory::HOTBAR, true);
                if (made.count) merge(made, Inventory::HOTBAR, Inventory::SIZE, true);
                if (made.count) {  // no room for the whole result: don't craft it
                    inv = before;
                    break;
                }
                crafted.push_back(*result);
                consume_ingredients(grid);
                update_result();
            }
        } else {
            take_result();
        }
        return;
    }

    if (shift) {
        if (!slot) return;
        Stack moving = *slot;
        if (s.kind == Slot::Inv && chest[0])
            merge_into_chest(moving);  // inventory -> chest
        else if (s.kind == Slot::Chest) {
            merge(moving, 0, Inventory::HOTBAR, true);  // chest -> hotbar from the right, then storage from the bottom
            if (moving.count) merge(moving, Inventory::HOTBAR, Inventory::SIZE, true);
        } else if (s.kind == Slot::Inv && s.index < Inventory::HOTBAR)
            merge(moving, Inventory::HOTBAR, Inventory::SIZE, false);  // hotbar -> storage
        else if (s.kind == Slot::Inv)
            merge(moving, 0, Inventory::HOTBAR, false);  // storage -> hotbar
        else if (s.kind == Slot::Furnace) {
            merge(moving, 0, Inventory::HOTBAR, true);  // furnace -> hotbar from the right, then storage from the bottom
            if (moving.count) merge(moving, Inventory::HOTBAR, Inventory::SIZE, true);
        } else {
            merge(moving, Inventory::HOTBAR, Inventory::SIZE, false);  // grid or armor -> storage, then hotbar
            if (moving.count) merge(moving, 0, Inventory::HOTBAR, false);
        }
        if (s == Slot{Slot::Furnace, FURNACE_OUTPUT} && moving.count < slot->count)
            smelted.push_back(Stack{slot->item, slot->count - moving.count});
        if (moving.count)
            slot->count = moving.count;
        else
            slot.reset();
        if (s.kind == Slot::Grid) update_result();
        return;
    }

    if (!cursor && slot) {
        // Pick up: everything with the left button, the bigger half with the right.
        int take = button == 0 ? slot->count : (slot->count + 1) / 2;
        cursor = *slot;
        cursor->count = take;
        slot->count -= take;
        if (s == Slot{Slot::Furnace, FURNACE_OUTPUT}) smelted.push_back(*cursor);
        if (slot->count == 0) slot.reset();
    } else if (cursor && accepts(s, *cursor)) {
        int lim = limit(s, *cursor);
        if (!slot) {
            int put = button == 0 ? std::min(cursor->count, lim) : 1;
            slot = *cursor;
            slot->count = put;
            cursor->count -= put;
        } else if (stackable(*slot, *cursor)) {
            int put = std::min(button == 0 ? cursor->count : 1, lim - slot->count);
            if (put > 0) {
                slot->count += put;
                cursor->count -= put;
            }
        } else if (cursor->count <= lim) {
            std::swap(*slot, *cursor);
        }
    } else if (cursor && slot && s.kind == Slot::Furnace && stackable(*slot, *cursor) &&
               cursor->count + slot->count <= max_stack(cursor->item)) {
        // The output slot takes nothing, but its items join a matching stack on the mouse.
        cursor->count += slot->count;
        smelted.push_back(*slot);
        slot.reset();
    }
    if (cursor && cursor->count == 0) cursor.reset();
    if (s.kind == Slot::Grid) update_result();
}

std::optional<Stack> CraftingMenu::click_outside(int button) {
    if (!cursor) return std::nullopt;
    Stack thrown = *cursor;
    if (button == 0 || cursor->count == 1) {
        cursor.reset();
    } else {
        thrown.count = 1;
        cursor->count -= 1;
    }
    return thrown;
}

std::optional<Stack> CraftingMenu::drop_one(Slot s) {
    if (s.kind == Slot::Result) return std::nullopt;
    std::optional<Stack>& slot = at(s);
    if (!slot) return std::nullopt;
    Stack thrown = *slot;
    thrown.count = 1;
    if (--slot->count == 0) slot.reset();
    if (s.kind == Slot::Grid) update_result();
    return thrown;
}

std::vector<Stack> CraftingMenu::close() {
    std::vector<Stack> left;
    auto put_back = [&](std::optional<Stack>& s) {
        if (!s) return;
        int over = inv.add(*s);
        if (over) {
            Stack rest = *s;
            rest.count = over;
            left.push_back(rest);
        }
        s.reset();
    };
    put_back(cursor);
    for (auto& cell : grid) put_back(cell);
    result.reset();
    return left;
}

// --- Creative picker ----------------------------------------------------------------------

void CreativeMenu::click_item(int index, int button, bool shift) {
    const auto& items = all_items();
    if (cursor) {
        cursor.reset();
        return;
    }
    if (index < 0 || index >= static_cast<int>(items.size())) return;
    ItemId item = items[index];
    Stack stack{item, button == 0 ? max_stack(item) : 1};
    if (shift) {
        for (int i = 0; i < Inventory::HOTBAR; ++i)
            if (!inv.slots[i]) {
                inv.slots[i] = stack;
                return;
            }
        return;
    }
    cursor = stack;
}

void CreativeMenu::click_hotbar(int index, int button, bool shift) {
    auto& slot = inv.slots[index];
    if (shift) {
        slot.reset();
        return;
    }
    if (!cursor) {
        if (slot) {
            int take = button == 0 ? slot->count : (slot->count + 1) / 2;
            cursor = *slot;
            cursor->count = take;
            slot->count -= take;
            if (slot->count == 0) slot.reset();
        }
        return;
    }
    if (slot && stackable(*slot, *cursor)) {
        int put = std::min(button == 0 ? cursor->count : 1, max_stack(slot->item) - slot->count);
        slot->count += put;
        cursor->count -= put;
        if (cursor->count == 0) cursor.reset();
    } else if (!slot && button == 1) {
        slot = *cursor;
        slot->count = 1;
        if (--cursor->count == 0) cursor.reset();
    } else {
        std::swap(slot, cursor);
    }
}
