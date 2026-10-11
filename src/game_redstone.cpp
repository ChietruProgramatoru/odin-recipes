// Redstone in the game (M13): placing the parts, flipping levers and pressing buttons, who stands
// on pressure plates, redstone ore glowing when touched, and what circuits set off (TNT, note
// blocks, dispensers firing, pistons shoving players and mobs). The rules are in redstone.cpp.
#include <algorithm>
#include <cmath>

#include "chest.h"
#include "game.h"
#include "redstone.h"

namespace {
int ifloor(double v) { return static_cast<int>(std::floor(v)); }

// A body's feet are on a plate in this cell (Beta: the plate's box, 1/16 in from the sides, up to a
// quarter block high).
bool body_on_plate(const Body& b, glm::ivec3 p) {
    constexpr double IN = 1.0 / 16;
    return b.x + b.half > p.x + IN && b.x - b.half < p.x + 1 - IN && b.z + b.half > p.z + IN && b.z - b.half < p.z + 1 - IN &&
           b.y >= p.y - 0.01 && b.y < p.y + 0.25;
}
}  // namespace

bool Game::on_plate(glm::ivec3 p, BlockId plate) {
    if (!vitals.dead() && body_on_plate(player, p)) return true;
    for (const auto& a : animals)
        if (body_on_plate(a->body, p)) return true;
    for (const auto& m : monsters)
        if (body_on_plate(m->body, p)) return true;
    if (plate != WOOD_PLATE_BLOCK) return false;
    // Wooden plates feel anything: items and arrows too.
    for (const auto& d : drops)
        if (body_on_plate(d->body, p)) return true;
    for (const auto& a : arrows)
        if (ifloor(a->pos.x) == p.x && ifloor(a->pos.y) == p.y && ifloor(a->pos.z) == p.z) return true;
    return false;
}

void Game::tick_redstone_inputs(BlockEvents& events) {
    Redstone& rs = blocks.redstone();
    auto feet = [&](const Body& b) { return glm::ivec3(ifloor(b.x), ifloor(b.y + 0.01), ifloor(b.z)); };
    auto step_on = [&](glm::ivec3 c) {
        const BlockId b = world.get_block(c.x, c.y, c.z);
        if (is_plate(b) && on_plate(c, b)) rs.press_plate(c, blocks.now(), events.dirty, events.redstone);
    };
    if (!vitals.dead()) step_on(feet(player));
    for (const auto& a : animals) step_on(feet(a->body));
    for (const auto& m : monsters) step_on(feet(m->body));
    for (const auto& d : drops) step_on(feet(d->body));
    for (const auto& a : arrows) step_on({ifloor(a->pos.x), ifloor(a->pos.y), ifloor(a->pos.z)});
    // Redstone ore lights up when it is hit or walked on (Beta), and goes out again by itself.
    std::vector<glm::ivec3> touched;
    if (mining_pos) touched.push_back(*mining_pos);
    if (player.on_ground && !vitals.dead()) touched.push_back(feet(player) - glm::ivec3(0, 1, 0));
    for (glm::ivec3 c : touched)
        if (world.get_block(c.x, c.y, c.z) == REDSTONE_ORE) events.rebuild(world.set_block(c.x, c.y, c.z, GLOWING_REDSTONE_ORE));
}

void Game::apply_redstone(RedstoneEvents& ev) {
    for (const auto& [p, item] : ev.drops)
        if (!creative) spawn_drop(item, p.x + 0.5, p.y + 0.3, p.z + 0.5);
    for (glm::ivec3 p : ev.tnt) prime_tnt(p, TNT_FUSE_TICKS);
    for (glm::ivec3 p : ev.notes) play_note(p);
    for (glm::ivec3 p : ev.clicks) play_sound(Sound::Click, glm::dvec3(p) + 0.5, 0.6f);
    for (glm::ivec3 p : ev.dispense) dispense(p);
    for (const PistonMove& m : ev.pistons) {
        if (!m.dirty.empty()) renderer->rebuild(m.dirty);
        for (const auto& [p, item] : m.drops) spawn_drop(item, p.x + 0.5, p.y + 0.3, p.z + 0.5);
        push_entities(m);
        play_sound(m.extended ? Sound::PistonOut : Sound::PistonIn, glm::dvec3(m.piston) + 0.5);
    }
    ev = {};
}

