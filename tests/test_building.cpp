// Building (M11): shapes, placing, doors, chests, paintings, what falls off, drops and saving.
#include <doctest/doctest.h>

#include <algorithm>
#include <set>

#include <glm/geometric.hpp>

#include "building.h"
#include "chest.h"
#include "container.h"
#include "items.h"
#include "painting.h"
#include "physics.h"
#include "save.h"
#include "sound.h"
#include "world.h"

namespace {

// Stone up to y = 9, so the ground is at y = 10.
std::unique_ptr<Chunk> stone_chunk(int cx, int cz) {
    auto c = std::make_unique<Chunk>(cx, cz);
    for (int y = 0; y < 10; ++y)
        for (int i = 0; i < CHUNK * CHUNK; ++i) c->blocks[(y << 8) | i] = STONE;
    c->max_y = 10;
    return c;
}

World stone_world() {
    World w(1);
    for (int cx = -1; cx <= 1; ++cx)
        for (int cz = -1; cz <= 1; ++cz) w.insert(stone_chunk(cx, cz));
    return w;
}

constexpr int G = 10;  // the first air layer
const glm::ivec3 UP{0, 1, 0};
const glm::ivec3 FLOOR{4, G - 1, 4};  // a stone block to click the top of

// Put what placement() returns into the world.
void put(World& w, const std::vector<PlacedCell>& cells) {
    for (const PlacedCell& c : cells) {
        if (w.get_block(c.pos.x, c.pos.y, c.pos.z) == c.block)
            w.set_data(c.pos.x, c.pos.y, c.pos.z, c.data);
        else
            w.set_block(c.pos.x, c.pos.y, c.pos.z, c.block, c.data);
    }
}

bool place(World& w, ItemId item, glm::ivec3 clicked, glm::ivec3 face, double yaw = 0) {
    auto cells = placement(w, item, clicked, face, yaw);
    if (!cells) return false;
    put(w, *cells);
    return true;
}

}  // namespace

TEST_CASE("wool takes its colour from the item and drops the same colour") {
    World w = stone_world();
    REQUIRE(place(w, RED_WOOL, FLOOR, UP));
    CHECK(w.get_block(4, G, 4) == WOOL);
    CHECK(w.get_data(4, G, 4) == RED_WOOL - WHITE_WOOL);
    CHECK(data_drops(WOOL, RED_WOOL - WHITE_WOOL) == std::vector<ItemId>{RED_WOOL});
    CHECK(placeable_future_block(BLACK_WOOL));
    CHECK(placeable_future_block(COBBLE_SLAB));
}

TEST_CASE("two slabs of a kind make a double slab, which drops two") {
    World w = stone_world();
    REQUIRE(place(w, SANDSTONE_SLAB, FLOOR, UP));
    CHECK(w.get_block(4, G, 4) == SLAB);
    CHECK(w.get_data(4, G, 4) == 1);
    // Clicking the slab's top puts the second into the same cell.
    REQUIRE(place(w, SANDSTONE_SLAB, {4, G, 4}, UP));
    CHECK(w.get_block(4, G, 4) == DOUBLE_SLAB);
    CHECK(w.get_block(4, G + 1, 4) == AIR);
    CHECK(data_drops(DOUBLE_SLAB, 1) == std::vector<ItemId>{SANDSTONE_SLAB, SANDSTONE_SLAB});
    // A slab of another kind just goes on top.
    w.set_block(6, G, 4, SLAB, 0);
    REQUIRE(place(w, WOOD_SLAB, {6, G, 4}, UP));
    CHECK(w.get_block(6, G, 4) == SLAB);
    CHECK(w.get_block(6, G + 1, 4) == SLAB);
    CHECK(w.get_data(6, G + 1, 4) == 2);
}

