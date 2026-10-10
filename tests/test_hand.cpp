// The first-person hand: its swing and the models of held items.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "hand.h"
#include "textures.h"

TEST_CASE("a swing lasts Beta's 8 ticks and restarts only once half done") {
    Hand hand;
    CHECK_FALSE(hand.swinging());
    CHECK(hand.swing_progress() == 0);
    hand.swing();
    CHECK(hand.swinging());
    hand.update(0.1, std::nullopt, 0);
    CHECK(hand.swing_progress() == doctest::Approx(0.25));
    hand.swing();  // too early to start over
    CHECK(hand.swing_progress() == doctest::Approx(0.25));
    hand.update(0.15, std::nullopt, 0);
    hand.swing();  // past half: starts again
    CHECK(hand.swing_progress() == 0);
    CHECK(hand.swinging());
    hand.update(Hand::SWING_SECONDS, std::nullopt, 0);
    CHECK_FALSE(hand.swinging());
    CHECK(hand.swing_progress() == 0);
}

TEST_CASE("changing the held item dips the hand out of view and brings the new item up") {
    Hand hand;
    hand.update(1.0, STICK, 0);
    CHECK(hand.shown() == std::optional<ItemId>(STICK));
    CHECK(hand.equipped() == 1);
    hand.update(0.05, DIAMOND_SWORD, 1);  // one tick: 0.4 of the way down, still the stick
    CHECK(hand.equipped() == doctest::Approx(0.6));
    CHECK(hand.shown() == std::optional<ItemId>(STICK));
    hand.update(0.1, DIAMOND_SWORD, 1);  // all the way down: the sword is taken
    CHECK(hand.equipped() < 0.1f);
    CHECK(hand.shown() == std::optional<ItemId>(DIAMOND_SWORD));
    hand.update(0.15, DIAMOND_SWORD, 1);
    CHECK(hand.equipped() == 1);
    // The same item in another slot still dips (Beta), and so does emptying the hand.
    hand.update(0.05, DIAMOND_SWORD, 2);
    CHECK(hand.equipped() < 1);
    hand.update(1.0, std::nullopt, 2);
    CHECK_FALSE(hand.shown());
    CHECK(hand.equipped() == 1);
}

TEST_CASE("view bobbing sways while walking on the ground and can be turned off") {
    Hand hand;
    hand.update(1.0, std::nullopt, 0);
    CHECK(hand.bob() == glm::mat4(1.0f));  // standing still
    hand.update(0.3, std::nullopt, 0, {4.3, 0, true});
    const glm::mat4 walking = hand.bob();
    CHECK(walking != glm::mat4(1.0f));
    CHECK(std::abs(walking[3].y) <= 0.1f);  // a gentle sway, under a tenth of a block
    hand.bobbing = false;
    CHECK(hand.bob() == glm::mat4(1.0f));
    hand.bobbing = true;
    hand.update(2.0, std::nullopt, 0, {4.3, 0, false});  // in the air the bob dies away
    CHECK(std::abs(hand.bob()[3].y) < 0.001f);
}

TEST_CASE("the hand shows the arm, or the held item's own model, and moves while swinging") {
    Hand hand;
    hand.update(1.0, std::nullopt, 0);
    auto arm = hand.parts();
    REQUIRE(arm.size() == 1);
    CHECK(arm[0].model == MODEL_PLAYER_ARM);
    Hand holding;
    holding.update(1.0, DIAMOND_SWORD, 0);
    auto sword = holding.parts();
    REQUIRE(sword.size() == 1);
    CHECK(sword[0].model == HELD_MODEL_BASE + DIAMOND_SWORD);
    hand.swing();
    holding.swing();
    hand.update(0.1, std::nullopt, 0);
    holding.update(0.1, DIAMOND_SWORD, 0);
    CHECK(holding.parts()[0].transform != sword[0].transform);
    CHECK(hand.parts()[0].transform != arm[0].transform);
    // Whatever is held stays in front of the camera (-z).
    for (float z : {sword[0].transform[3].z, arm[0].transform[3].z}) CHECK(z < 0);
}

TEST_CASE("every item has a held model: blocks as cubes, the rest as one-pixel-thick sprites") {
    const auto models = build_held_models(make_tiles());
    for (ItemId id : all_items()) {
        INFO(item_name(id));
        auto it = models.find(HELD_MODEL_BASE + id);
        REQUIRE(it != models.end());
        const MeshData& mesh = it->second;
        CHECK_FALSE(mesh.empty());
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (const Vertex& v : mesh.vertices) {
            lo = glm::min(lo, glm::vec3(v.x, v.y, v.z));
            hi = glm::max(hi, glm::vec3(v.x, v.y, v.z));
        }
        if (looks_like_block(id)) {
            CHECK(lo.x == doctest::Approx(-0.5f));
            CHECK(hi.x == doctest::Approx(0.5f));
            CHECK(lo.y == doctest::Approx(-0.5f));
        } else {
            CHECK(lo.z == doctest::Approx(-1.0f / 16));
            CHECK(hi.z == doctest::Approx(0.0f));
            CHECK(hi.x <= 1.0f);
            CHECK(mesh.vertices.size() > 8);  // edges as well as the two faces
        }
    }
    // A slab is half a block tall; stairs are two boxes.
    const MeshData& slab = models.at(HELD_MODEL_BASE + STONE_SLAB);
    float top = -1;
    for (const Vertex& v : slab.vertices) top = std::max(top, v.y);
    CHECK(top == doctest::Approx(0.0f));
    CHECK(models.at(HELD_MODEL_BASE + WOOD_STAIRS).vertices.size() == 48);
    CHECK(models.at(HELD_MODEL_BASE + GRASS).vertices.size() == 24);
}
