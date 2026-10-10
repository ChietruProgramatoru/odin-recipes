// Inventory and health/hunger rules. Pure logic, no graphics.
#pragma once
#include <array>
#include <optional>

#include "items.h"

constexpr int MAX_HEALTH = 20;  // 10 hearts
constexpr int MAX_HUNGER = 20;  // 10 drumsticks

// Exhaustion costs (4.0 exhaustion = 1 hunger point once saturation is gone).
constexpr double EXHAUST_WALK = 0.02;    // per block walked
constexpr double EXHAUST_SPRINT = 0.12;  // per block sprinted
constexpr double EXHAUST_JUMP = 0.06;
constexpr double EXHAUST_SPRINT_JUMP = 0.2;
constexpr double EXHAUST_BREAK = 0.03;
constexpr double EXHAUST_ATTACK = 0.1;
constexpr double EXHAUST_DAMAGE = 0.1;
constexpr double EXHAUST_REGEN = 3.0;

constexpr double REGEN_INTERVAL = 4.0;   // seconds per half-heart healed when well fed
constexpr double STARVE_INTERVAL = 4.0;  // seconds per half-heart lost when starving

struct Stack {
    ItemId item;
    int count;
    int damage = 0;  // uses worn off a tool or armor piece
    bool operator==(const Stack&) const = default;
};

// Can these two stacks go in one slot? Same item and wear, and the item stacks.
bool stackable(const Stack& a, const Stack& b);

class Inventory {
public:
    static constexpr int HOTBAR = 9;  // slots 0-8 are the hotbar, 9-35 the storage rows (like Beta)
    static constexpr int SIZE = 36;
    static constexpr int ARMOR = 4;   // helmet, chestplate, leggings, boots
    std::array<std::optional<Stack>, SIZE> slots{};
    std::array<std::optional<Stack>, ARMOR> armor{};
    int selected = 0;

    // Add items: first onto stacks of the same item, then into empty slots, hotbar first.
    // Returns how many did not fit.
    int add(ItemId item, int count = 1);
    int add(Stack stack);
    int count(ItemId item) const;
    bool remove(ItemId item, int count = 1);
    std::optional<ItemId> selected_item() const;
    std::optional<ItemId> take_selected();
    // Index of the first slot holding `item`, or -1.
    int find(ItemId item) const;
    bool operator==(const Inventory&) const = default;
};

class Vitals {
public:
    Vitals() { reset(); }
    void reset();

    int health, hunger;
    double saturation, exhaustion;
    double regen_timer, starve_timer;
    double hurt_timer;  // invulnerability after being hit

    bool dead() const { return health <= 0; }
    bool can_sprint() const { return hunger > 6; }
    void exhaust(double amount);
    // Returns true if damage was applied.
    bool damage(int amount, bool ignore_cooldown = false);
    bool eat(ItemId item);
    // Advance timers. Returns damage taken this tick (for the screen flash).
    int tick(double dt);
};

// Sprinting starts with a quick double tap of the forward key and lasts while it stays held.
class SprintControl {
public:
    static constexpr double DOUBLE_TAP = 0.35;  // seconds between the two taps
    bool active = false;
    // The forward key went down at time `now` (seconds).
    void forward_pressed(double now, bool allowed);
    // Every frame: sprinting ends when forward is let go or sprinting isn't allowed any more.
    void update(bool forward_held, bool allowed);
    void stop() { active = false; }

private:
    double last_press_ = -1e9;
};

int fall_damage(double distance);
