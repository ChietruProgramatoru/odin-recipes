// Achievements and statistics (M17): the achievement tree, what earns each one, the shown values
// and stats.json, and the crafting screen reporting what was crafted and smelted.
#include <doctest/doctest.h>

#include <set>

#include "achievements.h"
#include "container.h"
#include "items.h"

TEST_CASE("Beta's 16 achievements form one tree rooted at Taking Inventory, inside the map") {
    std::set<std::string> keys;
    for (int a = 0; a < ACHIEVEMENT_COUNT; ++a) {
        const AchievementInfo& info = achievement_info(a);
        keys.insert(info.key);
        CHECK(valid_item(info.icon));
        CHECK(info.col >= ACH_MAP_MIN_COL);
        CHECK(info.col <= ACH_MAP_MAX_COL);
        CHECK(info.row >= ACH_MAP_MIN_ROW);
        CHECK(info.row <= ACH_MAP_MAX_ROW);
        // Each one's parent comes before it, so walking up always reaches the root.
        if (a == ACH_OPEN_INVENTORY)
            CHECK(info.parent == -1);
        else
            CHECK((info.parent >= 0 && info.parent < a));
    }
    CHECK(keys.size() == ACHIEVEMENT_COUNT);
    CHECK(std::string(achievement_info(ACH_FLY_PIG).name) == "When Pigs Fly");
    CHECK(achievement_info(ACH_ON_A_RAIL).special);
}

TEST_CASE("an achievement is earned once, and only after its parent") {
    Stats st;
    CHECK_FALSE(st.can_earn(ACH_MINE_WOOD));
    CHECK_FALSE(st.earn(ACH_MINE_WOOD));  // Beta ignores it until Taking Inventory is earned
    CHECK_FALSE(st.earned[ACH_MINE_WOOD]);
    CHECK(st.earn(ACH_OPEN_INVENTORY));
    CHECK_FALSE(st.earn(ACH_OPEN_INVENTORY));
    CHECK(st.earn(ACH_MINE_WOOD));
    CHECK(st.earned_count() == 2);
}

TEST_CASE("crafting, smelting and picking up earn the matching achievements") {
    CHECK(achievement_for_craft(CRAFTING_TABLE) == ACH_WORKBENCH);
    CHECK(achievement_for_craft(WOOD_PICKAXE) == ACH_PICKAXE);
    CHECK(achievement_for_craft(STONE_PICKAXE) == ACH_BETTER_PICKAXE);
    CHECK(achievement_for_craft(FURNACE) == ACH_FURNACE);
    CHECK(achievement_for_craft(CAKE) == ACH_CAKE);
    CHECK_FALSE(achievement_for_craft(IRON_PICKAXE));
    CHECK(achievement_for_smelt(IRON_INGOT) == ACH_IRON);
    CHECK(achievement_for_smelt(COOKED_FISH) == ACH_COOK_FISH);
    CHECK_FALSE(achievement_for_smelt(GLASS));
    CHECK(achievement_for_pickup(BIRCH_LOG) == ACH_MINE_WOOD);
    CHECK(achievement_for_pickup(LEATHER) == ACH_KILL_COW);
    CHECK_FALSE(achievement_for_pickup(COBBLE));
}

TEST_CASE("statistics show times and distances in Beta's units") {
    CHECK(format_stat(StatUnit::Count, 42) == "42");
    CHECK(format_stat(StatUnit::Time, 200) == "10 s");
    CHECK(format_stat(StatUnit::Time, 20 * 60 * 90) == "1.50 h");
    CHECK(format_stat(StatUnit::Distance, 40) == "40 cm");
    CHECK(format_stat(StatUnit::Distance, 3750) == "37.5 m");
    CHECK(format_stat(StatUnit::Distance, 124000) == "1.24 km");
}

TEST_CASE("stats.json keeps every counter and achievement") {
    Stats st;
    st.add(STAT_WALK, 12345);
    st.add(STAT_DEATHS, 2);
    st.mined[STONE] = 30;
    st.crafted[WOOD_PICKAXE] = 1;
    st.depleted[WOOD_SWORD] = 3;
    st.earn(ACH_OPEN_INVENTORY);
    st.earn(ACH_MINE_WOOD);
    auto back = stats_from_json(stats_to_json(st));
    REQUIRE(back);
    CHECK(*back == st);
    CHECK_FALSE(stats_from_json("not json"));
    // Unknown and broken entries are skipped.
    auto partial = stats_from_json(R"({"general": {"jump": 5, "nope": 1}, "mined": {"x": 3, "3": -1}, "achievements": ["mineWood", "?"]})");
    REQUIRE(partial);
    CHECK(partial->general[STAT_JUMPS] == 5);
    CHECK(partial->mined.empty());
    CHECK(partial->earned[ACH_MINE_WOOD]);
    CHECK(partial->earned_count() == 1);
}

TEST_CASE("the crafting screen reports what was crafted and what came out of the furnace") {
    Inventory inv;
    CraftingMenu menu(inv, 2);
    menu.grid[0] = Stack{PLANKS, 4};
    menu.grid[1] = Stack{PLANKS, 4};
    menu.grid[2] = Stack{PLANKS, 4};
    menu.grid[3] = Stack{PLANKS, 4};
    menu.click({Slot::Grid, 0}, 1, false);  // pick half up and put it back, to work out the result
    menu.click({Slot::Grid, 0}, 0, false);
    REQUIRE(menu.result);
    CHECK(menu.result->item == CRAFTING_TABLE);
    menu.click({Slot::Result, 0}, 0, false);
    REQUIRE(menu.crafted.size() == 1);
    CHECK(menu.crafted[0].item == CRAFTING_TABLE);
    menu.click({Slot::Result, 0}, 0, true);  // shift: as many as fit
    CHECK(menu.crafted.size() == 4);

    Inventory inv2;
    Furnace fur;
    fur.output = Stack{IRON_INGOT, 5};
    CraftingMenu fm(inv2, fur);
    fm.click({Slot::Furnace, FURNACE_OUTPUT}, 1, false);  // right click: half
    REQUIRE(fm.smelted.size() == 1);
    CHECK(fm.smelted[0].count == 3);
    fm.click({Slot::Furnace, FURNACE_OUTPUT}, 0, true);  // shift the rest into the inventory
    REQUIRE(fm.smelted.size() == 2);
    CHECK(fm.smelted[1].count == 2);
    CHECK_FALSE(fur.output);
}
