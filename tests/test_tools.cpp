// Tools, weapons and armor rules (Beta 1.7.3 values).
#include <doctest/doctest.h>

#include "tools.h"

TEST_CASE("tool kinds and materials come from the item table order") {
    CHECK(tool_kind(AIR) == ToolKind::None);
    CHECK(tool_kind(STICK) == ToolKind::None);
    CHECK(tool_kind(WOOD_SWORD) == ToolKind::Sword);
    CHECK(tool_kind(STONE_PICKAXE) == ToolKind::Pickaxe);
    CHECK(tool_kind(GOLD_AXE) == ToolKind::Axe);
    CHECK(tool_kind(DIAMOND_HOE) == ToolKind::Hoe);
    CHECK(tool_kind(IRON_SHOVEL) == ToolKind::Shovel);
    CHECK(tool_material(DIAMOND_SHOVEL) == Material::Diamond);
    CHECK_FALSE(tool_material(BOW));
    CHECK(tool(Material::Iron, 2) == IRON_PICKAXE);
}

TEST_CASE("Beta durability per material") {
    CHECK(max_damage(WOOD_PICKAXE) == 59);
    CHECK(max_damage(STONE_SWORD) == 131);
    CHECK(max_damage(IRON_AXE) == 250);
    CHECK(max_damage(GOLD_SHOVEL) == 32);
    CHECK(max_damage(DIAMOND_PICKAXE) == 1561);
    CHECK(max_damage(BOW) == 384);
    CHECK(max_damage(SHEARS) == 238);
    CHECK(max_damage(FLINT_AND_STEEL) == 64);
}

TEST_CASE("stone and ores need a pickaxe of the right tier to drop anything") {
    CHECK_FALSE(can_harvest(AIR, STONE));
    CHECK_FALSE(harvest_drop(AIR, STONE));
    CHECK_FALSE(can_harvest(WOOD_SHOVEL, COBBLE));
    CHECK_FALSE(can_harvest(DIAMOND_SWORD, COAL_ORE));
    CHECK(harvest_drop(WOOD_PICKAXE, STONE) == COBBLE);
    CHECK(harvest_drop(GOLD_PICKAXE, COAL_ORE) == COAL);
    CHECK(can_harvest(WOOD_PICKAXE, BRICK));
    CHECK(can_harvest(WOOD_PICKAXE, FURNACE));
    CHECK(can_harvest(WOOD_PICKAXE, SANDSTONE));

    // Iron and lapis: stone or better.
    for (BlockId ore : {IRON_ORE, LAPIS_ORE}) {
        CHECK_FALSE(can_harvest(WOOD_PICKAXE, ore));
        CHECK_FALSE(can_harvest(GOLD_PICKAXE, ore));
        CHECK(can_harvest(STONE_PICKAXE, ore));
        CHECK(can_harvest(IRON_PICKAXE, ore));
        CHECK(can_harvest(DIAMOND_PICKAXE, ore));
    }
    // Gold, diamond and redstone: iron or better.
    for (BlockId ore : {GOLD_ORE, DIAMOND_ORE, REDSTONE_ORE}) {
        CHECK_FALSE(can_harvest(STONE_PICKAXE, ore));
        CHECK(can_harvest(IRON_PICKAXE, ore));
        CHECK(can_harvest(DIAMOND_PICKAXE, ore));
    }
    CHECK(harvest_drop(IRON_PICKAXE, DIAMOND_ORE) == DIAMOND);

    // Everything else drops by hand.
    CHECK(harvest_drop(AIR, DIRT) == DIRT);
    CHECK(harvest_drop(AIR, LOG) == LOG);
    CHECK(harvest_drop(AIR, GLOWSTONE) == GLOWSTONE);
    CHECK_FALSE(harvest_drop(AIR, LEAVES));
}

TEST_CASE("shears cut leaves fast and keep them") {
    CHECK(harvest_drop(SHEARS, LEAVES) == LEAVES);
    CHECK(harvest_drop(SHEARS, BIRCH_LEAVES) == BIRCH_LEAVES);
    CHECK(*break_seconds(SHEARS, LEAVES) < *break_seconds(AIR, LEAVES));
    CHECK(wear_from_breaking(SHEARS, LEAVES) == 1);
    CHECK(wear_from_breaking(SHEARS, DIRT) == 0);
}

