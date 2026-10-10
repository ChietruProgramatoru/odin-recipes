#include "survival.h"

#include <algorithm>
#include <cmath>

bool stackable(const Stack& a, const Stack& b) {
    return a.item == b.item && a.damage == b.damage && max_stack(a.item) > 1;
}

int Inventory::add(ItemId item, int count) { return add(Stack{item, count}); }

int Inventory::add(Stack stack) {
    const int limit = max_stack(stack.item);
    for (auto& slot : slots)
        if (slot && stackable(*slot, stack) && slot->count < limit) {
            int moved = std::min(stack.count, limit - slot->count);
            slot->count += moved;
            stack.count -= moved;
            if (!stack.count) return 0;
        }
    for (auto& slot : slots)
        if (!slot) {
            int moved = std::min(stack.count, limit);
            slot = Stack{stack.item, moved, stack.damage};
            stack.count -= moved;
            if (!stack.count) return 0;
        }
    return stack.count;
}

int Inventory::count(ItemId item) const {
    int n = 0;
    for (const auto& slot : slots)
        if (slot && slot->item == item) n += slot->count;
    return n;
}

bool Inventory::remove(ItemId item, int count) {
    if (this->count(item) < count) return false;
    for (auto& slot : slots) {
        if (!slot || slot->item != item) continue;
        int taken = std::min(count, slot->count);
        slot->count -= taken;
        count -= taken;
        if (slot->count == 0) slot.reset();
        if (!count) break;
    }
    return true;
}

std::optional<ItemId> Inventory::selected_item() const {
    const auto& slot = slots[selected];
    if (!slot) return std::nullopt;
    return slot->item;
}

std::optional<ItemId> Inventory::take_selected() {
    auto& slot = slots[selected];
    if (!slot) return std::nullopt;
    ItemId item = slot->item;
    if (--slot->count == 0) slot.reset();
    return item;
}

int Inventory::find(ItemId item) const {
    for (int i = 0; i < SIZE; ++i)
        if (slots[i] && slots[i]->item == item) return i;
    return -1;
}

void Vitals::reset() {
    health = MAX_HEALTH;
    hunger = MAX_HUNGER;
    saturation = 5.0;
    exhaustion = 0.0;
    regen_timer = 0.0;
    starve_timer = 0.0;
    peaceful_timer = 0.0;
    hurt_timer = 0.0;
    air = MAX_AIR;
}

int Vitals::breathe(bool head_under_water) {
    if (!head_under_water) {
        air = MAX_AIR;
        return 0;
    }
    if (--air > -20) return 0;
    air = 0;
    return DROWN_DAMAGE;
}

int Vitals::bubbles() const {
    // Beta: ceil((air - 2) * 10 / 300) whole bubbles.
    return std::max(0, static_cast<int>(std::ceil((air - 2) * 10.0 / MAX_AIR)));
}

void Vitals::exhaust(double amount) {
    if (peaceful) return;
    exhaustion += amount;
    while (exhaustion >= 4.0) {
        exhaustion -= 4.0;
        if (saturation > 0)
            saturation = std::max(0.0, saturation - 1);
        else
            hunger = std::max(0, hunger - 1);
    }
}

bool Vitals::damage(int amount, bool ignore_cooldown) {
    if (amount <= 0 || dead() || (hurt_timer > 0 && !ignore_cooldown)) return false;
    health = std::max(0, health - amount);
    hurt_timer = 0.5;
    exhaust(EXHAUST_DAMAGE);
    return true;
}

bool Vitals::eat(ItemId item) {
    const FoodInfo* food = food_info(item);
    return food && eat(*food);
}

bool Vitals::eat(const FoodInfo& food) {
    if (hunger >= MAX_HUNGER) return false;
    hunger = std::min(MAX_HUNGER, hunger + food.hunger);
    saturation = std::min(static_cast<double>(hunger), saturation + food.saturation);
    return true;
}

int Vitals::tick(double dt) {
    hurt_timer = std::max(0.0, hurt_timer - dt);
    int took = 0;
    if (dead()) return 0;
    if (hunger >= 18 && health < MAX_HEALTH) {
        regen_timer += dt;
        if (regen_timer >= REGEN_INTERVAL) {
            regen_timer = 0.0;
            health += 1;
            exhaust(EXHAUST_REGEN);
        }
    } else {
        regen_timer = 0.0;
    }
    if (peaceful && health < MAX_HEALTH) {
        peaceful_timer += dt;
        if (peaceful_timer >= PEACEFUL_REGEN_INTERVAL) {
            peaceful_timer = 0.0;
            health += 1;
        }
    } else {
        peaceful_timer = 0.0;
    }
    if (hunger == 0 && !peaceful) {
        starve_timer += dt;
        if (starve_timer >= STARVE_INTERVAL) {
            starve_timer = 0.0;
            if (health > 1) {  // like Normal difficulty: starving won't kill you
                health -= 1;
                took = 1;
            }
        }
    } else {
        starve_timer = 0.0;
    }
    return took;
}

void SprintControl::forward_pressed(double now, bool allowed) {
    if (allowed && now - last_press_ <= DOUBLE_TAP) active = true;
    last_press_ = now;
}

void SprintControl::update(bool forward_held, bool allowed) {
    if (!forward_held || !allowed) active = false;
}

int fall_damage(double distance) { return std::max(0, static_cast<int>(distance - 3.0 + 0.5)); }
