#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <unordered_map>

#include <GLFW/glfw3.h>

#include "textures.h"

namespace {
constexpr double PI = 3.14159265358979323846;
double radians(double d) { return d * PI / 180.0; }
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
ChunkPos chunk_at(double x, double z) { return {chunk_of(ifloor(x)), chunk_of(ifloor(z))}; }
}  // namespace

Game::Game(GLFWwindow* win, GLuint atlas, Painter& painter, const Options& opts, GameSetup setup)
    : window(win),
      options(opts),
      level(std::move(setup.level)),
      saver(std::make_unique<WorldSaver>(setup.world_dir)),
      world(level.seed),
      rng(setup.scripted ? 0 : std::random_device{}()),
      player(world, 0, 0, 0, 0.3, 1.8),
      scripted_(setup.scripted) {
    world.set_store(&saver->store());
    renderer = std::make_unique<WorldRenderer>(world, atlas, view_distance_chunks(options.view_distance), SKY);
    renderer->set_mesh_options({options.smooth_lighting, options.fancy_graphics});
    renderer->on_chunk_shown = [this](ChunkPos pos) { on_chunk_shown(pos); };
    hud = std::make_unique<Hud>(painter);

    for (ItemId item : creative_items()) creative_inventory.add(item, 1);

    spawn = level.spawn ? *level.spawn : find_spawn();
    level.spawn = spawn;
    const bool new_world = !level.player;
    if (level.player) {
        restore_player(*level.player);
    } else {
        player.x = spawn.x;
        player.y = spawn.y;
        player.z = spawn.z;
    }
    fall_peak = player.y;

    // Build the area around the player before the first frame; the rest streams in.
    renderer->load_all(player.x, player.z, 2);
    if (new_world) {
        for (int i = 0; i < 5; ++i) spawn_cow_near(player.x, player.z, 6, 20);
        hud->show_message("Welcome! Punch trees, find cows, survive.", 5);
    } else {
        hud->show_message("Welcome back to " + level.name, 3);
    }
    set_mouse_captured(!vitals.dead());
}

