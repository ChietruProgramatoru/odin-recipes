#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include <GLFW/glfw3.h>

#include "autotest.h"
#include "textures.h"

namespace {
constexpr double PI = 3.14159265358979323846;
double radians(double d) { return d * PI / 180.0; }
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
}  // namespace

Game::Game(GLFWwindow* win, uint64_t seed_, int render_distance, AutoTest* autotest)
    : window(win),
      seed(seed_),
      world(seed_),
      rng(autotest ? 0 : std::random_device{}()),
      player(world, 0, 0, 0, 0.3, 1.8),
      autotest_(autotest) {
    atlas = upload_texture(make_atlas(make_tiles()));
    renderer = std::make_unique<WorldRenderer>(world, atlas, render_distance, SKY);
    renderer->on_chunk_shown = [this](ChunkPos pos) { maybe_spawn_cows(pos); };
    hud = std::make_unique<Hud>(atlas);

    for (ItemId item : creative_items()) creative_inventory.add(item, 1);

    spawn = find_spawn();
    player.x = spawn.x;
    player.y = spawn.y;
    player.z = spawn.z;
    fall_peak = player.y;

    // Build the area around spawn before the first frame; the rest streams in.
    renderer->load_all(player.x, player.z, 2);
    for (int i = 0; i < 5; ++i) spawn_cow_near(player.x, player.z, 6, 20);

    set_mouse_captured(true);
    hud->show_message("Welcome! Punch trees, find cows, survive.", 5);
}

Game::~Game() {
    renderer.reset();
    hud.reset();
    glDeleteTextures(1, &atlas);
}

// --- world setup -------------------------------------------------------------------------

glm::dvec3 Game::find_spawn() {
    for (int r = 0; r < 200; r += 4) {
        const int pts[6][2] = {{r, 0}, {0, r}, {-r, 0}, {0, -r}, {r, r}, {-r, -r}};
        for (const auto& p : pts) {
            int y = world.surface_y(p[0], p[1]);
            if (y > SEA_LEVEL + 1 && world.get_block(p[0], y - 1, p[1]) == GRASS) return {p[0] + 0.5, y, p[1] + 0.5};
        }
    }
    return {0.5, world.surface_y(0, 0), 0.5};
}

bool Game::spawn_cow_near(double x, double z, double rmin, double rmax) {
    for (int i = 0; i < 10; ++i) {
        double a = rng.uniform(0, 2 * PI);
        double r = rng.uniform(rmin, rmax);
        int cx = ifloor(x + std::cos(a) * r), cz = ifloor(z + std::sin(a) * r);
        int y = world.surface_y(cx, cz);
        if (world.get_block(cx, y - 1, cz) == GRASS && !world.is_solid_at(cx, y + 1, cz)) {
            cows.push_back(std::make_unique<Cow>(world, cx + 0.5, y, cz + 0.5, rng));
            return true;
        }
    }
    return false;
}

void Game::maybe_spawn_cows(ChunkPos pos) {
    Chunk* chunk = world.find(pos.cx, pos.cz);
    if (!chunk || chunk->mob_spawned) return;
    chunk->mob_spawned = true;
    if (cows.size() >= MAX_COWS || rng.random() > 0.12) return;
    double cx = pos.cx * CHUNK + 8, cz = pos.cz * CHUNK + 8;
    int n = rng.randint(1, 3);
    for (int i = 0; i < n; ++i) spawn_cow_near(cx, cz, 0, 6);
}

// --- camera ------------------------------------------------------------------------------

glm::vec3 Game::eye() const {
    return {static_cast<float>(player.x), static_cast<float>(player.y + EYE_HEIGHT), static_cast<float>(player.z)};
}

glm::vec3 Game::forward() const {
    double y = radians(yaw), p = radians(pitch);
    return {static_cast<float>(std::sin(y) * std::cos(p)), static_cast<float>(-std::sin(p)),
            static_cast<float>(std::cos(y) * std::cos(p))};
}

// --- input ---------------------------------------------------------------------------------

bool Game::key_down(int key) const { return glfwGetKey(window, key) == GLFW_PRESS; }

