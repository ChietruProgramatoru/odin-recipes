// Minecarts, boats and rails in the game (M12): placing them, riding, hitting, the storage minecart's
// chest, the powered minecart's coal and detector rails. The physics is in vehicles.cpp.
#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

#include "game.h"
#include "rails.h"
#include "tools.h"

namespace {
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
}  // namespace

void Game::update_vehicles(double dt) {
    vehicle_ticks += dt * 20;
    while (vehicle_ticks >= 1) {
        vehicle_ticks -= 1;
        tick_vehicles();
    }
    if (in_vehicle) {
        // The rider sits in it, and moves smoothly between ticks with it.
        const glm::dvec3 s = in_vehicle->seat(vehicle_ticks);
        player.x = s.x;
        player.y = s.y;
        player.z = s.z;
        player.vx = player.vy = player.vz = 0;
        fall_peak = player.y;
        was_on_ground = true;
    }
}

void Game::tick_vehicles() {
    const double far_away = (renderer->render_distance + 3) * CHUNK;
    RideInput input;
    input.forward = action_down(KEY_FORWARD) - action_down(KEY_BACK);
    input.strafe = action_down(KEY_RIGHT) - action_down(KEY_LEFT);
    input.yaw = yaw;
    for (size_t i = 0; i < vehicles.size();) {
        Vehicle& v = *vehicles[i];
        if (!world.find(chunk_of(ifloor(v.body.x)), chunk_of(ifloor(v.body.z)))) {
            ++i;  // its chunk isn't there yet: wait, so it doesn't fall through the world
            continue;
        }
        if (&v != in_vehicle && std::hypot(v.body.x - player.x, v.body.z - player.z) > far_away) {
            // Too far to simulate: keep it with its chunk until the player comes back.
            stash_entity(v.record());
            remove_vehicle(i, false);
            continue;
        }
        v.ridden = &v == in_vehicle;
        // Walking into a minecart or boat shoves it along.
        if (!in_vehicle && !vitals.dead() && v.overlaps(player)) v.push_from(player.x, player.z);
        if (v.tick(v.ridden ? &input : nullptr)) {
            // A boat that crashed breaks up (or anything that fell out of the world goes).
            const bool fell = v.body.y < -64;
            if (!fell) add_puffs(puffs, {v.body.x, v.body.y + 0.3, v.body.z}, 6, 0.6, 0.6, 0.15, rng);
            remove_vehicle(i, !fell);
            continue;
        }
        ++i;
    }
    // Vehicles that run into each other push apart.
    for (size_t a = 0; a < vehicles.size(); ++a)
        for (size_t b = a + 1; b < vehicles.size(); ++b) {
            Vehicle& va = *vehicles[a];
            Vehicle& vb = *vehicles[b];
            if (!va.overlaps(vb.body)) continue;
            if (va.is_cart() && vb.is_cart()) {
                bump_carts(va, vb);
            } else {
                va.push_from(vb.body.x, vb.body.z);
                vb.push_from(va.body.x, va.body.z);
            }
        }
    // Detector rails are pressed while a minecart is on them.
    std::vector<glm::ivec3> pressed;
    for (const auto& v : vehicles) {
        if (!v->is_cart() || !v->on_rail) continue;
        glm::ivec3 c(ifloor(v->body.x), ifloor(v->body.y), ifloor(v->body.z));
        if (is_rail(world.get_block(c.x, c.y - 1, c.z))) --c.y;
        if (world.get_block(c.x, c.y, c.z) == DETECTOR_RAIL && std::find(pressed.begin(), pressed.end(), c) == pressed.end())
            pressed.push_back(c);
    }
    for (glm::ivec3 c : pressed) renderer->rebuild(set_rail_powered(world, c, true));
    for (glm::ivec3 c : pressed_detectors)
        if (std::find(pressed.begin(), pressed.end(), c) == pressed.end()) renderer->rebuild(set_rail_powered(world, c, false));
    pressed_detectors = std::move(pressed);
    // A burning powered minecart smokes.
    for (const auto& v : vehicles)
        if (v->kind == VehicleKind::PoweredMinecart && v->fuel > 0 && rng.below(4) == 0)
            add_puffs(puffs, {v->body.x, v->body.y + 0.95, v->body.z}, 1, 0.1, 0.4, 0.12, rng);
}

