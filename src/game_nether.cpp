// The Nether in the game (M14): lighting portals, going through them, swapping the world for the
// other dimension, ghasts' fireballs and angry zombie pigmen. Terrain and portal rules are in nether.cpp.
#include <algorithm>
#include <cmath>

#include "game.h"

namespace {
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
constexpr double LINK_RANGE = 128.0;  // a portal links to one this close to its spot on the other side (Beta)
}  // namespace

// --- Portals --------------------------------------------------------------------------------------

bool Game::try_light_portal(glm::ivec3 pos) {
    const auto shape = find_portal_frame(world, pos);
    if (!shape) return false;
    renderer->rebuild(light_portal(world, *shape));
    remember_portal(*shape);
    return true;
}

void Game::remember_portal(const PortalShape& shape) {
    const LevelData::Portal p{world.dimension(), shape.origin};
    if (std::find(level.portals.begin(), level.portals.end(), p) == level.portals.end()) level.portals.push_back(p);
}

void Game::tick_portal() {
    // Standing in a portal: any portal block in the player's box.
    bool inside = false;
    const Body& b = player;
    for (int x = ifloor(b.x - b.half); x <= ifloor(b.x + b.half) && !inside; ++x)
        for (int y = ifloor(b.y); y <= ifloor(b.y + b.height) && !inside; ++y)
            for (int z = ifloor(b.z - b.half); z <= ifloor(b.z + b.half) && !inside; ++z) inside = world.get_block(x, y, z) == PORTAL;
    if (!inside) {
        portal_cooldown = false;  // stepped out of the one they arrived in
        portal_ticks = std::max(0, portal_ticks - 4);
        return;
    }
    if (portal_cooldown || vitals.dead() || in_vehicle || riding || sleeping_in) return;
    if (++portal_ticks >= PORTAL_TICKS) travel_through_portal();
}

void Game::travel_through_portal() {
    const Dimension to = world.nether() ? Dimension::Overworld : Dimension::Nether;
    // The portal being left, so it can be linked back to later.
    std::optional<PortalShape> from;
    const Body& b = player;
    for (int x = ifloor(b.x - b.half); x <= ifloor(b.x + b.half) && !from; ++x)
        for (int y = ifloor(b.y); y <= ifloor(b.y + b.height) && !from; ++y)
            for (int z = ifloor(b.z - b.half); z <= ifloor(b.z + b.half) && !from; ++z) from = portal_at(world, {x, y, z});
    const glm::ivec3 here = from ? from->origin : glm::ivec3(ifloor(b.x), ifloor(b.y), ifloor(b.z));
    if (from) remember_portal(*from);
    const glm::ivec3 target = portal_target(here, to);

    change_dimension(to);

    // The nearest remembered portal on this side close enough to the spot (Beta searches 128 blocks
    // around it). Ones that were broken since are forgotten.
    std::optional<PortalShape> link;
    std::vector<LevelData::Portal> near;
    for (const LevelData::Portal& p : level.portals)
        if (p.dimension == to && std::hypot(p.pos.x - target.x, p.pos.z - target.z) <= LINK_RANGE) near.push_back(p);
    std::sort(near.begin(), near.end(), [&](const auto& a, const auto& c) {
        return std::hypot(a.pos.x - target.x, a.pos.z - target.z) < std::hypot(c.pos.x - target.x, c.pos.z - target.z);
    });
    for (const LevelData::Portal& p : near) {
        world.chunk(chunk_of(p.pos.x), chunk_of(p.pos.z));
        if ((link = portal_at(world, p.pos))) break;
        level.portals.erase(std::find(level.portals.begin(), level.portals.end(), p));
    }
    if (!link) {
        std::vector<ChunkPos> dirty;
        link = build_portal(world, target, dirty);
        remember_portal(*link);
    }
    const glm::dvec3 at = link->exit();
    player.x = at.x;
    player.y = at.y;
    player.z = at.z;
    // Face out of it, the way the portal is walked through.
    yaw = link->axis == 0 ? 0 : 90;
    fall_peak = player.y;
    renderer->load_all(player.x, player.z, 2);
    portal_cooldown = true;  // not straight back: step out of it first
    portal_ticks = 0;
    hud->show_message(to == Dimension::Nether ? "Entering the Nether" : "Leaving the Nether", 3);
}

