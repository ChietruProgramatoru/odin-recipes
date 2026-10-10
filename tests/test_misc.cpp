// Cows, item drops, textures, the font and the PNG writer.
#include <doctest/doctest.h>

#include "font.h"
#include "icons.h"
#include "mobs.h"
#include "png.h"
#include "textures.h"

TEST_CASE("a cow dies after 5 punches, then drops 1 to 3 beef") {
    World w(5);
    Rng rng(1);
    int y = w.surface_y(8, 8);
    Cow cow(w, 8.5, y, 8.5, rng);
    for (int i = 0; i < 4; ++i) cow.hurt(2, 6, 8);
    CHECK(cow.alive());
    CHECK(cow.state == Cow::State::Panic);
    cow.hurt(2, 6, 8);
    CHECK_FALSE(cow.alive());
    bool finished = false;
    for (int i = 0; i < 60 && !finished; ++i) finished = cow.tick(1.0 / 60, 6, 8, rng);
    CHECK(finished);
    for (int i = 0; i < 50; ++i) {
        int n = drop_beef_count(rng);
        CHECK(n >= 1);
        CHECK(n <= 3);
    }
}

TEST_CASE("a wandering cow stays on the ground") {
    World w(5);
    Rng rng(2);
    for (int cx = -2; cx <= 2; ++cx)
        for (int cz = -2; cz <= 2; ++cz) w.chunk(cx, cz);
    int y = w.surface_y(8, 8);
    Cow cow(w, 8.5, y, 8.5, rng);
    for (int i = 0; i < 600; ++i) cow.tick(1.0 / 60, 100, 100, rng);
    CHECK(cow.body.y > 1);
    CHECK_FALSE(cow.body.collides());
}

TEST_CASE("the cow hit box is found by a ray pointed at it") {
    World w(5);
    Rng rng(3);
    Cow cow(w, 10.5, 30, 10.5, rng);
    auto t = cow.ray_hit({10.5f, 31, 7.5f}, {0, 0, 1}, 3.5f);
    REQUIRE(t);
    CHECK(*t == doctest::Approx(2.4f));
    CHECK_FALSE(cow.ray_hit({10.5f, 31, 7.5f}, {0, 0, -1}, 3.5f));
}

TEST_CASE("item drops fall and settle") {
    World w(5);
    Rng rng(4);
    int y = w.surface_y(8, 8);
    ItemDrop drop(w, Stack{BEEF, 1}, 8.5, y + 2, 8.5, rng);
    for (int i = 0; i < 300; ++i) drop.tick(1.0 / 60);
    CHECK(drop.body.on_ground);
}

TEST_CASE("models exist for cows and every item") {
    auto models = build_models();
    CHECK(models.count(MODEL_COW_BODY));
    CHECK(models.count(MODEL_COW_HEAD_HURT));
    for (ItemId item : all_items()) CHECK(models.count(ITEM_MODEL_BASE + item));
    CHECK(models[MODEL_COW_BODY].vertices.size() == 2 * 6 * 4);
}

TEST_CASE("every tile gets a 16x16 texture and the atlas has room for them") {
    auto tiles = make_tiles();
    for (int i = 0; i < TILE_COUNT; ++i) CHECK(tiles.count(i));
    CHECK(TILE_COUNT <= ITEM_TILE_BASE);  // world tiles stay below the item tiles
    CHECK(tiles.rbegin()->first < ATLAS_COLS * ATLAS_ROWS);
    // Every item's icon tiles exist.
    for (ItemId item : all_items()) {
        const ItemIcon& icon = item_info(item)->icon;
        CHECK(tiles.count(icon.top));
        if (icon.shape != ItemIcon::Flat) CHECK((tiles.count(icon.left) && tiles.count(icon.right)));
    }
    for (const auto& [i, img] : tiles) {
        CHECK(img.width == TILE);
        CHECK(img.height == TILE);
    }
    Image atlas = make_atlas(tiles);
    CHECK(atlas.width == TILE * ATLAS_COLS);
    // Water is see-through, stone is not.
    CHECK(tiles[T_WATER].at(3, 3).a == 170);
    CHECK(tiles[T_STONE].at(3, 3).a == 255);
    auto uv = tile_uv(T_GRASS_TOP);
    CHECK(uv[0] < 0.01f);
    CHECK(uv[3] > 0.99f);  // the first tile is in the top row of the atlas
}

TEST_CASE("font measures text") {
    CHECK(text_width("") == 0);
    CHECK(text_width("Hi") == 2 * GLYPH_ADVANCE - 1);
    CHECK(text_width("abc\nlonger") == 6 * GLYPH_ADVANCE - 1);
    CHECK(text_height("one\ntwo") == LINE_HEIGHT + GLYPH_H);
    CHECK(glyph_rows('A')[0] == 0x0E);
    CHECK(glyph_rows('\t') == glyph_rows('?'));
}

TEST_CASE("PNG writer makes a valid header and compresses flat images") {
    std::vector<uint8_t> rgb(64 * 64 * 3, 200);
    auto png = encode_png(64, 64, rgb);
    REQUIRE(png.size() > 33);
    CHECK(png[0] == 0x89);
    CHECK(png[1] == 'P');
    CHECK(png[12] == 'I');
    CHECK(png[13] == 'H');
    CHECK(png.size() < rgb.size() / 10);
}

TEST_CASE("the player model has every body part, standing on the ground") {
    auto models = build_models();
    for (int m : {MODEL_PLAYER_HEAD, MODEL_PLAYER_BODY, MODEL_PLAYER_ARM, MODEL_PLAYER_LEG}) CHECK_FALSE(models[m].empty());
    PlayerPose pose{5, 70, 5, 30, 10, 0, 0, false};
    auto parts = player_parts(pose);
    CHECK(parts.size() == 6);
    // The lowest and highest model points: feet at y = 70, head top at about 1.8 above.
    float lo = 1e9f, hi = -1e9f;
    for (const ModelPart& p : parts)
        for (const Vertex& v : models[p.model].vertices) {
            glm::vec4 world = p.transform * glm::vec4(v.x, v.y, v.z, 1);
            lo = std::min(lo, world.y);
            hi = std::max(hi, world.y);
        }
    CHECK(lo == doctest::Approx(70).epsilon(0.001));
    CHECK(hi == doctest::Approx(71.82).epsilon(0.005));
}
