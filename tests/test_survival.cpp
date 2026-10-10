// Inventory, health and hunger rules.
#include <doctest/doctest.h>

#include "survival.h"

TEST_CASE("inventory stacks to 64, crafts planks and reports overflow") {
    Inventory inv;
    CHECK(inv.add(DIRT, 70) == 0);
    CHECK(inv.slots[0] == Stack{DIRT, 64});
    CHECK(inv.slots[1] == Stack{DIRT, 6});
    inv.add(LOG, 1);
    CHECK(inv.craft_planks());
    CHECK(inv.count(PLANKS) == 4);
    CHECK(inv.count(LOG) == 0);
    inv.selected = 0;
    CHECK(inv.take_selected() == DIRT);
    CHECK(inv.count(DIRT) == 69);
    CHECK(inv.add(STONE, 64 * 9) > 0);  // overflow is reported
    CHECK(inv.count(COBBLE) == 0);
}

TEST_CASE("crafting planks needs a log and room") {
    Inventory inv;
    CHECK_FALSE(inv.craft_planks());
    for (int i = 0; i < Inventory::SIZE; ++i) inv.slots[i] = Stack{i == 0 ? LOG : STONE, i == 0 ? 1 : 64};
    inv.slots[0] = Stack{LOG, 2};
    CHECK_FALSE(inv.craft_planks());  // no free slot for the planks
    CHECK(inv.count(LOG) == 2);
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