void Game::change_dimension(Dimension to, std::optional<glm::dvec3> at) {
    close_gui();
    if (riding) dismount();
    get_out();
    mining_pos.reset();
    hud->set_mining(std::nullopt);
    // Everything here is saved with its chunks (save() writes the animals, monsters, items and
    // vehicles into them), then the world is swapped for the other dimension's.
    save(true);
    animals.clear();
    monsters.clear();
    drops.clear();
    arrows.clear();
    tnts.clear();
    falling.clear();
    vehicles.clear();
    fireballs.clear();
    puffs.clear();
    bolts.clear();
    spawners.clear();
    pressed_detectors.clear();
    target.reset();
    target_animal = nullptr;
    target_monster = nullptr;
    target_vehicle = nullptr;
    chest_vehicle = nullptr;
    burn_ticks = 0;
    renderer.reset();  // stops the worker threads before their world changes
    blocks.clear();
    world.reset(to);
    world.set_store(&saver->store(to));
    make_renderer();
    if (at) {
        player.x = at->x;
        player.y = at->y;
        player.z = at->z;
        renderer->load_all(player.x, player.z, 2);
    }
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
}

// --- Fireballs and zombie pigmen ---------------------------------------------------------------------

void Game::update_fireballs(double dt) {
    for (size_t i = 0; i < fireballs.size();) {
        Fireball& f = *fireballs[i];
        if (!world.find(chunk_of(ifloor(f.pos.x)), chunk_of(ifloor(f.pos.z))) || f.age > FIREBALL_LIFE) {
            fireballs.erase(fireballs.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        bool burst = f.tick(dt, world).has_value();
        // Straight into the player or a monster: 4 damage before the blast (Beta).
        auto inside = [&](const Body& b, double grow) {
            return f.pos.x > b.x - b.half - grow && f.pos.x < b.x + b.half + grow && f.pos.z > b.z - b.half - grow &&
                   f.pos.z < b.z + b.half + grow && f.pos.y > b.y - grow && f.pos.y < b.y + b.height + grow;
        };
        if (!burst && !creative && !vitals.dead() && inside(player, 0.5)) {
            hurt_player_by_monster(FIREBALL_HIT_DAMAGE, f.pos.x, f.pos.z);
            burst = true;
        }
        for (size_t k = 0; k < monsters.size() && !burst; ++k) {
            Monster& m = *monsters[k];
            if (m.alive() && &m != f.owner && inside(m.body, 0.3)) {
                m.hurt(FIREBALL_HIT_DAMAGE, f.pos.x, f.pos.z);
                burst = true;
            }
        }
        if (!burst) {
            ++i;
            continue;
        }
        const glm::dvec3 at = f.pos;
        fireballs.erase(fireballs.begin() + static_cast<std::ptrdiff_t>(i));
        explode(at, FIREBALL_POWER, true, true);
    }
}

bool Game::hit_fireball() {
    if (vitals.dead()) return false;
    const glm::vec3 e = eye(), dir = forward();
    const float block = target && !targeting_mob() ? target->distance : static_cast<float>(ATTACK_REACH);
    for (const auto& f : fireballs)
        if (auto d = f->ray_hit(e, dir, std::min(block, static_cast<float>(ATTACK_REACH)))) {
            f->deflect(glm::dvec3(dir));
            return true;
        }
    return false;
}

void Game::anger_pigmen(const Monster& hit) {
    for (const auto& m : monsters)
        if (m->kind == MobKind::PigZombie && m->alive() &&
            glm::length(glm::dvec3(m->body.x - hit.body.x, m->body.y - hit.body.y, m->body.z - hit.body.z)) < PIGMAN_ANGER_RANGE)
            m->enrage(monster_rng_);
}

void Game::evaporate(glm::ivec3 pos) {
    add_puffs(puffs, glm::dvec3(pos) + 0.5, 8, 0.4, 1.2, 0.25, rng);
}
