// The compass, the clock, maps and fishing in the game (M15). The rules are in navigation.cpp
// and fishing.cpp.
#include <algorithm>
#include <cmath>
#include <system_error>

#include <glm/geometric.hpp>

#include "game.h"
#include "icons.h"
#include "textures.h"

namespace {
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
}  // namespace

// --- compass, clock and maps ---------------------------------------------------------------

MapData& Game::map_data(int id) {
    auto it = maps.find(id);
    if (it != maps.end()) return it->second;
    std::optional<MapData> loaded;
    if (auto bytes = read_file(map_path(saver->dir(), id))) loaded = decode_map(*bytes);
    // A map whose file is missing starts again where the player is.
    MapData m = loaded ? *loaded : new_map(id, player.x, player.z);
    m.id = id;
    level.next_map = std::max(level.next_map, id + 1);
    return maps.emplace(id, std::move(m)).first->second;
}

MapData* Game::held_map() {
    Inventory& inv = active_inventory();
    auto& slot = inv.slots[inv.selected];
    if (!slot || slot->item != MAP) return nullptr;
    if (slot->damage <= 0) {
        // A new (blank) map: it gets the next number and is centred where the player stands (Beta).
        const int id = level.next_map++;
        maps[id] = new_map(id, player.x, player.z);
        slot->damage = id;
    }
    return &map_data(slot->damage);
}

void Game::tick_navigation() {
    // The needles move every tick, held or not, like Beta's animated textures.
    // In the Nether both spin about at random (Beta).
    const bool lost = world.nether();
    compass_needle.tick(lost ? rng.uniform(0, 6.283185307179586) : compass_target({player.x, player.y, player.z}, yaw, spawn));
    clock_needle.tick(lost ? rng.uniform(0, 6.283185307179586) : clock_target(static_cast<double>(level.time)));
    renderer->update_atlas(item_info(COMPASS)->icon.top, compass_icon(compass_needle.angle));
    renderer->update_atlas(item_info(CLOCK)->icon.top, clock_icon(clock_needle.angle));
    // A map in hand fills in as the player goes, and its picture goes into the atlas when it changed.
    if (vitals.dead()) return;
    if (MapData* m = held_map()) {
        // Maps only draw the overworld; one held in the Nether stays as it was.
        if (!world.nether()) update_map(*m, world, player.x, player.z, level.time);
        if (m->id != shown_map_ || m->version != shown_map_version_) {
            renderer->update_atlas(MAP_TILE_BASE, map_image(*m));
            shown_map_ = m->id;
            shown_map_version_ = m->version;
        }
    }
}

void Game::save_maps() {
    for (auto& [id, m] : maps) {
        if (!m.dirty) continue;
        std::error_code ec;
        std::filesystem::create_directories(map_path(saver->dir(), id).parent_path(), ec);
        if (write_file_atomic(map_path(saver->dir(), id), encode_map(m))) m.dirty = false;
    }
}

void Game::draw_held_map(int w, int h) {
    const Inventory& inv = active_inventory();
    const auto& slot = inv.slots[inv.selected];
    if (!slot || slot->item != MAP || slot->damage != shown_map_ || hand.shown() != MAP) return;
    const MapData& m = maps.at(shown_map_);
    // Held up in front of the player, low in the view, sliding up as the hand comes up.
    const float size = std::min(w, h) * 0.62f;
    const float x = (w - size) / 2;
    const float y = size * 0.06f - (1 - hand.equipped()) * size;
    const float border = size * 0.05f;
    painter_.begin(w, h);
    painter_.rect(x, y, size, size, {0.47f, 0.38f, 0.22f, 1});
    painter_.rect(x + border * 0.3f, y + border * 0.3f, size - border * 0.6f, size - border * 0.6f, {0.87f, 0.81f, 0.64f, 1});
    const float inner = size - 2 * border, ix = x + border, iy = y + border;
    painter_.tiles(MAP_TILE_BASE, MAP_SIZE / TILE, MAP_SIZE / TILE, ix, iy, inner, inner, {1, 1, 1, 1});
    // The player's marker, pointing the way they look (north is up on the map).
    glm::dvec2 p = m.to_map(player.x, player.z);
    p = glm::clamp(p, glm::dvec2(1.5), glm::dvec2(MAP_SIZE - 1.5));
    const float px = ix + static_cast<float>(p.x / MAP_SIZE) * inner, py = iy + inner - static_cast<float>(p.y / MAP_SIZE) * inner;
    const float r = inner * 4 / MAP_SIZE;  // 8 map pixels across
    const double a = yaw * 3.14159265358979323846 / 180.0;
    const float ux = static_cast<float>(std::sin(a)) * r, uy = static_cast<float>(-std::cos(a)) * r;  // forward on the map
    const float rx = uy, ry = -ux;                                                                    // to its right
    const float xy[4][2] = {{px - rx - ux, py - ry - uy}, {px + rx - ux, py + ry - uy}, {px + rx + ux, py + ry + uy},
                            {px - rx + ux, py - ry + uy}};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    painter_.tile_quad(MAP_MARKER_TILE, xy, uv, {1, 1, 1, 1});
    painter_.end();
}

// --- fishing -------------------------------------------------------------------------------

glm::dvec3 Game::rod_tip() const {
    // Where the line leaves the rod: up and to the right of the eyes in first person, at the end of
    // the rod in the player's hand in third person.
    const glm::dvec3 f(forward());
    glm::dvec3 right = glm::cross(f, glm::dvec3(0, 1, 0));
    right = glm::length(right) > 1e-6 ? glm::normalize(right) : glm::dvec3(1, 0, 0);
    const glm::dvec3 up = glm::cross(right, f);
    if (third_person) {
        const double a = yaw * 3.14159265358979323846 / 180.0;
        const glm::dvec3 ahead(std::sin(a), 0, std::cos(a)), side(-std::cos(a), 0, std::sin(a));
        return glm::dvec3(player.x, player.y + 2.0, player.z) + ahead * 1.0 + side * 0.4;
    }
    return glm::dvec3(eye()) + f * 0.6 + right * 0.34 + up * 0.03;
}