TEST_CASE("shapes: a slab is half a block, stairs have a back, a fence stands 1.5 high") {
    const FenceLinks none{};
    auto slab = shape_boxes(SLAB, 0, none, true);
    REQUIRE(slab.size() == 1);
    CHECK(slab[0].hi.y == doctest::Approx(0.5));
    auto stairs = shape_boxes(WOOD_STAIRS, 0, none, true);
    CHECK(stairs.size() == 2);
    float top = 0;
    for (const Box& b : stairs) top = std::max(top, b.hi.y);
    CHECK(top == doctest::Approx(1.0));
    auto fence = shape_boxes(FENCE, 0, none, true);
    REQUIRE_FALSE(fence.empty());
    CHECK(fence[0].hi.y == doctest::Approx(1.5));
    CHECK(shape_boxes(FENCE, 0, none, false)[0].hi.y == doctest::Approx(1.0));  // aimed at as a block high
    CHECK(shape_boxes(SIGN_POST, 0, none, true).empty());  // you walk through signs
    CHECK_FALSE(shape_boxes(SIGN_POST, 0, none, false).empty());
}

TEST_CASE("aiming at the top of a slab picks the cell above it, as Beta does") {
    World w = stone_world();
    w.set_block(4, G, 4, SLAB, 0);
    // From the north and above, the ray comes in through the slab cell's north side but meets the slab's top.
    const glm::vec3 eye(4.5f, G + 1.6f, 2.0f), target(4.5f, G + 0.45f, 4.5f);
    auto hit = w.raycast(eye, glm::normalize(target - eye), 5);
    REQUIRE(hit);
    CHECK(hit->block == glm::ivec3(4, G, 4));
    REQUIRE(hit->before);
    CHECK(*hit->before == glm::ivec3(4, G + 1, 4));
}

TEST_CASE("fences join fences only and need something under them") {
    World w = stone_world();
    REQUIRE(place(w, FENCE, FLOOR, UP));
    REQUIRE(place(w, FENCE, FLOOR + glm::ivec3(1, 0, 0), UP));
    w.set_block(4, G, 5, STONE);
    const FenceLinks links = fence_links(w, {4, G, 4});
    CHECK(links[facing_of({1, 0, 0})]);
    CHECK_FALSE(links[facing_of({0, 0, 1})]);  // stone isn't a fence (Beta 1.7.3)
    CHECK_FALSE(placement(w, FENCE, {8, G + 3, 8}, {1, 0, 0}, 0));  // nothing below in the air
}

TEST_CASE("a body can't step over a fence but walks up a slab") {
    World w = stone_world();
    w.set_block(5, G, 4, FENCE);
    Body b(w, 4.5, G, 4.5, 0.3, 1.8);
    b.step_height = 0.5;
    b.on_ground = true;
    for (int i = 0; i < 20; ++i) b.move(0.1, 0, 0);
    CHECK(b.x == doctest::Approx(5 + 6.0 / 16 - 0.3));  // against the post
    CHECK(b.y == doctest::Approx(G));

    World w2 = stone_world();
    w2.set_block(5, G, 4, SLAB, 0);
    Body c(w2, 4.5, G, 4.5, 0.3, 1.8);
    c.step_height = 0.5;
    c.on_ground = true;
    for (int i = 0; i < 12; ++i) {  // onto the slab, not off its far side
        c.move(0.1, -0.05, 0);
        c.on_ground = true;
    }
    CHECK(c.x > 5.4);
    CHECK(c.y == doctest::Approx(G + 0.5));
}

TEST_CASE("ladders and trapdoors go on walls only; signs on top or on a side") {
    World w = stone_world();
    w.set_block(4, G, 4, STONE);
    CHECK_FALSE(placement(w, LADDER, {4, G, 4}, UP, 0));
    REQUIRE(place(w, LADDER, {4, G, 4}, {1, 0, 0}));
    CHECK(w.get_block(5, G, 4) == LADDER);
    CHECK(facing_dir(w.get_data(5, G, 4)) == glm::ivec3(-1, 0, 0));  // the wall is to the west
    CHECK(climbable(LADDER));

    REQUIRE(place(w, SIGN, {4, G, 4}, {0, 0, 1}));
    CHECK(w.get_block(4, G, 5) == WALL_SIGN);
    REQUIRE(place(w, SIGN, {4, G, 4}, UP, 180));
    CHECK(w.get_block(4, G + 1, 4) == SIGN_POST);
    CHECK(w.get_data(4, G + 1, 4) == 0);  // looking north, it faces south (back at you)
    CHECK(data_drops(WALL_SIGN, 0) == std::vector<ItemId>{SIGN});
}