TEST_CASE("break times follow Beta's hardness and tool speeds") {
    // By hand: 1.5 s per point of hardness when the block drops, 5 s when it doesn't.
    CHECK(*break_seconds(AIR, DIRT) == doctest::Approx(0.75));
    CHECK(*break_seconds(AIR, LOG) == doctest::Approx(3.0));
    CHECK(*break_seconds(AIR, STONE) == doctest::Approx(7.5));
    // The right tool: divided by the material's efficiency.
    CHECK(*break_seconds(WOOD_PICKAXE, STONE) == doctest::Approx(1.125));
    CHECK(*break_seconds(STONE_PICKAXE, STONE) == doctest::Approx(0.5625));
    CHECK(*break_seconds(DIAMOND_SHOVEL, DIRT) == doctest::Approx(0.75 / 8));
    CHECK(*break_seconds(GOLD_AXE, PLANKS) == doctest::Approx(3.0 / 12));
    // Gold is the fastest, diamond the next.
    CHECK(*break_seconds(GOLD_PICKAXE, COBBLE) < *break_seconds(DIAMOND_PICKAXE, COBBLE));
    CHECK(*break_seconds(DIAMOND_PICKAXE, COBBLE) < *break_seconds(IRON_PICKAXE, COBBLE));
    // The wrong tool is no faster than a hand; a wooden pickaxe can't really mine iron ore.
    CHECK(*break_seconds(DIAMOND_SHOVEL, LOG) == doctest::Approx(*break_seconds(AIR, LOG)));
    CHECK(*break_seconds(WOOD_PICKAXE, IRON_ORE) == doctest::Approx(15.0));
    // Swords cut everything a bit faster.
    CHECK(*break_seconds(WOOD_SWORD, DIRT) == doctest::Approx(0.5));
    // Torches break at once; bedrock, water and lava never.
    CHECK(*break_seconds(AIR, TORCH) == 0);
    CHECK_FALSE(break_seconds(DIAMOND_PICKAXE, BEDROCK));
    CHECK_FALSE(break_seconds(DIAMOND_PICKAXE, WATER));
}

TEST_CASE("sword damage by tier; tools hit a little harder than a fist") {
    CHECK(attack_damage(AIR) == 1);
    CHECK(attack_damage(DIRT) == 1);
    CHECK(attack_damage(WOOD_SWORD) == 4);
    CHECK(attack_damage(GOLD_SWORD) == 4);
    CHECK(attack_damage(STONE_SWORD) == 6);
    CHECK(attack_damage(IRON_SWORD) == 8);
    CHECK(attack_damage(DIAMOND_SWORD) == 10);
    CHECK(attack_damage(WOOD_SHOVEL) == 1);
    CHECK(attack_damage(WOOD_PICKAXE) == 2);
    CHECK(attack_damage(WOOD_AXE) == 3);
    CHECK(attack_damage(DIAMOND_AXE) == 6);
    CHECK(attack_damage(IRON_HOE) == 1);
    CHECK(attack_damage(BOW) == 1);
}

TEST_CASE("tools wear out: swords from hits, tools from digging") {
    CHECK(wear_from_hit(IRON_SWORD) == 1);
    CHECK(wear_from_hit(IRON_PICKAXE) == 2);
    CHECK(wear_from_hit(AIR) == 0);
    CHECK(wear_from_breaking(IRON_SWORD, DIRT) == 2);
    CHECK(wear_from_breaking(IRON_PICKAXE, DIRT) == 1);
    CHECK(wear_from_breaking(IRON_HOE, DIRT) == 0);

    Stack pick{GOLD_PICKAXE, 1};
    int uses = 0;
    while (!wear_out(pick, 1)) ++uses;
    CHECK(uses == 32);  // Beta: breaks on the use after its damage reaches the durability
    Stack dirt{DIRT, 5};
    CHECK_FALSE(wear_out(dirt, 1));
    CHECK(dirt.damage == 0);
}

TEST_CASE("armor: 3/8/6/3 points for any material, scaled down as it wears") {
    Inventory inv;
    CHECK(armor_value(inv) == 0);
    inv.armor[1] = Stack{LEATHER_CHESTPLATE, 1};
    CHECK(armor_points(LEATHER_CHESTPLATE) == 8);
    CHECK(armor_value(inv) == 8);
    inv.armor[0] = Stack{DIAMOND_HELMET, 1};
    inv.armor[2] = Stack{IRON_LEGGINGS, 1};
    inv.armor[3] = Stack{GOLD_BOOTS, 1};
    CHECK(armor_value(inv) == 20);
    CHECK(max_damage(LEATHER_HELMET) == 33);
    CHECK(max_damage(DIAMOND_CHESTPLATE) == 384);
    // Half worn: about half the points.
    for (auto& piece : inv.armor) piece->damage = max_damage(piece->item) / 2;
    CHECK(armor_value(inv) >= 9);
    CHECK(armor_value(inv) <= 11);
}

TEST_CASE("armor takes 4% off per point and wears with every hit") {
    Inventory inv;
    int rest = 0;
    CHECK(absorb_with_armor(inv, 10, rest) == 10);  // no armor
    CHECK(rest == 0);

    for (int p = 0; p < 4; ++p) inv.armor[p] = Stack{armor(Material::Diamond, static_cast<ArmorPiece>(p)), 1};
    rest = 0;
    CHECK(absorb_with_armor(inv, 10, rest) == 2);  // full set: 80% off (10 * 5 / 25)
    CHECK(inv.armor[0]->damage == 10);
    CHECK(inv.armor[3]->damage == 10);

    // A worn-out piece breaks and is removed.
    inv.armor[3]->damage = max_damage(DIAMOND_BOOTS);
    absorb_with_armor(inv, 1, rest);
    CHECK_FALSE(inv.armor[3]);

    // Fractions of a point carry over to the next hit.
    Inventory one;
    one.armor[0] = Stack{IRON_HELMET, 1};  // 3 points: 1 damage -> 22/25
    rest = 0;
    int total = 0;
    for (int i = 0; i < 25; ++i) total += absorb_with_armor(one, 1, rest);
    CHECK(total >= 21);
    CHECK(total <= 23);
}