void Game::use_fishing_rod() {
    mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);  // one click casts, the next reels in
    hand.swing();
    if (bobber) {
        const Reel r = reel_result(*bobber);
        const glm::dvec3 b = bobber->pos(), to = glm::dvec3(player.x, player.y + 1, player.z) - b;
        if (r == Reel::Mob) {
            // Pull the hooked mob towards the player (Beta: a tenth of the way per tick, and up a little).
            const double up = (to.y * 0.1 + std::sqrt(glm::length(to)) * 0.08) * 20;
            for (auto& a : animals)
                if (a.get() == bobber->hooked) {
                    a->knock_x = to.x * 2;
                    a->knock_z = to.z * 2;
                    a->body.vy = up;
                }
            for (auto& m : monsters)
                if (m.get() == bobber->hooked) {
                    m->knock_x = to.x * 2;
                    m->knock_z = to.z * 2;
                    m->body.vy = up;
                }
        } else if (r == Reel::Fish) {
            // The fish flies out of the water to the player's feet.
            auto fish = std::make_unique<ItemDrop>(world, Stack{RAW_FISH, 1}, b.x, b.y + 0.2, b.z, fishing_rng_);
            const double t = std::clamp(0.4 + glm::length(to) * 0.04, 0.4, 1.2);
            fish->body.vx = to.x / t;
            fish->body.vz = to.z / t;
            fish->body.vy = (to.y - 1) / t + 0.5 * GRAVITY * t;
            fish->pickup_delay = 0;
            drops.push_back(std::move(fish));
            ++fish_caught;
            count(STAT_FISH);
        }
        wear_selected(reel_wear(r));
        bobber.reset();
        return;
    }
    glm::dvec3 dir(forward());
    dir += glm::dvec3(fishing_rng_.uniform(-1, 1), fishing_rng_.uniform(-1, 1), fishing_rng_.uniform(-1, 1)) * 0.0075;
    const glm::dvec3 from = glm::dvec3(eye()) + glm::normalize(dir) * 0.3 - glm::dvec3(0, 0.225, 0);
    bobber = std::make_unique<Bobber>(world, from, glm::normalize(dir) * CAST_SPEED);
}

void Game::tick_bobber() {
    if (!bobber) return;
    Bobber& b = *bobber;
    const auto held = active_inventory().selected_item();
    const glm::dvec3 p = b.pos();
    if (vitals.dead() || held != FISHING_ROD || glm::length(p - glm::dvec3(player.x, player.y, player.z)) > MAX_LINE ||
        b.ground_ticks >= GROUND_LIFETIME || !world.is_loaded(ifloor(p.x), ifloor(p.z)) || p.y < -16) {
        bobber.reset();
        return;
    }
    if (b.hooked) {
        // It rides on the mob it caught, until the mob is gone.
        const Body* on = nullptr;
        for (const auto& a : animals)
            if (a.get() == b.hooked && a->alive()) on = &a->body;
        for (const auto& m : monsters)
            if (m.get() == b.hooked && m->alive()) on = &m->body;
        if (!on) {
            b.hooked = nullptr;
            b.hooked_kind = -1;
        } else {
            b.body.x = on->x;
            b.body.y = on->y + on->height * 0.8;
            b.body.z = on->z;
            return;
        }
    }
    std::vector<ArrowTarget> mobs;
    for (size_t i = 0; i < animals.size(); ++i)
        if (animals[i]->alive() && animals[i].get() != riding) {
            ArrowTarget t = animals[i]->target(0, static_cast<int>(i));
            t.who = animals[i].get();
            mobs.push_back(t);
        }
    for (size_t i = 0; i < monsters.size(); ++i)
        if (monsters[i]->alive()) {
            ArrowTarget t = monsters[i]->target(1, static_cast<int>(i));
            t.who = monsters[i].get();
            mobs.push_back(t);
        }
    const Bobber::Tick t = b.tick(fishing_rng_, rain_at({ifloor(p.x), ifloor(p.y) + 1, ifloor(p.z)}), mobs);
    if (t.bite) add_puffs(puffs, b.pos() + glm::dvec3(0, 0.1, 0), 6, 0.2, 1.5, 0.12, fishing_rng_);  // the splash
}

std::vector<ModelPart> Game::fishing_parts() {
    if (!bobber) return {};
    std::vector<ModelPart> out = bobber->parts();
    const glm::dvec3 top = bobber->pos() + glm::dvec3(0, 0.22, 0);
    for (const ModelPart& part : fishing_line(rod_tip(), top)) out.push_back(part);
    return out;
}

// --- throwing ------------------------------------------------------------------------------

void Game::throw_item(ItemId item) {
    // Thrown like Beta's eggs and snowballs: as fast as an arrow, a little less accurate.
    if (!creative) active_inventory().take_selected();
    glm::dvec3 dir(forward());
    dir += glm::dvec3(animal_rng_.uniform(-1, 1), animal_rng_.uniform(-1, 1), animal_rng_.uniform(-1, 1)) * 0.0075;
    dir = glm::normalize(dir);
    auto thrown = std::make_unique<Arrow>(glm::dvec3(eye()) + dir * 0.3, dir * Arrow::SPEED);
    thrown->item = item;
    arrows.push_back(std::move(thrown));
    hand.swing();
}