TEST_CASE("a door takes two cells, opens both halves, and iron doors stay shut") {
    World w = stone_world();
    REQUIRE(place(w, WOOD_DOOR, FLOOR, UP, 0));
    CHECK(w.get_block(4, G, 4) == WOOD_DOOR_BLOCK);
    CHECK(w.get_block(4, G + 1, 4) == WOOD_DOOR_BLOCK);
    CHECK((w.get_data(4, G + 1, 4) & DOOR_UPPER) != 0);
    CHECK(joined_cells(w, {4, G + 1, 4}).size() == 2);
    auto toggled = toggle_door(w, {4, G + 1, 4});
    REQUIRE(toggled);
    put(w, *toggled);
    CHECK(door_open(w.get_data(4, G, 4)));
    CHECK(door_open(w.get_data(4, G + 1, 4)));
    // A door needs two free cells and the top of a block.
    w.set_block(6, G + 1, 4, STONE);
    CHECK_FALSE(placement(w, WOOD_DOOR, {6, G - 1, 4}, UP, 0));
    CHECK_FALSE(placement(w, WOOD_DOOR, {6, G - 1, 4}, {1, 0, 0}, 0));
    REQUIRE(place(w, IRON_DOOR, {8, G - 1, 4}, UP, 0));
    auto iron = toggle_door(w, {8, G, 4});
    REQUIRE(iron);
    CHECK(iron->empty());  // needs redstone
    CHECK(data_drops(IRON_DOOR_BLOCK, 0) == std::vector<ItemId>{IRON_DOOR});
}

TEST_CASE("an open door lets you through where the closed one stops you") {
    World w = stone_world();
    REQUIRE(place(w, WOOD_DOOR, FLOOR, UP, 0));
    const int d = w.get_data(4, G, 4);
    const auto closed = shape_boxes(WOOD_DOOR_BLOCK, d, {}, true);
    const auto open = shape_boxes(WOOD_DOOR_BLOCK, d | DOOR_OPEN, {}, true);
    REQUIRE(closed.size() == 1);
    REQUIRE(open.size() == 1);
    CHECK(closed[0] != open[0]);
    // Either way it's a thin panel.
    for (const Box& b : {closed[0], open[0]}) CHECK(std::min(b.hi.x - b.lo.x, b.hi.z - b.lo.z) == doctest::Approx(3.0f / 16));
}

TEST_CASE("things fall off when what holds them goes") {
    World w = stone_world();
    w.set_block(4, G, 4, STONE);
    REQUIRE(place(w, LADDER, {4, G, 4}, {1, 0, 0}));
    REQUIRE(place(w, WOOD_DOOR, {8, G - 1, 4}, UP, 0));
    w.set_block(4, G, 4, AIR);
    auto fell = fall_off(w, {4, G, 4});
    REQUIRE(fell.size() == 1);
    CHECK(fell[0].first == glm::ivec3(5, G, 4));
    CHECK(fell[0].second == LADDER);
    w.set_block(8, G - 1, 4, AIR);
    fell = fall_off(w, {8, G - 1, 4});
    REQUIRE(fell.size() == 1);
    CHECK(fell[0].first == glm::ivec3(8, G, 4));
    CHECK(fell[0].second == WOOD_DOOR);
    CHECK(joined_cells(w, fell[0].first).size() == 2);
}

