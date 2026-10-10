// The first-person hand: its swing and the models of held items.
#include <doctest/doctest.h>

#include <algorithm>

#include "hand.h"
#include "textures.h"

TEST_CASE("a swing lasts Beta's 8 ticks and restarts only once half done") {
    Hand hand;
    CHECK_FALSE(hand.swinging());
    CHECK(hand.swing_progress() == 0);
    hand.swing();
    CHECK(hand.swinging());
    hand.update(0.1);
    CHECK(hand.swing_progress() == doctest::Approx(0.25));
    hand.swing();  // too early to start over
    CHECK(hand.swing_progress() == doctest::Approx(0.25));
    hand.update(0.15);
    hand.swing();  // past half: starts again
    CHECK(hand.swing_progress() == 0);
    CHECK(hand.swinging());
    hand.update(Hand::SWING_SECONDS);
    CHECK_FALSE(hand.swinging());
    CHECK(hand.swing_progress() == 0);
}

TEST_CASE("the hand shows the arm, or the held item's own model, and moves while swinging") {
    Hand hand;
    auto arm = hand.parts(std::nullopt);
    REQUIRE(arm.size() == 1);
    CHECK(arm[0].model == MODEL_PLAYER_ARM);
    auto sword = hand.parts(DIAMOND_SWORD);
    REQUIRE(sword.size() == 1);
    CHECK(sword[0].model == HELD_MODEL_BASE + DIAMOND_SWORD);
    hand.swing();
    hand.update(0.1);
    CHECK(hand.parts(DIAMOND_SWORD)[0].transform != sword[0].transform);
    CHECK(hand.parts(std::nullopt)[0].transform != arm[0].transform);
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