Game::~Game() {
    renderer.reset();  // stops the worker threads that read saved chunks
    hud.reset();
    if (saver) saver->wait();
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

void Game::restore_player(const PlayerData& p) {
    player.x = p.pos.x;
    player.y = p.pos.y;
    player.z = p.pos.z;
    yaw = p.yaw;
    pitch = p.pitch;
    vitals.health = p.health;
    vitals.hunger = p.hunger;
    vitals.saturation = p.saturation;
    vitals.exhaustion = p.exhaustion;
    creative = p.creative;
    flying = p.flying;
    inventory = p.inventory;
    if (vitals.dead()) on_death();
}

PlayerData Game::player_data() const {
    PlayerData p;
    p.pos = {player.x, player.y, player.z};
    p.yaw = yaw;
    p.pitch = pitch;
    p.health = vitals.health;
    p.hunger = vitals.hunger;
    p.saturation = vitals.saturation;
    p.exhaustion = vitals.exhaustion;
    p.creative = creative;
    p.flying = flying;
    p.inventory = inventory;
    return p;
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

void Game::on_chunk_shown(ChunkPos pos) {
    Chunk* chunk = world.find(pos.cx, pos.cz);
    if (!chunk) return;
    // Cows and items that were saved here, or left behind when the player walked away.
    if (!chunk->entities.empty()) {
        auto entities = std::move(chunk->entities);
        chunk->entities.clear();
        chunk->dirty = true;
        for (const EntityRecord& e : entities) spawn_entity(e);
    }
    if (chunk->mob_spawned) return;
    chunk->mob_spawned = true;
    chunk->dirty = true;
    if (cows.size() >= MAX_COWS || rng.random() > 0.12) return;
    double cx = pos.cx * CHUNK + 8, cz = pos.cz * CHUNK + 8;
    int n = rng.randint(1, 3);
    for (int i = 0; i < n; ++i) spawn_cow_near(cx, cz, 0, 6);
}

void Game::spawn_entity(const EntityRecord& e) {
    if (e.kind == EntityRecord::COW) {
        auto cow = std::make_unique<Cow>(world, e.x, e.y, e.z, rng);
        cow->yaw = cow->target_yaw = e.yaw;
        cow->health = std::clamp(e.health, 1, COW_MAX_HEALTH);
        cows.push_back(std::move(cow));
    } else {
        auto drop = std::make_unique<ItemDrop>(world, e.item, e.x, e.y, e.z, rng);
        drop->body.vx = drop->body.vy = drop->body.vz = 0;
        drop->age = e.age;
        drops.push_back(std::move(drop));
    }
}

void Game::stash_entity(const EntityRecord& e) {
    Chunk& chunk = world.chunk(chunk_of(ifloor(e.x)), chunk_of(ifloor(e.z)));
    chunk.entities.push_back(e);
    chunk.dirty = true;
}

namespace {
EntityRecord cow_record(const Cow& c) {
    EntityRecord e;
    e.kind = EntityRecord::COW;
    e.x = c.body.x;
    e.y = c.body.y;
    e.z = c.body.z;
    e.yaw = static_cast<float>(c.yaw);
    e.health = c.health;
    return e;
}
EntityRecord drop_record(const ItemDrop& d) {
    EntityRecord e;
    e.kind = EntityRecord::ITEM;
    e.x = d.body.x;
    e.y = d.body.y;
    e.z = d.body.z;
    e.item = d.item;
    e.age = static_cast<float>(d.age);
    return e;
}
}  // namespace

// --- saving --------------------------------------------------------------------------------

bool Game::save(bool wait) {
    // Cows and items are saved with the chunk they stand in.
    std::unordered_map<ChunkPos, std::vector<EntityRecord>, ChunkPosHash> live;
    for (const auto& cow : cows)
        if (cow->alive()) live[chunk_at(cow->body.x, cow->body.z)].push_back(cow_record(*cow));
    for (const auto& drop : drops) live[chunk_at(drop->body.x, drop->body.z)].push_back(drop_record(*drop));
    for (const auto& [pos, list] : live) world.chunk(pos.cx, pos.cz);

    std::vector<std::unique_ptr<Chunk>> copies;
    for (Chunk* c : world.all_chunks()) {
        auto it = live.find({c->cx, c->cz});
        bool has_live = it != live.end();
        if (!c->dirty && !c->saved_entities && !has_live) continue;
        auto copy = std::make_unique<Chunk>(*c);
        if (has_live) copy->entities.insert(copy->entities.end(), it->second.begin(), it->second.end());
        c->dirty = false;
        c->saved_entities = !copy->entities.empty();
        copies.push_back(std::move(copy));
    }
    LevelData data = level;
    data.last_played = now_ms();
    data.spawn = spawn;
    data.player = player_data();
    level.last_played = data.last_played;
    saver->save(std::move(data), std::move(copies));
    autosave_timer = 0;
    return wait ? saver->wait() : true;
}

// --- camera ------------------------------------------------------------------------------

glm::vec3 Game::eye() const {
    float h = EYE_HEIGHT - (sneaking ? SNEAK_EYE_DROP : 0.0f);
    return {static_cast<float>(player.x), static_cast<float>(player.y + h), static_cast<float>(player.z)};
}

glm::vec3 Game::forward() const {
    double y = radians(yaw), p = radians(pitch);
    return {static_cast<float>(std::sin(y) * std::cos(p)), static_cast<float>(-std::sin(p)),
            static_cast<float>(std::cos(y) * std::cos(p))};
}

glm::vec3 Game::camera() {
    glm::vec3 e = eye();
    if (!third_person) return e;
    glm::vec3 back = -forward();
    float dist = static_cast<float>(THIRD_PERSON_DISTANCE);
    // Pull the camera in front of any wall behind the player.
    if (auto hit = world.raycast(e, back, dist)) dist = std::max(0.2f, hit->distance - 0.25f);
    return e + back * dist;
}

// --- input ---------------------------------------------------------------------------------

bool Game::key_down(int key) const { return key > 0 && glfwGetKey(window, key) == GLFW_PRESS; }

void Game::set_mouse_captured(bool captured) {
    if (scripted_) return;  // never grab the mouse during the scripted test
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (captured && glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    have_cursor_ = false;  // the next cursor position starts fresh, so the view doesn't jump
}

void Game::set_paused(bool p) {
    paused = p;
    mouse_buttons_.clear();
    set_mouse_captured(!p && !vitals.dead());
}

void Game::apply_options() {
    renderer->render_distance = view_distance_chunks(options.view_distance);
    renderer->set_mesh_options({options.smooth_lighting, options.fancy_graphics});
}

void Game::on_key(int key, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_F1) hud_hidden = !hud_hidden;
    if (key == GLFW_KEY_F3) debug_screen = !debug_screen;
    if (key == GLFW_KEY_F5) third_person = !third_person;
    if (vitals.dead()) {
        if (key == GLFW_KEY_R) respawn();
        return;
    }
    if (paused) return;
    if (key == options.keys[KEY_FORWARD]) sprint_.forward_pressed(glfwGetTime(), sprint_allowed());
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        active_inventory().selected = key - GLFW_KEY_1;
        hud->show_item_name(active_inventory().selected_item());
    } else if (key == options.keys[KEY_CREATIVE]) {
        creative = !creative;
        flying = false;
        hud->show_message(creative ? "Creative mode" : "Survival mode");
    } else if (key == options.keys[KEY_CRAFT] && !creative) {
        bool ok = inventory.craft_planks();
        hud->show_message(ok ? "Crafted 4 planks" : "Need 1 oak log (and a free slot) to craft planks", 2);
    } else if (key == options.keys[KEY_DROP]) {
        drop_selected();
    } else if (key == options.keys[KEY_INVENTORY]) {
        hud->show_message("The inventory screen isn't built yet", 2);
    } else if (key == GLFW_KEY_H) {
        hud->show_help = !hud->show_help;
    } else if (key == options.keys[KEY_JUMP] && creative) {
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
    if (paused || vitals.dead() || scripted_) return;
    double k = options.degrees_per_pixel();
    yaw -= dx * k;
    pitch = std::clamp(pitch + dy * k * (options.invert_mouse ? -1 : 1), -89.9, 89.9);  // screen y grows downwards
}

void Game::on_mouse_button(int button, int action) {
    if (action == GLFW_RELEASE) {
        mouse_buttons_.erase(button);
        return;
    }
    if (vitals.dead() || paused) return;
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

// --- per frame -------------------------------------------------------------------------------

void Game::update(double dt) {
    dt = std::min(dt, 0.05);
    ++frame;
    if (dt > 0) fps += (1.0 / std::max(dt, 1e-4) - fps) * 0.05;
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
        ticks_ += dt * 20;
        level.time += static_cast<int64_t>(ticks_);
        ticks_ -= std::floor(ticks_);
        autosave_timer += dt;
        if (autosave_timer >= AUTOSAVE_INTERVAL) save(false);
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
    if (third_person && !dead) {
        PlayerPose pose{player.x, player.y, player.z, yaw, pitch, walk_phase, walk_amount, sneaking};
        for (auto& part : player_parts(pose)) parts.push_back(part);
    }
    renderer->set_instances(std::move(parts));
    renderer->set_outline(target && !target_cow ? std::optional<glm::ivec3>(target->block) : std::nullopt);
    hud->update(static_cast<float>(dt));
}

std::string Game::help_text() const {
    auto k = [&](KeyAction a) { return key_name(options.keys[a]); };
    return k(KEY_FORWARD) + k(KEY_LEFT) + k(KEY_BACK) + k(KEY_RIGHT) + " move   " + k(KEY_JUMP) + " jump/swim   " + k(KEY_SNEAK) +
           " sneak   double-tap " + k(KEY_FORWARD) + " sprint\n"
           "Left mouse: mine block / hit cow (hold to mine)\n"
           "Right mouse: place block / eat food\n"
           "1-9 or wheel: pick slot   " + k(KEY_DROP) + ": drop   " + k(KEY_CRAFT) + ": craft 1 log -> 4 planks\n" +
           k(KEY_CREATIVE) + ": creative cheat (double-tap " + k(KEY_JUMP) + " to fly)\n"
           "F1 hide HUD   F2 screenshot   F3 debug   F5 third person\n"
           "ESC: menu   H: hide this help";
}

std::string Game::debug_text() {
    char buf[512];
    int bx = ifloor(player.x), by = ifloor(player.y), bz = ifloor(player.z);
    // Beta's facing number: 0 south (+z), 1 west, 2 north, 3 east.
    int f = ifloor(-yaw / 90.0 + 0.5) & 3;
    static const char* FACING[4] = {"south", "west", "north", "east"};
    // Real light levels arrive with M5; until then only open sky counts.
    int sky = 15;
    for (int y = ifloor(eye().y); y < HEIGHT; ++y)
        if (is_opaque(world.get_block(bx, y, bz))) sky = 0;
    std::snprintf(buf, sizeof buf,
                  "PetrutzCraft M1 (%.0f fps, %d chunk updates)\n"
                  "x: %.3f (block %d)\ny: %.3f (feet, block %d)\nz: %.3f (block %d)\n"
                  "Chunk: %d %d (at %d %d in it)\n"
                  "Facing: %s (f: %d)\n"
                  "Sky light: %d\n"
                  "Seed: %lld\n"
                  "Chunks loaded: %zu   cows: %zu   items: %zu",
                  fps, renderer->jobs_in_flight(), player.x, bx, player.y, by, player.z, bz, chunk_of(bx), chunk_of(bz),
                  bx & 15, bz & 15, FACING[f], f, sky, static_cast<long long>(level.seed), world.chunk_count(),
                  cows.size(), drops.size());
    return buf;
}

void Game::draw(int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(SKY.r, SKY.g, SKY.b, SKY.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::vec3 cam = camera();
    renderer->draw(w, h, cam, forward());

    if (hud_hidden && !vitals.dead()) return;
    bool underwater = world.get_block(ifloor(cam.x), ifloor(cam.y), ifloor(cam.z)) == WATER;
    std::string info = debug_screen ? debug_text() : "PetrutzCraft M1";
    if (!debug_screen && creative) info += flying ? "\nCreative (flying)" : "\nCreative";
    hud->draw(w, h, {&active_inventory(), &vitals, creative, info, help_text(), underwater, vitals.dead()});
}

void Game::move(double dt) {
    Body& p = player;
    double yr = radians(yaw);
    double fx = std::sin(yr), fz = std::cos(yr);
    double rx = -std::cos(yr), rz = std::sin(yr);
    int f = action_down(KEY_FORWARD) - action_down(KEY_BACK);
    int s = action_down(KEY_RIGHT) - action_down(KEY_LEFT);
    double wx = fx * f + rx * s, wz = fz * f + rz * s;
    double length = std::hypot(wx, wz);
    if (length > 0) {
        wx /= length;
        wz /= length;
    }
    p.check_water();

    const bool down = action_down(KEY_SNEAK);
    sneaking = down && !flying;
    sprint_.update(f > 0, sprint_allowed());
    const bool sprint = sprint_.active;
    double speed = flying ? FLY_SPEED : sneaking ? SNEAK_SPEED : sprint ? SPRINT_SPEED : WALK_SPEED;
    if (p.in_water && !flying) speed *= 0.55;
    double accel = (p.on_ground || flying) ? 14 : 5;
    double k = std::min(1.0, accel * dt);
    p.vx += (wx * speed - p.vx) * k;
    p.vz += (wz * speed - p.vz) * k;

    bool jumped = false;
    if (flying) {
        p.vy = (action_down(KEY_JUMP) - down) * 8.0;
    } else {
        p.apply_gravity(dt);
        if (action_down(KEY_JUMP)) {
            if (p.in_water) {
                p.vy = std::min(p.vy + 30 * dt, 4.0);
            } else if (p.on_ground) {
                p.vy = JUMP_SPEED;
                jumped = true;
            }
        }
    }

    double ox = p.x, oz = p.z;
    double dx = p.vx * dt, dz = p.vz * dt;
    // Sneaking on the ground never walks off an edge.
    if (sneaking && p.on_ground && !jumped && !p.in_water) p.clamp_to_edges(dx, dz);
    HitAxes hit = p.move(dx, p.vy * dt, dz);
    if (hit.x || hit.z) sprint_.stop();  // running into a wall ends the sprint
    if (flying && p.on_ground && down) flying = false;

    double moved = std::hypot(p.x - ox, p.z - oz);
    walk_phase += moved * 2.4;
    walk_amount += (std::min(1.0, moved / std::max(dt, 1e-4) / WALK_SPEED) - walk_amount) * std::min(1.0, dt * 10);
    if (!creative) {
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

void Game::drop_selected() {
    Inventory& inv = active_inventory();
    auto item = inv.selected_item();
    if (!item) return;
    if (!creative) inv.take_selected();
    glm::vec3 e = eye(), f = forward();
    auto drop = std::make_unique<ItemDrop>(world, *item, e.x, e.y - 0.3, e.z, rng);
    drop->body.vx = f.x * 6;
    drop->body.vy = f.y * 6 + 1.5;
    drop->body.vz = f.z * 6;
    drop->pickup_delay = 2.0;  // so it isn't picked straight back up
    drops.push_back(std::move(drop));
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
    set_paused(false);
    hud->show_message("Respawned");
}

// --- cows and drops -------------------------------------------------------------------------------

void Game::update_cows(double dt) {
    // Past the distance where chunk meshes are dropped, so the chunk is shown (and spawns it) again on the way back.
    const double far_away = (renderer->render_distance + 3) * CHUNK;
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
            // Too far to simulate: keep it with its chunk until the player comes back.
            if (cow.alive()) stash_entity(cow_record(cow));
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
    // Past the distance where chunk meshes are dropped, so the chunk is shown (and spawns it) again on the way back.
    const double far_away = (renderer->render_distance + 3) * CHUNK;
    for (auto it = drops.begin(); it != drops.end();) {
        ItemDrop& drop = **it;
        Body& b = drop.body;
        if (!world.find(chunk_of(ifloor(b.x)), chunk_of(ifloor(b.z)))) {
            ++it;
            continue;
        }
        if (std::hypot(b.x - player.x, b.z - player.z) > far_away) {
            stash_entity(drop_record(drop));
            it = drops.erase(it);
            continue;
        }
        drop.tick(dt);
        double dx = b.x - player.x, dz = b.z - player.z;
        double horiz = std::hypot(dx, dz);
        bool vert_ok = player.y - 1.2 < b.y && b.y < player.y + 2.2;
        if (drop.age > drop.pickup_delay && !vitals.dead() && vert_ok && horiz < 2.5) {
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