TEST_CASE("chests join into double chests, never three in a row, facing the same way") {
    World w = stone_world();
    REQUIRE(place(w, CHEST, FLOOR, UP, 0));
    CHECK(w.get_data(4, G, 4) == 2);  // the player looks south, so its front faces north, at them
    CHECK_FALSE(chest_partner(w, {4, G, 4}));
    REQUIRE(place(w, CHEST, FLOOR + glm::ivec3(1, 0, 0), UP, 0));
    CHECK(chest_partner(w, {4, G, 4}) == glm::ivec3(5, G, 4));
    CHECK(w.get_data(4, G, 4) == w.get_data(5, G, 4));
    CHECK_FALSE(placement(w, CHEST, FLOOR + glm::ivec3(2, 0, 0), UP, 0));  // a third beside them
    CHECK_FALSE(placement(w, CHEST, FLOOR + glm::ivec3(-1, 0, 0), UP, 0));
    // Looking along the join, the fronts turn to face the long side.
    World v = stone_world();
    REQUIRE(place(v, CHEST, FLOOR, UP, 90));
    REQUIRE(place(v, CHEST, FLOOR + glm::ivec3(1, 0, 0), UP, 90));
    const glm::ivec3 front = facing_dir(v.get_data(4, G, 4));
    CHECK(front.x == 0);
    CHECK(v.get_data(4, G, 4) == v.get_data(5, G, 4));
    const ChestPair pair = chest_pair(w, {5, G, 4});
    CHECK(pair.first == glm::ivec3(4, G, 4));  // the west half shows on top
}

TEST_CASE("a double chest's screen moves stacks between the chest and the inventory") {
    World w = stone_world();
    put(w, *placement(w, CHEST, FLOOR, UP, 0));
    put(w, *placement(w, CHEST, FLOOR + glm::ivec3(1, 0, 0), UP, 0));
    ChestSlots* a = chest_slots(w, {4, G, 4});
    ChestSlots* b = chest_slots(w, {5, G, 4});
    REQUIRE(a);
    REQUIRE(b);
    Inventory inv;
    inv.slots[0] = Stack{COBBLE, 64};
    CraftingMenu menu(inv, *a, b);
    CHECK(menu.chest_slots() == 2 * CHEST_SLOTS);
    menu.click({Slot::Inv, 0}, 0, true);
    CHECK_FALSE(inv.slots[0]);
    REQUIRE((*a)[0]);
    CHECK((*a)[0]->count == 64);
    (*b)[5] = Stack{IRON_INGOT, 3};
    menu.click({Slot::Chest, CHEST_SLOTS + 5}, 0, true);
    CHECK_FALSE((*b)[5]);
    REQUIRE(inv.slots[Inventory::HOTBAR - 1]);  // into the hotbar from the right
    CHECK(inv.slots[Inventory::HOTBAR - 1]->item == IRON_INGOT);
    // Breaking the chest takes its slots with it.
    w.set_block(4, G, 4, AIR);
    CHECK(w.chunk_at({4, G, 4})->chests.count(cell_index({4, G, 4})) == 0);
}

TEST_CASE("dungeon loot comes from Beta's table") {
    Rng rng(5);
    std::set<ItemId> seen;
    for (int i = 0; i < 200; ++i) {
        ChestSlots slots{};
        dungeon_loot(rng, slots);
        for (const auto& s : slots)
            if (s) seen.insert(s->item);
    }
    for (ItemId item : {SADDLE, IRON_INGOT, BREAD, WHEAT, GUNPOWDER, STRING, BUCKET}) CHECK(seen.count(item));
    CHECK((seen.count(RECORD_13) || seen.count(RECORD_CAT)));
}

TEST_CASE("paintings hang on walls in sizes that fit and come down together") {
    World w = stone_world();
    for (int x = 0; x < 8; ++x)
        for (int y = G; y < G + 6; ++y) w.set_block(x, y, 4, STONE);
    Rng rng(3);
    auto cells = hang_painting(w, {3, G + 2, 4}, {0, 0, 1}, rng);
    REQUIRE(cells);
    const auto& motif = painting_motifs()[(*cells)[0].second.motif];
    CHECK(static_cast<int>(cells->size()) == motif.w * motif.h);
    place_painting(w, facing_of({0, 0, -1}), *cells);
    for (const auto& [pos, part] : *cells) {
        CHECK(w.get_block(pos.x, pos.y, pos.z) == PAINTING_BLOCK);
        CHECK(pos.z == 5);
    }
    CHECK(painting_cells(w, (*cells)[0].first).size() == cells->size());
    CHECK(data_drops(PAINTING_BLOCK, 0) == std::vector<ItemId>{PAINTING});
    // No wall, no painting.
    World empty = stone_world();
    CHECK_FALSE(hang_painting(empty, {3, G + 2, 4}, {0, 0, 1}, rng));
}