void Game::remove_vehicle(size_t index, bool drop_items) {
    Vehicle* v = vehicles[index].get();
    if (in_vehicle == v) get_out();
    if (chest_vehicle == v && gui) close_gui();
    chest_vehicle = chest_vehicle == v ? nullptr : chest_vehicle;
    if (target_vehicle == v) target_vehicle = nullptr;
    if (drop_items)
        for (const Stack& s : v->drops())
            drops.push_back(std::make_unique<ItemDrop>(world, s, v->body.x, v->body.y + 0.5, v->body.z, rng));
    vehicles.erase(vehicles.begin() + static_cast<long>(index));
}

bool Game::place_vehicle(ItemId item) {
    const auto kind = vehicle_for_item(item);
    if (!kind) return false;
    std::unique_ptr<Vehicle> v;
    if (*kind == VehicleKind::Boat) {
        // A boat goes on top of whatever you aim at, water included (Beta).
        auto hit = world.raycast(eye(), forward(), REACH, [](glm::ivec3, BlockId b) { return b != AIR; });
        if (!hit) return false;
        const glm::ivec3 b = hit->block;
        v = std::make_unique<Vehicle>(world, *kind, b.x + 0.5, b.y + 1 - BOAT_HEIGHT / 2, b.z + 0.5);
    } else {
        // A minecart only goes on a rail.
        if (!target || targeting_mob()) return false;
        const glm::ivec3 b = target->block;
        if (!is_rail(world.get_block(b.x, b.y, b.z))) return false;
        v = std::make_unique<Vehicle>(world, *kind, b.x + 0.5, b.y, b.z + 0.5);
    }
    if (v->body.collides()) return false;
    v->yaw = v->prev_yaw = yaw;
    vehicles.push_back(std::move(v));
    if (!creative) active_inventory().take_selected();
    return true;
}

void Game::use_vehicle(Vehicle& v) {
    if (v.kind == VehicleKind::StorageMinecart) {
        show_gui(std::make_unique<InventoryScreen>(active_inventory(), v.chest, nullptr, "Minecart"));
        chest_vehicle = &v;
        return;
    }
    if (v.kind == VehicleKind::PoweredMinecart) {
        // Coal makes it go, away from the player; a click without coal only turns it.
        Inventory& inv = active_inventory();
        if (v.add_fuel(inv.selected_item().value_or(AIR), player.x, player.z) && !creative) inv.take_selected();
        return;
    }
    if (in_vehicle == &v) {
        get_out();
    } else if (!in_vehicle && !riding) {
        get_in(v);
    }
}

void Game::hit_vehicle(Vehicle& v) {
    // Beta lets you hit vehicles as fast as you click.
    attack_cooldown = 0;
    const ItemId held = active_inventory().selected_item().value_or(AIR);
    const bool broke = v.hit(creative ? VEHICLE_BREAK_DAMAGE : attack_damage(held));
    if (!creative) vitals.exhaust(EXHAUST_ATTACK);
    if (!broke) return;
    for (size_t i = 0; i < vehicles.size(); ++i)
        if (vehicles[i].get() == &v) {
            remove_vehicle(i, !creative);  // nothing drops in creative
            break;
        }
}

void Game::get_in(Vehicle& v) {
    if (!v.rideable()) return;
    in_vehicle = &v;
    v.ridden = true;
    sneaking = flying = false;
    sprint_.stop();
    mining_pos.reset();
    hud->show_message("Press " + key_name(options.keys[KEY_SNEAK]) + " to get out", 3);
}

void Game::get_out() {
    if (!in_vehicle) return;
    Vehicle& v = *in_vehicle;
    v.ridden = false;
    in_vehicle = nullptr;
    // Out on top of it (Beta), or beside it if something is in the way.
    const glm::dvec2 spots[5] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (glm::dvec2 d : spots) {
        player.x = v.body.x + d.x * (v.body.half + player.half + 0.05);
        player.z = v.body.z + d.y * (v.body.half + player.half + 0.05);
        player.y = d == glm::dvec2(0, 0) ? v.body.y + v.body.height : v.body.y + 0.01;
        if (!player.collides()) break;
    }
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
}

void Game::lay_rail(glm::ivec3 pos) { renderer->rebuild(connect_rail(world, pos)); }
