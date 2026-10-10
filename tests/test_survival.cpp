// Inventory, health and hunger rules.
#include <doctest/doctest.h>

#include "survival.h"

TEST_CASE("inventory stacks to each item's limit, hotbar first, and reports overflow") {
    Inventory inv;
    CHECK(inv.add(DIRT, 70) == 0);
    CHECK(inv.slots[0] == Stack{DIRT, 64});
    CHECK(inv.slots[1] == Stack{DIRT, 6});
    CHECK(inv.add(SNOWBALL, 20) == 0);  // snowballs stack to 16
    CHECK(inv.slots[2] == Stack{SNOWBALL, 16});
    CHECK(inv.slots[3] == Stack{SNOWBALL, 4});
    CHECK(inv.add(WOOD_PICKAXE, 2) == 0);  // tools don't stack
    CHECK(inv.slots[4] == Stack{WOOD_PICKAXE, 1});
    CHECK(inv.slots[5] == Stack{WOOD_PICKAXE, 1});
    inv.selected = 0;
    CHECK(inv.take_selected() == DIRT);
    CHECK(inv.count(DIRT) == 69);
    CHECK(inv.add(STONE, 64 * 36) > 0);  // overflow is reported
    CHECK(inv.slots[35] == Stack{STONE, 64});
}

TEST_CASE("worn tools only stack with tools worn the same") {
    CHECK(stackable(Stack{DIRT, 3}, Stack{DIRT, 9}));
    CHECK_FALSE(stackable(Stack{DIRT, 3}, Stack{SAND, 3}));
    CHECK_FALSE(stackable(Stack{IRON_SWORD, 1}, Stack{IRON_SWORD, 1}));  // max stack 1
    Inventory inv;
    inv.add(Stack{BOW, 1, 30});
    CHECK(inv.slots[0] == Stack{BOW, 1, 30});
}

TEST_CASE("hunger, regeneration, starving, eating and fall damage") {
    Vitals v;
    v.saturation = 0;
    v.exhaust(8.0);
    CHECK(v.hunger == 18);
    v.health = 10;
    for (int i = 0; i < static_cast<int>(4.0 / 0.05) + 1; ++i) v.tick(0.05);
    CHECK(v.health == 11);  // regenerates when well fed
    v.hunger = 0;
    v.health = 2;
    for (int i = 0; i < 400; ++i) v.tick(0.05);
    CHECK(v.health == 1);  // starving hurts but never kills
    v.hunger = 10;
    CHECK(v.eat(BEEF));
    CHECK(v.hunger == 16);
    CHECK(fall_damage(3.0) == 0);
    CHECK(fall_damage(10.0) == 7);
    CHECK(v.damage(20, true));
    CHECK(v.dead());
}

TEST_CASE("saturation is used up before hunger, and you can't eat when full") {
    Vitals v;
    v.exhaust(4.0);
    CHECK(v.saturation == doctest::Approx(4.0));
    CHECK(v.hunger == MAX_HUNGER);
    CHECK_FALSE(v.eat(APPLE));
    CHECK_FALSE(v.can_sprint() == false);
    v.hunger = 6;
    CHECK_FALSE(v.can_sprint());
}

TEST_CASE("damage cooldown blocks a second hit right away") {
    Vitals v;
    CHECK(v.damage(2));
    CHECK_FALSE(v.damage(2));
    CHECK(v.health == 18);
    v.tick(0.6);
    CHECK(v.damage(2));
}

TEST_CASE("sprinting starts with a quick double tap of forward") {
    SprintControl s;
    s.forward_pressed(10.0, true);
    CHECK_FALSE(s.active);
    s.forward_pressed(10.2, true);
    CHECK(s.active);
    s.update(true, true);
    CHECK(s.active);
    s.update(false, true);  // let go of forward
    CHECK_FALSE(s.active);

    s.forward_pressed(20.0, true);
    s.forward_pressed(20.5, true);  // too slow
    CHECK_FALSE(s.active);

    s.forward_pressed(30.0, false);
    s.forward_pressed(30.1, false);  // hungry or sneaking
    CHECK_FALSE(s.active);

    s.forward_pressed(40.0, true);
    s.forward_pressed(40.1, true);
    s.update(true, false);  // started sneaking
    CHECK_FALSE(s.active);
}