TEST_CASE("every painting motif has art and Beta's sizes") {
    int big = 0;
    for (const PaintingMotif& m : painting_motifs()) {
        CHECK(m.w >= 1);
        CHECK(m.w <= 4);
        CHECK(m.h >= 1);
        CHECK(m.h <= 4);
        big += m.w == 4 && m.h == 4;
    }
    CHECK(big >= 1);
}

TEST_CASE("note blocks: instruments from the block below, pitches over two octaves") {
    CHECK(note_instrument(STONE) == 1);
    CHECK(note_instrument(SAND) == 2);
    CHECK(note_instrument(GLASS) == 3);
    CHECK(note_instrument(PLANKS) == 4);
    CHECK(note_instrument(DIRT) == 0);
    CHECK(note_pitch(0) == doctest::Approx(0.5f));
    CHECK(note_pitch(12) == doctest::Approx(1.0f));
    CHECK(note_pitch(24) == doctest::Approx(2.0f));
    CHECK(sound_range(Sound::RecordCat) > sound_range(Sound::NoteHarp));
    CHECK(make_sound(Sound::RecordCat).size() > static_cast<size_t>(SOUND_RATE * 30));
}

TEST_CASE("chunks keep chests, signs, paintings, notes and discs through a save") {
    auto c = stone_chunk(0, 0);
    const int chest = cell_index({1, G, 1}), sign = cell_index({2, G, 1}), paint = cell_index({3, G, 1}),
              note = cell_index({4, G, 1}), juke = cell_index({5, G, 1}), gone = cell_index({6, G, 1});
    c->blocks[chest] = CHEST;
    c->blocks[sign] = SIGN_POST;
    c->blocks[paint] = PAINTING_BLOCK;
    c->blocks[note] = NOTE_BLOCK;
    c->blocks[juke] = JUKEBOX;
    c->chests[chest][3] = Stack{DIAMOND, 2};
    c->signs[sign] = {"Hello", "", "PetrutzCraft", "!"};
    c->paintings[paint] = {2, 1, 0};
    c->notes[note] = 17;
    c->records[juke] = RECORD_CAT;
    c->notes[gone] = 3;  // no note block there any more: dropped on load
    auto d = decode_chunk(encode_chunk(*c), 0, 0);
    REQUIRE(d);
    REQUIRE(d->chests.count(chest));
    CHECK(d->chests[chest][3] == std::optional<Stack>(Stack{DIAMOND, 2}));
    CHECK(d->signs[sign] == c->signs[sign]);
    CHECK(d->paintings[paint] == c->paintings[paint]);
    CHECK(d->notes[note] == 17);
    CHECK(d->records[juke] == RECORD_CAT);
    CHECK(d->notes.count(gone) == 0);
}

TEST_CASE("old placeholder items turn into the blocks") {
    CHECK(current_item_id(OLD_BOOKSHELF_ITEM) == BOOKSHELF);
    CHECK(current_item_id(OLD_FENCE_ITEM) == FENCE);
    CHECK(current_item_id(OLD_WOOD_STAIRS_ITEM) == WOOD_STAIRS);
    CHECK(current_item_id(OLD_JUKEBOX_ITEM) == JUKEBOX);
    CHECK(is_block(BOOKSHELF));
    CHECK(item_name(RECORD_13) == "Music Disc 13");
}

TEST_CASE("generated chunks, dungeon chests and all, come back from a save the same") {
    World w(7);
    int chests = 0;
    for (int cx = -6; cx <= 6; ++cx)
        for (int cz = -6; cz <= 6; ++cz) {
            Chunk& c = w.chunk(cx, cz);
            chests += static_cast<int>(c.chests.size());
            auto back = decode_chunk(encode_chunk(c), cx, cz);
            REQUIRE(back);
            CHECK(back->blocks == c.blocks);
            CHECK(back->data == c.data);
            CHECK(back->chests.size() == c.chests.size());
        }
    MESSAGE("dungeon chests found: " << chests);
}