void Game::set_mouse_captured(bool captured) {
    if (autotest_) return;  // never grab the mouse during the scripted test
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (captured && glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    have_cursor_ = false;  // the next cursor position starts fresh, so the view doesn't jump
}

void Game::set_paused(bool p) {
    paused = p;
    mouse_buttons_.clear();
    set_mouse_captured(!p);
}

void Game::on_key(int key, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (vitals.dead()) {
        if (key == GLFW_KEY_R) respawn();
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        set_paused(!paused);
        return;
    }
    if (paused) return;
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        active_inventory().selected = key - GLFW_KEY_1;
        hud->show_item_name(active_inventory().selected_item());
    } else if (key == GLFW_KEY_C) {
        creative = !creative;
        flying = false;
        hud->show_message(creative ? "Creative mode" : "Survival mode");
    } else if (key == GLFW_KEY_F && !creative) {
        bool ok = inventory.craft_planks();
        hud->show_message(ok ? "Crafted 4 planks" : "Need 1 oak log (and a free slot) to craft planks", 2);
    } else if (key == GLFW_KEY_H) {
        hud->show_help = !hud->show_help;
    } else if (key == GLFW_KEY_SPACE && creative) {
        double now = glfwGetTime();
        if (now - last_space_ < 0.3) {
            flying = !flying;
            player.vy = 0;
        }
        last_space_ = now;
    }
}

void Game::on_cursor(double x, double y) {
    double dx = have_cursor_ ? x - cursor_x_ : 0, dy = have_cursor_ ? y - cursor_y_ : 0;
    cursor_x_ = x;
    cursor_y_ = y;
    have_cursor_ = true;
    if (paused || vitals.dead() || autotest_) return;
    yaw -= dx * MOUSE_SENSITIVITY;
    pitch = std::clamp(pitch + dy * MOUSE_SENSITIVITY, -89.9, 89.9);  // screen y grows downwards
}

void Game::on_mouse_button(int button, int action) {
    if (action == GLFW_RELEASE) {
        mouse_buttons_.erase(button);
        return;
    }
    if (vitals.dead()) return;
    if (paused) {
        set_paused(false);
        return;
    }
    mouse_buttons_.insert(button);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        break_cooldown = 0.0;
        update_target();
        try_attack();
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        update_target();
        use_item();
    }
}

void Game::on_scroll(double dy) {
    if (paused || vitals.dead() || dy == 0) return;
    Inventory& inv = active_inventory();
    inv.selected = (inv.selected + (dy > 0 ? -1 : 1) + Inventory::SIZE) % Inventory::SIZE;
    hud->show_item_name(inv.selected_item());
}

void Game::on_focus(bool focused) {
    if (!focused && !vitals.dead() && !autotest_) set_paused(true);
}

// --- per frame -------------------------------------------------------------------------------

void Game::run() {
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window) && !quit) {
        glfwPollEvents();
        double now = glfwGetTime();
        double dt = autotest_ ? 1.0 / 60 : now - last;
        last = now;
        if (now > 0 && dt > 0) fps_ += (1.0 / std::max(dt, 1e-4) - fps_) * 0.05;
        update(dt);
        if (quit) break;
        draw();
        glfwSwapBuffers(window);
    }
}

void Game::update(double dt) {
    dt = std::min(dt, 0.05);
    ++frame;
    const bool dead = vitals.dead();

    if (!paused && !dead) {
        move(dt);
        update_target();
        mine(dt);
        if (mouse_down(GLFW_MOUSE_BUTTON_RIGHT) && use_cooldown <= 0) use_item();
        use_cooldown -= dt;
        break_cooldown -= dt;
        attack_cooldown -= dt;
        if (!creative && vitals.tick(dt)) hud->flash_damage();
    } else {
        target.reset();
        hud->set_mining(std::nullopt);
    }

    if (!paused) {
        update_cows(dt);
        update_drops(dt);
    }
    renderer->update(player.x, player.z);

    const double view = renderer->render_distance * CHUNK;
    std::vector<ModelPart> parts;
    auto in_view = [&](const Body& b) { return std::abs(b.x - player.x) < view && std::abs(b.z - player.z) < view; };
    for (const auto& cow : cows)
        if (in_view(cow->body))
            for (auto& part : cow->parts()) parts.push_back(part);
    for (const auto& drop : drops)
        if (in_view(drop->body))
            for (auto& part : drop->parts()) parts.push_back(part);
    renderer->set_instances(std::move(parts));
    renderer->set_outline(target && !target_cow ? std::optional<glm::ivec3>(target->block) : std::nullopt);
    hud->update(static_cast<float>(dt));

    if (autotest_) (*autotest_)(*this);
}