// --- Arrows and fire --------------------------------------------------------------------------

#include "mobs.h"

namespace {
// Clear a box of sky well above the terrain so nothing else gets in the way.
void clear_sky(World& w) {
    for (int x = -2; x < 12; ++x)
        for (int y = 100; y < 120; ++y)
            for (int z = -2; z < 12; ++z) w.set_block(x, y, z, AIR);
}
}  // namespace

TEST_CASE("an arrow flies, drops a little, and sticks in the first solid block") {
    World w(7);
    clear_sky(w);
    w.set_block(0, 110, 8, STONE);
    w.set_block(0, 110, 4, TORCH);  // arrows fly through torches
    Arrow a({0.5, 110.5, 0.5}, {0, 0, Arrow::SPEED});
    std::vector<ArrowTarget> none;
    for (int i = 0; i < 40 && !a.stuck; ++i) CHECK_FALSE(a.tick(0.02, w, none));
    REQUIRE(a.stuck);
    CHECK(a.stuck_in == glm::ivec3(0, 110, 8));
    CHECK(a.pos.z == doctest::Approx(8.15).epsilon(0.01));
    CHECK(a.pos.y < 110.5);  // gravity pulled it down a bit
    CHECK(a.yaw == doctest::Approx(0).epsilon(0.01));

    // It stays put, then falls once its block is broken.
    a.tick(1.0, w, none);
    CHECK(a.stuck);
    CHECK(a.stuck_time == doctest::Approx(1.0));
    w.set_block(0, 110, 8, AIR);
    a.tick(0.05, w, none);
    CHECK_FALSE(a.stuck);
    CHECK(a.vel.y < 0);

    // A stuck arrow lasts a minute.
    Arrow b({0.5, 110.5, 0.5}, {0, 0, 0});
    b.stuck = true;
    b.stuck_in = {0, 109, 0};
    w.set_block(0, 109, 0, STONE);
    b.tick(Arrow::STUCK_LIFETIME + 1, w, none);
    CHECK(b.expired());
}

TEST_CASE("an arrow hits the first cow in its path, but never the one who shot it") {
    World w(7);
    clear_sky(w);
    w.set_block(5, 109, 5, STONE);
    Rng rng(1);
    Cow near(w, 5.5, 110, 3.5, rng), far(w, 5.5, 110, 6.5, rng);
    std::vector<ArrowTarget> targets{far.target(0, 1), near.target(0, 0)};
    Arrow a({5.5, 110.8, 0.5}, {0, 0, Arrow::SPEED});
    a.shooter = &near;  // flies through its own shooter
    std::optional<ArrowTarget> hit;
    for (int i = 0; i < 40 && !hit && !a.stuck; ++i) hit = a.tick(0.02, w, targets);
    REQUIRE(hit);
    CHECK(hit->index == 1);
    far.hurt(ARROW_DAMAGE, a.pos.x, a.pos.z - 1);
    CHECK(far.health == COW_MAX_HEALTH - ARROW_DAMAGE);
}

TEST_CASE("fire is a thin light source you walk through") {
    CHECK(block_info(FIRE));
    CHECK_FALSE(is_solid(FIRE));
    CHECK_FALSE(is_opaque(FIRE));
    CHECK(light_emission(FIRE) == 15);
    CHECK(*break_seconds(AIR, FIRE) == 0);  // punching it puts it out
    CHECK_FALSE(harvest_drop(AIR, FIRE));
    CHECK_FALSE(valid_item(FIRE));  // not in the inventory or the creative picker
}

TEST_CASE("the player model wears the armor in the armor slots") {
    Inventory inv;
    inv.armor[0] = Stack{GOLD_HELMET, 1};
    inv.armor[2] = Stack{CHAIN_LEGGINGS, 1};
    auto worn = worn_armor(inv);
    CHECK(worn[0] == Material::Gold);
    CHECK_FALSE(worn[1]);
    CHECK(worn[2] == Material::Stone);  // chain uses the Stone slot of the material list
    CHECK_FALSE(worn[3]);
    PlayerPose pose;
    size_t bare = player_parts(pose).size();
    pose.armor = worn;
    CHECK(player_parts(pose).size() == bare + 1 + 3);  // helmet; waist and two legs
    auto models = build_models();
    CHECK(models.count(MODEL_ARMOR_HELMET + static_cast<int>(Material::Diamond)));
    CHECK(models.count(MODEL_ARROW));
}