void Game::push_entities(const PistonMove& m) {
    if (m.moved_to.empty()) return;
    // Anything in a cell a block (or the head) moved into goes along with it, a block's width.
    const glm::dvec3 d(m.dir);
    auto touches = [&](const Body& b) {
        for (glm::ivec3 c : m.moved_to)
            if (b.x + b.half > c.x && b.x - b.half < c.x + 1 && b.z + b.half > c.z && b.z - b.half < c.z + 1 && b.y < c.y + 1.01 &&
                b.y + b.height > c.y)
                return true;
        return false;
    };
    auto push = [&](Body& b) {
        if (!touches(b)) return;
        b.x += d.x;
        b.y += d.y;
        b.z += d.z;
        if (d.y > 0) b.vy = std::max(b.vy, 0.0);
    };
    if (!in_vehicle && !sleeping_in) {
        push(player);
        if (d.y > 0) fall_peak = player.y;
    }
    for (auto& a : animals) push(a->body);
    for (auto& mo : monsters) push(mo->body);
    for (auto& dr : drops) push(dr->body);
}

bool Game::place_redstone(ItemId item) {
    if (!target || targeting_mob() || !target->before) return false;
    const glm::ivec3 face = *target->before - target->block;
    auto cell = redstone_placement(world, item, target->block, face, yaw, pitch);
    if (!cell) return false;
    // Pistons, dispensers and repeaters are solid: not inside someone.
    const std::vector<Box> boxes = is_shaped(cell->block) ? shape_boxes(cell->block, cell->data, {}, true)
                                   : is_solid(cell->block) ? std::vector<Box>{{glm::vec3(0), glm::vec3(1)}}
                                                           : std::vector<Box>{};
    std::vector<const Body*> bodies{&player};
    for (const auto& a : animals) bodies.push_back(&a->body);
    for (const auto& m : monsters) bodies.push_back(&m->body);
    const glm::ivec3 c = cell->pos;
    for (const Box& box : boxes)
        for (const Body* b : bodies)
            if (c.x + box.hi.x > b->x - b->half && c.x + box.lo.x < b->x + b->half && c.z + box.hi.z > b->z - b->half &&
                c.z + box.lo.z < b->z + b->half && c.y + box.hi.y > b->y && c.y + box.lo.y < b->y + b->height)
                return false;
    renderer->rebuild(world.set_block(c.x, c.y, c.z, cell->block, cell->data));
    if (is_solid(cell->block) && world.get_block(c.x, c.y - 1, c.z) == FARMLAND)
        renderer->rebuild(world.set_block(c.x, c.y - 1, c.z, DIRT));
    if (!creative) active_inventory().take_selected();
    return true;
}

bool Game::use_redstone(glm::ivec3 pos) {
    BlockEvents events;
    if (!blocks.redstone().use(pos, blocks.now(), events.dirty, events.redstone)) return false;
    apply_block_events(events);
    return true;
}

void Game::open_dispenser(glm::ivec3 pos) {
    ChestSlots* slots = chest_slots(world, pos);
    if (!slots) return;
    show_gui(std::make_unique<InventoryScreen>(active_inventory(), *slots));
    chest_open = {pos};
}

void Game::dispense(glm::ivec3 pos) {
    if (world.get_block(pos.x, pos.y, pos.z) != DISPENSER_BLOCK) return;
    ChestSlots* slots = chest_slots(world, pos);
    if (!slots) return;
    // A random slot with something in it (Beta).
    std::vector<int> full;
    for (int i = 0; i < DISPENSER_SLOTS; ++i)
        if ((*slots)[i]) full.push_back(i);
    const glm::dvec3 mid = glm::dvec3(pos) + 0.5;
    if (full.empty()) {
        play_sound(Sound::Click, mid, 1.2f);  // Beta: a click and nothing comes out
        return;
    }
    auto& slot = (*slots)[full[rng.below(static_cast<int>(full.size()))]];
    Stack one{slot->item, 1};
    one.damage = slot->damage;
    if (--slot->count == 0) slot.reset();
    const glm::dvec3 dir(facing_dir(world.get_data(pos.x, pos.y, pos.z) & 3));
    const glm::dvec3 from = mid + dir * 0.6;
    if (one.item == ARROW) {
        // Arrows are shot, a little slower than from a bow and a little up.
        glm::dvec3 v = glm::normalize(dir + glm::dvec3(0, 0.1, 0) +
                                      glm::dvec3(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)) * 0.0075);
        auto arrow = std::make_unique<Arrow>(from, v * (Arrow::SPEED * 0.73));
        arrows.push_back(std::move(arrow));
    } else {
        auto drop = std::make_unique<ItemDrop>(world, one, from.x, from.y - 0.15, from.z, rng);
        const double speed = rng.uniform(2, 2.2);
        drop->body.vx = dir.x * speed + rng.uniform(-0.15, 0.15);
        drop->body.vy = 4;
        drop->body.vz = dir.z * speed + rng.uniform(-0.15, 0.15);
        drops.push_back(std::move(drop));
    }
    play_sound(Sound::Click, mid, 1.0f);
}