void Game::draw() {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(SKY.r, SKY.g, SKY.b, SKY.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer->draw(w, h, eye(), forward());

    glm::vec3 e = eye();
    bool underwater = world.get_block(ifloor(e.x), ifloor(e.y), ifloor(e.z)) == WATER;
    char info[160];
    const char* mode = creative ? (flying ? "Creative (flying)" : "Creative") : "Survival";
    std::snprintf(info, sizeof info, "%s   XYZ %.1f %.1f %.1f   cows %zu   FPS %.0f", mode, player.x, player.y,
                  player.z, cows.size(), fps_);
    hud->draw(w, h, {&active_inventory(), &vitals, creative, info, underwater, vitals.dead(), paused});
}

void Game::move(double dt) {
    Body& p = player;
    double yr = radians(yaw);
    double fx = std::sin(yr), fz = std::cos(yr);
    double rx = -std::cos(yr), rz = std::sin(yr);
    int f = key_down(GLFW_KEY_W) - key_down(GLFW_KEY_S);
    int s = key_down(GLFW_KEY_D) - key_down(GLFW_KEY_A);
    double wx = fx * f + rx * s, wz = fz * f + rz * s;
    double length = std::hypot(wx, wz);
    if (length > 0) {
        wx /= length;
        wz /= length;
    }
    p.check_water();

    bool shift = key_down(GLFW_KEY_LEFT_SHIFT) || key_down(GLFW_KEY_RIGHT_SHIFT);
    bool sprint = shift && key_down(GLFW_KEY_W) && !flying && (creative || vitals.can_sprint());
    double speed = flying ? FLY_SPEED : (sprint ? SPRINT_SPEED : WALK_SPEED);
    if (p.in_water && !flying) speed *= 0.55;
    double accel = (p.on_ground || flying) ? 14 : 5;
    double k = std::min(1.0, accel * dt);
    p.vx += (wx * speed - p.vx) * k;
    p.vz += (wz * speed - p.vz) * k;

    bool jumped = false;
    if (flying) {
        p.vy = (key_down(GLFW_KEY_SPACE) - shift) * 8.0;
    } else {
        p.apply_gravity(dt);
        if (key_down(GLFW_KEY_SPACE)) {
            if (p.in_water) {
                p.vy = std::min(p.vy + 30 * dt, 4.0);
            } else if (p.on_ground) {
                p.vy = JUMP_SPEED;
                jumped = true;
            }
        }
    }

    double ox = p.x, oz = p.z;
    p.move(p.vx * dt, p.vy * dt, p.vz * dt);
    if (flying && p.on_ground && shift) flying = false;

    if (!creative) {
        double moved = std::hypot(p.x - ox, p.z - oz);
        vitals.exhaust(moved * (sprint ? EXHAUST_SPRINT : EXHAUST_WALK));
        if (jumped) vitals.exhaust(sprint ? EXHAUST_SPRINT_JUMP : EXHAUST_JUMP);
    }

    // Fall damage
    if (p.in_water || flying || creative) {
        fall_peak = p.y;
    } else if (!p.on_ground) {
        fall_peak = std::max(fall_peak, p.y);
    } else {
        if (!was_on_ground) {
            int dmg = fall_damage(fall_peak - p.y);
            if (dmg && vitals.damage(dmg, true)) {
                hud->flash_damage();
                if (vitals.dead()) on_death();
            }
        }
        fall_peak = p.y;
    }
    was_on_ground = p.on_ground;
}

void Game::update_target() {
    glm::vec3 origin = eye(), direction = forward();
    target = world.raycast(origin, direction, REACH);
    Cow* best = nullptr;
    float best_d = ATTACK_REACH;
    for (const auto& cow : cows) {
        if (!cow->alive()) continue;
        auto d = cow->ray_hit(origin, direction, ATTACK_REACH);
        if (d && *d < best_d) {
            best = cow.get();
            best_d = *d;
        }
    }
    if (best && target && target->distance < best_d) best = nullptr;
    target_cow = best;
}

// --- actions ------------------------------------------------------------------------------------

void Game::try_attack() {
    if (!target_cow || attack_cooldown > 0) return;
    attack_cooldown = 0.35;
    target_cow->hurt(creative ? 10 : 2, player.x, player.z);
    if (!creative) vitals.exhaust(EXHAUST_ATTACK);
}

void Game::mine(double dt) {
    if (!mouse_down(GLFW_MOUSE_BUTTON_LEFT) || !target || target_cow) {
        mining_pos.reset();
        mining_progress = 0;
        hud->set_mining(std::nullopt);
        return;
    }
    glm::ivec3 pos = target->block;
    BlockId blk = world.get_block(pos.x, pos.y, pos.z);
    const BlockInfo* info = block_info(blk);
    if (!info || (!info->hardness && !creative) || blk == BEDROCK) {
        hud->set_mining(std::nullopt);
        return;
    }
    if (pos != mining_pos) {
        mining_pos = pos;
        mining_progress = 0;
    }
    if (creative) {
        hud->set_mining(std::nullopt);
        if (break_cooldown <= 0) {
            break_block(pos, blk);
            break_cooldown = 0.25;
        }
        return;
    }
    mining_progress += dt / std::max(0.05, static_cast<double>(*info->hardness));
    hud->set_mining(static_cast<float>(mining_progress));
    if (mining_progress >= 1.0) {
        break_block(pos, blk);
        mining_pos.reset();
        mining_progress = 0;
    }
}

void Game::break_block(glm::ivec3 pos, BlockId blk) {
    renderer->rebuild(world.set_block(pos.x, pos.y, pos.z, AIR));
    if (creative) return;
    vitals.exhaust(EXHAUST_BREAK);
    std::optional<ItemId> drop = block_info(blk) ? block_info(blk)->drop : std::nullopt;
    if (blk == LEAVES && rng.random() < 0.1) drop = APPLE;
    if (drop) spawn_drop(*drop, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
}

void Game::spawn_drop(ItemId item, double x, double y, double z) {
    drops.push_back(std::make_unique<ItemDrop>(world, item, x, y, z, rng));
}

void Game::use_item() {
    use_cooldown = 0.25;
    Inventory& inv = active_inventory();
    auto item = inv.selected_item();
    if (!item) return;
    if (food_info(*item)) {
        if (!creative && vitals.eat(*item)) {
            inv.take_selected();
            hud->show_message("Yum!", 1);
        }
        return;
    }
    if (!is_block(*item) || !target || target_cow || !target->before) return;
    glm::ivec3 c = *target->before;
    std::vector<const Body*> bodies{&player};
    for (const auto& cow : cows) bodies.push_back(&cow->body);
    for (const Body* b : bodies)
        if (c.x + 1 > b->x - b->half && c.x < b->x + b->half && c.z + 1 > b->z - b->half && c.z < b->z + b->half &&
            c.y + 1 > b->y && c.y < b->y + b->height)
            return;  // don't build inside someone
    BlockId here = world.get_block(c.x, c.y, c.z);
    if (here != AIR && here != WATER) return;
    renderer->rebuild(world.set_block(c.x, c.y, c.z, static_cast<BlockId>(*item)));
    if (!creative) inv.take_selected();
}

void Game::on_death() {
    hud->set_mining(std::nullopt);
    mouse_buttons_.clear();
    set_mouse_captured(false);
}

void Game::respawn() {
    vitals.reset();
    player.x = spawn.x;
    player.y = spawn.y;
    player.z = spawn.z;
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
    paused = false;
    set_mouse_captured(true);
    hud->show_message("Respawned");
}

// --- cows and drops -------------------------------------------------------------------------------

void Game::update_cows(double dt) {
    const double far_away = (renderer->render_distance + 1) * CHUNK;
    for (auto it = cows.begin(); it != cows.end();) {
        Cow& cow = **it;
        // Skip cows on chunks that aren't generated yet so they don't fall through the world.
        if (!world.find(chunk_of(ifloor(cow.body.x)), chunk_of(ifloor(cow.body.z)))) {
            ++it;
            continue;
        }
        bool remove = false;
        if (cow.tick(dt, player.x, player.z, rng)) {
            int n = drop_beef_count(rng);
            for (int i = 0; i < n; ++i) spawn_drop(BEEF, cow.body.x, cow.body.y + 0.5, cow.body.z);
            remove = true;
        } else if (std::hypot(cow.body.x - player.x, cow.body.z - player.z) > far_away) {
            remove = true;
        }
        if (remove) {
            if (target_cow == &cow) target_cow = nullptr;
            it = cows.erase(it);
        } else {
            ++it;
        }
    }
    if (cows.size() < 6 && rng.random() < dt * 0.05) spawn_cow_near(player.x, player.z, 24, 48);
}

void Game::update_drops(double dt) {
    for (auto it = drops.begin(); it != drops.end();) {
        ItemDrop& drop = **it;
        drop.tick(dt);
        Body& b = drop.body;
        double dx = b.x - player.x, dz = b.z - player.z;
        double horiz = std::hypot(dx, dz);
        bool vert_ok = player.y - 1.2 < b.y && b.y < player.y + 2.2;
        if (drop.age > 0.5 && !vitals.dead() && vert_ok && horiz < 2.5) {
            if (horiz < 1.2 && inventory.add(drop.item, 1) == 0) {
                it = drops.erase(it);
                continue;
            }
            // Pull nearby items towards the player.
            double pull = 6.0 * dt / std::max(horiz, 0.3);
            b.x -= dx * std::min(1.0, pull);
            b.z -= dz * std::min(1.0, pull);
        }
        if (drop.age > 300)
            it = drops.erase(it);
        else
            ++it;
    }
}
