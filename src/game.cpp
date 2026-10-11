#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <unordered_map>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "bed.h"
#include "textures.h"
#include "tools.h"
#include "torch.h"

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
      painter_(painter),
      scripted_(setup.scripted),
      weather_rng_(setup.scripted ? 1 : std::random_device{}()),
      animal_rng_(setup.scripted ? 4 : std::random_device{}()),
      monster_rng_(setup.scripted ? 2 : std::random_device{}()),
      farm_rng_(setup.scripted ? 3 : std::random_device{}()) {
    weather = Weather(level.weather);
    world.set_store(&saver->store());
    renderer = std::make_unique<WorldRenderer>(world, atlas, view_distance_chunks(options.view_distance));
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
        // A herd of cows, then a few groups of whatever lives here.
        spawn_animal_group(player.x, player.z, 6, 20, AnimalKind::Cow);
        for (int i = 0; i < 4; ++i) spawn_animal_group(player.x, player.z, 6, 24);
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
    bed_spawn = p.bed;
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
    p.bed = bed_spawn;
    return p;
}

Animal* Game::spawn_animal(AnimalKind kind, double x, double y, double z) {
    animals.push_back(std::make_unique<Animal>(world, kind, x, y, z, animal_rng_));
    return animals.back().get();
}

int Game::spawn_animal_group(double x, double z, double rmin, double rmax, std::optional<AnimalKind> kind) {
    // Grass under open sky (Beta: light 9 or more) with room for an animal above it.
    auto spot = [&](int cx, int cz) -> std::optional<int> {
        const int y = world.surface_y(cx, cz);
        if (world.get_block(cx, y - 1, cz) != GRASS || world.is_solid_at(cx, y, cz) || world.is_solid_at(cx, y + 1, cz) ||
            world.sky_light(cx, y, cz) < 9)
            return std::nullopt;
        return y;
    };
    for (int i = 0; i < 10; ++i) {
        const double a = animal_rng_.uniform(0, 2 * PI), r = animal_rng_.uniform(rmin, rmax);
        const int cx = ifloor(x + std::cos(a) * r), cz = ifloor(z + std::sin(a) * r);
        if (!spot(cx, cz)) continue;
        const std::optional<AnimalKind> k = kind ? kind : pick_land_animal(world.biome_at(cx, cz).value_or(Biome::PLAINS), animal_rng_);
        if (!k) return 0;
        int made = 0;
        for (int n = animal_rng_.randint(1, 4), tries = 0; made < n && tries < 12; ++tries) {
            const int gx = cx + (tries ? animal_rng_.randint(-3, 3) : 0), gz = cz + (tries ? animal_rng_.randint(-3, 3) : 0);
            if (auto y = spot(gx, gz)) {
                spawn_animal(*k, gx + 0.5, *y, gz + 0.5);
                ++made;
            }
        }
        return made;
    }
    return 0;
}

int Game::spawn_squid(double x, double z, double rmin, double rmax) {
    for (int i = 0; i < 10; ++i) {
        const double a = animal_rng_.uniform(0, 2 * PI), r = animal_rng_.uniform(rmin, rmax);
        const int cx = ifloor(x + std::cos(a) * r), cz = ifloor(z + std::sin(a) * r);
        // Water at least 4 deep under the surface of the sea (Beta: squid swim between 46 and 62).
        const int y = SEA_LEVEL - 4;
        bool deep = true;
        for (int dy = 0; dy < 4 && deep; ++dy) deep = world.get_block(cx, y + dy, cz) == WATER;
        if (!deep) continue;
        const int n = animal_rng_.randint(1, 4);
        for (int k = 0; k < n; ++k) spawn_animal(AnimalKind::Squid, cx + animal_rng_.uniform(0.3, 0.7), y + 0.5, cz + animal_rng_.uniform(0.3, 0.7));
        return n;
    }
    return 0;
}

size_t Game::land_animal_count() const {
    return static_cast<size_t>(std::count_if(animals.begin(), animals.end(), [](const auto& a) { return a->kind != AnimalKind::Squid; }));
}

void Game::on_chunk_shown(ChunkPos pos) {
    Chunk* chunk = world.find(pos.cx, pos.cz);
    if (!chunk) return;
    // Fires and flowing water or lava that were busy when the world was saved carry on.
    blocks.wake_chunk(*chunk);
    // Mob spawners (dungeons) start working.
    for (int i = 0; i < static_cast<int>(chunk->blocks.size()); ++i) {
        if (chunk->blocks[i] != SPAWNER) continue;
        glm::ivec3 at(chunk->cx * CHUNK + (i & 15), i >> 8, chunk->cz * CHUNK + ((i >> 4) & 15));
        if (std::none_of(spawners.begin(), spawners.end(), [&](const Spawner& s) { return s.pos == at; }))
            spawners.push_back({at});
    }
    // Animals, monsters and items that were saved here, or left behind when the player walked away.
    if (!chunk->entities.empty()) {
        auto entities = std::move(chunk->entities);
        chunk->entities.clear();
        chunk->dirty = true;
        for (const EntityRecord& e : entities) spawn_entity(e);
    }
    if (chunk->mob_spawned) return;
    chunk->mob_spawned = true;
    chunk->dirty = true;
    // Animals mostly appear when the world is made (Beta): a group in about one chunk in 8.
    const double cx = pos.cx * CHUNK + 8, cz = pos.cz * CHUNK + 8;
    if (land_animal_count() < MAX_ANIMALS && animal_rng_.random() < 0.12) spawn_animal_group(cx, cz, 0, 7);
    if (animals.size() - land_animal_count() < MAX_SQUID && animal_rng_.random() < 0.1) spawn_squid(cx, cz, 0, 8);
}

void Game::spawn_entity(const EntityRecord& e) {
    if (e.kind == EntityRecord::MONSTER) {
        const auto kind = static_cast<MobKind>(e.mob);
        auto m = std::make_unique<Monster>(world, kind, e.x, e.y, e.z, monster_rng_, e.size);
        m->yaw = m->target_yaw = e.yaw;
        m->health = std::clamp(e.health, 1, m->info().health);
        m->powered = e.flags & EntityRecord::POWERED;
        Monster* spider = m.get();
        monsters.push_back(std::move(m));
        if (e.flags & EntityRecord::JOCKEY) {
            auto rider = std::make_unique<Monster>(world, MobKind::Skeleton, e.x, e.y, e.z, monster_rng_);
            rider->mount = spider;
            spider->rider = rider.get();
            monsters.push_back(std::move(rider));
        }
    } else if (e.kind == EntityRecord::FALLING) {
        auto f = std::make_unique<FallingBlock>(world, static_cast<BlockId>(e.item), glm::ivec3(0));
        f->body.x = e.x;
        f->body.y = e.y;
        f->body.z = e.z;
        falling.push_back(std::move(f));
    } else if (e.kind == EntityRecord::ANIMAL) {
        Animal& a = *spawn_animal(static_cast<AnimalKind>(e.mob), e.x, e.y, e.z);
        a.yaw = a.target_yaw = e.yaw;
        a.wool = e.size;
        a.sheared = e.flags & EntityRecord::SHEARED;
        a.saddled = e.flags & EntityRecord::SADDLED;
        a.tamed = e.flags & EntityRecord::TAMED;
        a.sitting = e.flags & EntityRecord::SITTING;
        a.angry = e.flags & EntityRecord::ANGRY;
        if (a.kind == AnimalKind::Chicken && e.age > 0) a.egg_timer = e.age;
        a.health = std::clamp(e.health, 1, a.max_health());
    } else {
        auto drop = std::make_unique<ItemDrop>(world, Stack{e.item, e.count, e.damage}, e.x, e.y, e.z, rng);
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
EntityRecord animal_record(const Animal& a) {
    EntityRecord e;
    e.kind = EntityRecord::ANIMAL;
    e.x = a.body.x;
    e.y = a.body.y;
    e.z = a.body.z;
    e.yaw = static_cast<float>(a.yaw);
    e.health = a.health;
    e.mob = static_cast<int>(a.kind);
    e.size = a.wool;
    e.flags = (a.sheared ? EntityRecord::SHEARED : 0) | (a.saddled ? EntityRecord::SADDLED : 0) |
              (a.tamed ? EntityRecord::TAMED : 0) | (a.sitting ? EntityRecord::SITTING : 0) | (a.angry ? EntityRecord::ANGRY : 0);
    e.age = a.kind == AnimalKind::Chicken ? static_cast<float>(a.egg_timer) : 0.0f;
    return e;
}
// A monster for its chunk. A skeleton riding a spider is saved as part of the spider.
EntityRecord monster_record(const Monster& m) {
    EntityRecord e;
    e.kind = EntityRecord::MONSTER;
    e.x = m.body.x;
    e.y = m.body.y;
    e.z = m.body.z;
    e.yaw = static_cast<float>(m.yaw);
    e.health = m.health;
    e.mob = static_cast<int>(m.kind);
    e.size = m.size;
    e.flags = (m.powered ? EntityRecord::POWERED : 0) | (m.rider ? EntityRecord::JOCKEY : 0);
    return e;
}
EntityRecord falling_record(const FallingBlock& f) {
    EntityRecord e;
    e.kind = EntityRecord::FALLING;
    e.x = f.body.x;
    e.y = f.body.y;
    e.z = f.body.z;
    e.item = f.block;
    return e;
}
EntityRecord drop_record(const ItemDrop& d) {
    EntityRecord e;
    e.kind = EntityRecord::ITEM;
    e.x = d.body.x;
    e.y = d.body.y;
    e.z = d.body.z;
    e.item = d.stack.item;
    e.count = d.stack.count;
    e.damage = d.stack.damage;
    e.age = static_cast<float>(d.age);
    return e;
}
}  // namespace

// --- saving --------------------------------------------------------------------------------

bool Game::save(bool wait) {
    // Animals, monsters and items are saved with the chunk they stand in.
    std::unordered_map<ChunkPos, std::vector<EntityRecord>, ChunkPosHash> live;
    for (const auto& a : animals)
        if (a->alive()) live[chunk_at(a->body.x, a->body.z)].push_back(animal_record(*a));
    for (const auto& m : monsters)
        if (m->alive() && !m->mount && !m->vanish) live[chunk_at(m->body.x, m->body.z)].push_back(monster_record(*m));
    for (const auto& drop : drops) live[chunk_at(drop->body.x, drop->body.z)].push_back(drop_record(*drop));
    for (const auto& f : falling) live[chunk_at(f->body.x, f->body.z)].push_back(falling_record(*f));
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
    data.weather = weather.state;
    data.player = player_data();
    level.last_played = data.last_played;
    saver->save(std::move(data), std::move(copies));
    autosave_timer = 0;
    return wait ? saver->wait() : true;
}

// --- camera ------------------------------------------------------------------------------

glm::vec3 Game::eye() const {
    if (sleeping_in) {
        // Lying with the head on the pillow.
        glm::vec3 d(FACING_DIRS[sleep_dir]);
        return glm::vec3(*sleeping_in) + glm::vec3(0.5f, 0.75f, 0.5f) + d * 0.25f;
    }
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
    set_mouse_captured(!p && !vitals.dead() && !gui);
}

void Game::open_gui(InventoryScreen::Kind kind) {
    show_gui(std::make_unique<InventoryScreen>(kind, kind == InventoryScreen::Kind::Creative ? creative_inventory : active_inventory()));
}

void Game::show_gui(std::unique_ptr<InventoryScreen> screen) {
    if (gui) close_gui();
    gui = std::move(screen);
    mouse_buttons_.clear();
    mining_pos.reset();
    hud->set_mining(std::nullopt);
    set_mouse_captured(false);
    if (!scripted_) {
        // Start with the mouse in the middle of the window, like Beta.
        int ww, wh;
        glfwGetWindowSize(window, &ww, &wh);
        glfwSetCursorPos(window, ww / 2.0, wh / 2.0);
        cursor_x_ = ww / 2.0;
        cursor_y_ = wh / 2.0;
    }
}

void Game::open_furnace(glm::ivec3 pos) {
    if (!is_furnace(world.get_block(pos.x, pos.y, pos.z))) return;
    show_gui(std::make_unique<InventoryScreen>(active_inventory(), world.furnace(pos)));
    furnace_open = pos;
}

void Game::close_gui() {
    if (!gui) return;
    for (const Stack& s : gui->close()) throw_stack(s);
    gui.reset();
    furnace_open.reset();
    set_mouse_captured(!paused && !vitals.dead());
}

void Game::gui_mouse(double& x, double& y) const {
    int ww, wh, fw, fh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fw, &fh);
    x = cursor_x_ * fw / std::max(1, ww);
    y = cursor_y_ * fh / std::max(1, wh);
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
    if (paused || sleeping_in) return;  // asleep: nothing to do but wait for morning
    if (gui) {
        if (key == options.keys[KEY_INVENTORY]) {
            close_gui();
        } else if (key == options.keys[KEY_DROP]) {
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            double mx, my;
            gui_mouse(mx, my);
            if (auto s = gui->drop_hovered(w, h, mx, my)) throw_stack(*s);
        }
        return;
    }
    if (key == options.keys[KEY_FORWARD]) sprint_.forward_pressed(glfwGetTime(), sprint_allowed());
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        active_inventory().selected = key - GLFW_KEY_1;
        hud->show_item_name(active_inventory().selected_item());
    } else if (key == options.keys[KEY_CREATIVE]) {
        creative = !creative;
        flying = false;
        hud->show_message(creative ? "Creative mode" : "Survival mode");
    } else if (key == options.keys[KEY_DROP]) {
        drop_selected();
    } else if (key == options.keys[KEY_INVENTORY]) {
        open_gui(creative ? InventoryScreen::Kind::Creative : InventoryScreen::Kind::Inventory);
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
    if (paused || vitals.dead() || scripted_ || gui || sleeping_in) return;
    double k = options.degrees_per_pixel();
    yaw -= dx * k;
    pitch = std::clamp(pitch + dy * k * (options.invert_mouse ? -1 : 1), -89.9, 89.9);  // screen y grows downwards
}

void Game::on_mouse_button(int button, int action) {
    if (action == GLFW_RELEASE) {
        mouse_buttons_.erase(button);
        return;
    }
    if (vitals.dead() || paused || sleeping_in) return;
    if (gui) {
        if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT) return;
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        double mx, my;
        gui_mouse(mx, my);
        bool shift = key_down(GLFW_KEY_LEFT_SHIFT) || key_down(GLFW_KEY_RIGHT_SHIFT);
        for (const Stack& s : gui->click(w, h, mx, my, button == GLFW_MOUSE_BUTTON_LEFT ? 0 : 1, shift)) throw_stack(s);
        return;
    }
    mouse_buttons_.insert(button);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        break_cooldown = 0.0;
        update_target();
        try_attack();
        hand.swing();  // Beta swings at anything, even thin air
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        update_target();
        use_item_by_hand();
    }
}

void Game::on_scroll(double dy) {
    if (paused || vitals.dead() || dy == 0) return;
    if (gui) {
        gui->scroll(dy);
        return;
    }
    Inventory& inv = active_inventory();
    inv.selected = (inv.selected + (dy > 0 ? -1 : 1) + Inventory::HOTBAR) % Inventory::HOTBAR;
    hud->show_item_name(inv.selected_item());
}

// --- per frame -------------------------------------------------------------------------------

void Game::update(double dt) {
    dt = std::min(dt, 0.05);
    ++frame;
    if (dt > 0) fps += (1.0 / std::max(dt, 1e-4) - fps) * 0.05;
    const bool dead = vitals.dead();

    vitals.peaceful = difficulty() == PEACEFUL;
    if (!paused && !dead) {
        if (!sleeping_in) move(dt);
        if (gui || sleeping_in) {
            target.reset();
        } else {
            update_target();
            mine(dt);
            if (mouse_down(GLFW_MOUSE_BUTTON_LEFT) && target && !targeting_mob()) hand.swing();  // digging
            if (mouse_down(GLFW_MOUSE_BUTTON_RIGHT) && use_cooldown <= 0) use_item_by_hand();
        }
        use_cooldown -= dt;
        break_cooldown -= dt;
        attack_cooldown -= dt;
        if (!creative && vitals.tick(dt)) {
            hud->flash_damage();
            if (sleeping_in) wake_up(false);
        }
    } else {
        target.reset();
        hud->set_mining(std::nullopt);
    }

    if (!paused) {
        // The hand follows the hotbar and sways with walking (on the ground, alive, not flying).
        const bool walking = player.on_ground && !flying && !dead && !sleeping_in;
        hand.bobbing = options.view_bobbing;
        hand.update(dt, active_inventory().selected_item(), active_inventory().selected,
                    {std::hypot(player.vx, player.vz), dead ? 0.0 : player.vy, walking});
        update_animals(dt);
        update_monsters(dt);
        update_drops(dt);
        update_arrows(dt);
        update_tnts(dt);
        update_falling(dt);
        for (size_t i = 0; i < puffs.size();) {
            if (puffs[i].tick(dt)) {
                ++i;
            } else {
                puffs[i] = puffs.back();
                puffs.pop_back();
            }
        }
        ticks_ += dt * 20;
        const int whole = static_cast<int>(ticks_);
        ticks_ -= whole;
        for (int i = 0; i < whole; ++i) {
            ++level.time;
            tick_world();
        }
        autosave_timer += dt;
        if (autosave_timer >= AUTOSAVE_INTERVAL) save(false);
    }
    renderer->update(player.x, player.z);

    const double view = renderer->render_distance * CHUNK;
    std::vector<ModelPart> parts;
    auto in_view = [&](const Body& b) { return std::abs(b.x - player.x) < view && std::abs(b.z - player.z) < view; };
    // Models are lit by the light in the block where they stand.
    auto add_lit = [&](std::vector<ModelPart> more, const Body& b) {
        int x = ifloor(b.x), y = ifloor(b.y + 0.2), z = ifloor(b.z);
        float sky = world.sky_light(x, y, z) / static_cast<float>(MAX_LIGHT);
        float blk = world.block_light(x, y, z) / static_cast<float>(MAX_LIGHT);
        for (ModelPart& part : more) {
            part.sky = sky;
            part.block = blk;
            parts.push_back(part);
        }
    };
    for (const auto& a : animals)
        if (in_view(a->body)) add_lit(a->parts(), a->body);
    for (const auto& m : monsters)
        if (in_view(m->body)) add_lit(m->parts(), m->body);
    for (const auto& tnt : tnts) add_lit(tnt->parts(), tnt->body);
    // Falling sand and gravel: the dropped-item cube, blown up to a whole block.
    for (const auto& f : falling) {
        const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(f->body.x, f->body.y + 0.49, f->body.z)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(0.98f / 0.25f));
        add_lit({{ITEM_MODEL_BASE + f->block, m}}, f->body);
    }
    for (const Puff& puff : puffs)
        for (ModelPart part : puff.parts()) parts.push_back(part);
    // A little monster spins inside each mob spawner near the player.
    const double spin = (static_cast<double>(level.time % 7200) + ticks_) * 9;
    for (const Spawner& s : spawners) {
        const glm::dvec3 c = glm::dvec3(s.pos) + 0.5;
        if (glm::length(c - glm::dvec3(player.x, player.y, player.z)) > SPAWNER_RANGE) continue;
        const MobKind kind = spawner_mob(level.seed, s.pos);
        const double sc = kind == MobKind::Spider ? 0.3 : 0.4;
        glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(c.x, s.pos.y + 0.15, c.z)) *
                         glm::rotate(glm::mat4(1.0f), static_cast<float>(radians(spin)), glm::vec3(0, 1, 0)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(sc)));
        for (ModelPart part : monster_model(kind, 1, root, 0, 0)) {
            part.sky = world.sky_light(s.pos.x, s.pos.y, s.pos.z) / static_cast<float>(MAX_LIGHT);
            part.block = world.block_light(s.pos.x, s.pos.y, s.pos.z) / static_cast<float>(MAX_LIGHT);
            parts.push_back(part);
        }
    }
    for (const auto& drop : drops)
        if (in_view(drop->body)) add_lit(drop->parts(), drop->body);
    for (const auto& arrow : arrows) {
        int x = ifloor(arrow->pos.x), y = ifloor(arrow->pos.y), z = ifloor(arrow->pos.z);
        if (arrow->stuck) {  // light from the open side, not from inside the block
            glm::dvec3 back = arrow->pos - glm::dvec3(std::sin(radians(arrow->yaw)), 0, std::cos(radians(arrow->yaw))) * 0.3;
            x = ifloor(back.x);
            z = ifloor(back.z);
            y = ifloor(back.y + std::sin(radians(arrow->pitch)) * 0.3);
        }
        for (ModelPart part : arrow->parts()) {
            part.sky = world.sky_light(x, y, z) / static_cast<float>(MAX_LIGHT);
            part.block = world.block_light(x, y, z) / static_cast<float>(MAX_LIGHT);
            parts.push_back(part);
        }
    }
    if (third_person && !dead) {
        PlayerPose pose{player.x, player.y, player.z, yaw, pitch, walk_phase, walk_amount, sneaking};
        pose.armor = worn_armor(inventory);
        add_lit(player_parts(pose), player);
    }
    renderer->set_instances(std::move(parts));
    renderer->set_outline(target && !targeting_mob() ? std::optional<glm::ivec3>(target->block) : std::nullopt);
    hud->update(static_cast<float>(dt));
}

std::string Game::help_text() const {
    auto k = [&](KeyAction a) { return key_name(options.keys[a]); };
    return k(KEY_FORWARD) + k(KEY_LEFT) + k(KEY_BACK) + k(KEY_RIGHT) + " move   " + k(KEY_JUMP) + " jump/swim   " + k(KEY_SNEAK) +
           " sneak   double-tap " + k(KEY_FORWARD) + " sprint\n"
           "Left mouse: mine block / hit a mob (hold to mine; stone needs a pickaxe)\n"
           "Right mouse: place block / eat / shoot the bow / use flint and steel or a bucket\n"
           "1-9 or wheel: pick slot   " + k(KEY_DROP) + ": drop   " + k(KEY_INVENTORY) + ": inventory and crafting\n" +
           k(KEY_CREATIVE) + ": creative cheat (double-tap " + k(KEY_JUMP) + " to fly)\n"
           "F1 hide HUD   F2 screenshot   F3 debug   F5 third person\n"
           "ESC: menu   H: hide this help";
}

std::string Game::debug_text() {
    char buf[640];
    int bx = ifloor(player.x), by = ifloor(player.y), bz = ifloor(player.z);
    // Beta's facing number: 0 south (+z), 1 west, 2 north, 3 east.
    int f = ifloor(-yaw / 90.0 + 0.5) & 3;
    static const char* FACING[4] = {"south", "west", "north", "east"};
    // Light where the player's feet are: sky light (dimmed at night) or block light, whichever is brighter.
    int sky = world.sky_light(bx, by, bz), blk = world.block_light(bx, by, bz);
    int dimmed = std::max(0, sky - static_cast<int>(current_sky().darkness));
    int minutes = clock_minutes(level.time);
    std::snprintf(buf, sizeof buf,
                  "PetrutzCraft M10 (%.0f fps, %d chunk updates)\n"
                  "x: %.3f (block %d)\ny: %.3f (feet, block %d)\nz: %.3f (block %d)\n"
                  "Chunk: %d %d (at %d %d in it)\n"
                  "Facing: %s (f: %d)\n"
                  "Biome: %s\n"
                  "Light: %d (%d sky, %d block)\n"
                  "Time: day %lld, %02d:%02d\n"
                  "Weather: %s (rain %.2f, thunder %.2f)\n"
                  "Seed: %lld\n"
                  "Difficulty: %s\n"
                  "Chunks loaded: %zu   animals: %zu   monsters: %zu   items: %zu",
                  fps, renderer->jobs_in_flight(), player.x, bx, player.y, by, player.z, bz, chunk_of(bx), chunk_of(bz),
                  bx & 15, bz & 15, FACING[f], f, biome_name(world.generator().biome_at(bx, bz)), std::max(dimmed, blk),
                  sky, blk, static_cast<long long>(level.time / DAY_TICKS + 1), minutes / 60, minutes % 60,
                  weather.is_thundering() ? "thunderstorm" : weather.is_raining() ? "rain" : "clear",
                  weather.rain_strength(), weather.thunder_strength(),
                  static_cast<long long>(level.seed), difficulty_name(difficulty()), world.chunk_count(), animals.size(),
                  monsters.size(), drops.size());
    return buf;
}

void Game::draw(int w, int h) {
    glViewport(0, 0, w, h);
    glm::vec3 cam = camera();
    SkyState sky = current_sky();
    // Rain and snow fall within 10 blocks of the camera with Fancy graphics, 5 with Fast (Beta).
    std::vector<WorldRenderer::Bolt> shown;
    for (const LightningBolt& b : bolts)
        if (b.visible()) shown.push_back({b.pos, b.seed});
    renderer->set_weather(precipitation_columns(world, cam, options.fancy_graphics ? 10 : 5, sky.rain), std::move(shown),
                          (static_cast<double>(level.time % 72000) + ticks_) / 20);
    renderer->set_view_bob(third_person ? glm::mat4(1.0f) : hand.bob());
    renderer->draw(w, h, cam, forward(), sky);
    if (!third_person && !hud_hidden && !sleeping_in && !vitals.dead()) {
        // The hand is lit by the light where the eyes are.
        std::vector<ModelPart> held = hand.parts();
        glm::vec3 e = eye();
        const int x = ifloor(e.x), y = ifloor(e.y), z = ifloor(e.z);
        for (ModelPart& part : held) {
            part.sky = world.sky_light(x, y, z) / static_cast<float>(MAX_LIGHT);
            part.block = world.block_light(x, y, z) / static_cast<float>(MAX_LIGHT);
        }
        renderer->draw_hand(held, w, h, sky.darkness);
    }
    if (sleeping_in) {
        // The screen fades to dark while falling asleep.
        float a = std::min(1.0f, (sleep_ticks + static_cast<float>(ticks_)) / SLEEP_TICKS) * 0.85f;
        painter_.begin(w, h);
        painter_.rect(0, 0, static_cast<float>(w), static_cast<float>(h), 0.02f, 0.02f, 0.06f, a);
        painter_.end();
    }

    if (hud_hidden && !vitals.dead()) return;
    const bool underwater = under_fluid(world, cam, WATER);
    std::string info = debug_screen ? debug_text() : "PetrutzCraft M10";
    if (!debug_screen && creative) info += flying ? "\nCreative (flying)" : "\nCreative";
    HudState hs{&active_inventory(), &vitals, armor_value(inventory), creative, info, help_text(), underwater, vitals.dead()};
    hs.burning = burn_ticks > 0 && !creative;
    if (head_under_water() && !creative) hs.bubbles = vitals.bubbles();
    hud->draw(w, h, hs);
    if (gui && !vitals.dead()) {
        double mx, my;
        gui_mouse(mx, my);
        painter_.begin(w, h);
        gui->draw(painter_, w, h, mx, my,
                  [this](int x, int y, int pw, int ph, float lx, float ly) { draw_player_preview(x, y, pw, ph, lx, ly); });
        painter_.end();
    }
}

void Game::draw_player_preview(int x, int y, int w, int h, float look_x, float look_y) {
    // The player stands at the origin facing the camera (+z) and turns towards the mouse.
    PlayerPose pose;
    pose.yaw = std::clamp(std::atan(look_x * 2.0) * 40.0, -40.0, 40.0);
    pose.pitch = std::clamp(std::atan(look_y * 2.0) * 50.0, -50.0, 50.0);
    pose.sneaking = false;
    pose.armor = worn_armor(inventory);
    const float aspect = static_cast<float>(w) / std::max(1, h);
    glm::mat4 proj = glm::ortho(-1.0f * aspect, 1.0f * aspect, -1.0f, 1.0f, 0.1f, 20.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0.92f, 5), glm::vec3(0, 0.92f, 0), glm::vec3(0, 1, 0));
    renderer->draw_models(player_parts(pose), x, y, w, h, proj * view);
    int fw, fh;
    glfwGetFramebufferSize(window, &fw, &fh);
    glViewport(0, 0, fw, fh);
}

void Game::move(double dt) {
    Body& p = player;
    if (riding) {
        if (action_down(KEY_SNEAK)) {
            dismount();
        } else {
            // Sitting on the pig: it goes where it wants and the player goes with it.
            p.x = riding->body.x;
            p.y = riding->body.y + RIDE_HEIGHT;
            p.z = riding->body.z;
            p.vx = p.vy = p.vz = 0;
            p.on_ground = true;
            fall_peak = p.y;
            was_on_ground = true;
            sneaking = false;
            walk_amount += (0 - walk_amount) * std::min(1.0, dt * 10);
            return;
        }
    }
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
    if (p.in_lava && !flying) speed *= 0.25;  // Beta: lava drags you more than twice as hard as water
    double accel = (p.on_ground || flying) ? 14 : 5;
    double k = std::min(1.0, accel * dt);
    // Water currents carry you along.
    const glm::dvec3 drift = flying ? glm::dvec3(0) : p.current * CURRENT_SPEED;
    p.vx += (wx * speed + drift.x - p.vx) * k;
    p.vz += (wz * speed + drift.z - p.vz) * k;
    if (!flying) p.vy += p.current.y * CURRENT_PUSH * dt;

    bool jumped = false;
    if (flying) {
        p.vy = (action_down(KEY_JUMP) - down) * 8.0;
    } else {
        p.apply_gravity(dt);
        if (action_down(KEY_JUMP)) {
            if (p.in_liquid()) {
                p.vy = std::min(p.vy + 30 * dt, p.in_lava ? 2.0 : 4.0);
            } else if (p.on_ground) {
                p.vy = JUMP_SPEED;
                jumped = true;
            }
        }
    }

    double ox = p.x, oz = p.z;
    double dx = p.vx * dt, dz = p.vz * dt;
    // Sneaking on the ground never walks off an edge.
    if (sneaking && p.on_ground && !jumped && !p.in_liquid()) p.clamp_to_edges(dx, dz);
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
    if (p.in_liquid() || flying || creative) {
        fall_peak = p.y;
    } else if (!p.on_ground) {
        fall_peak = std::max(fall_peak, p.y);
    } else {
        if (!was_on_ground) {
            int dmg = fall_damage(fall_peak - p.y);
            if (dmg) hurt_player(dmg, true);
            FarmEvents ev;
            if (trample(world, {ifloor(p.x), ifloor(p.y - 0.2), ifloor(p.z)}, fall_peak - p.y, farm_rng_, ev)) apply(ev);
        }
        fall_peak = p.y;
    }
    was_on_ground = p.on_ground;
}

void Game::update_target() {
    glm::vec3 origin = eye(), direction = forward();
    target = world.raycast(origin, direction, REACH);
    Animal* best = nullptr;
    Monster* best_monster = nullptr;
    float best_d = ATTACK_REACH;
    for (const auto& a : animals) {
        if (!a->alive() || a.get() == riding) continue;
        auto d = a->ray_hit(origin, direction, ATTACK_REACH);
        if (d && *d < best_d) {
            best = a.get();
            best_d = *d;
        }
    }
    for (const auto& m : monsters) {
        if (!m->alive()) continue;
        auto d = m->ray_hit(origin, direction, ATTACK_REACH);
        if (d && *d < best_d) {
            best = nullptr;
            best_monster = m.get();
            best_d = *d;
        }
    }
    // A block in front hides the mob, except plants: you can hit a cow standing in tall grass.
    if ((best || best_monster) && target && target->distance < best_d &&
        !is_plant(world.get_block(target->block.x, target->block.y, target->block.z)))
        best = nullptr, best_monster = nullptr;
    target_animal = best;
    target_monster = best_monster;
}

// --- actions ------------------------------------------------------------------------------------

void Game::try_attack() {
    if (!targeting_mob() || attack_cooldown > 0) return;
    attack_cooldown = 0.35;
    const ItemId held = active_inventory().selected_item().value_or(AIR);
    const int damage = creative ? 10 : attack_damage(held);
    if (target_animal) {
        Animal& a = *target_animal;
        a.hurt(damage, player.x, player.z);
        if (a.kind == AnimalKind::Wolf && !a.tamed) {
            // A wild wolf turns on you, and so does its pack (Beta).
            for (const auto& w : animals)
                if (w->kind == AnimalKind::Wolf && !w->tamed && w->alive() &&
                    std::hypot(w->body.x - a.body.x, w->body.z - a.body.z) < WOLF_ANGER_RANGE)
                    w->angry = true;
        }
        if (!(a.kind == AnimalKind::Wolf && a.tamed)) alert_wolves(&a);
    } else {
        target_monster->hurt(damage, player.x, player.z);
        target_monster->targeting = true;  // monsters turn on whoever hits them
        alert_wolves(target_monster);
    }
    if (creative) return;
    vitals.exhaust(EXHAUST_ATTACK);
    wear_selected(wear_from_hit(held));
}

void Game::mine(double dt) {
    if (!mouse_down(GLFW_MOUSE_BUTTON_LEFT) || !target || targeting_mob()) {
        mining_pos.reset();
        mining_progress = 0;
        hud->set_mining(std::nullopt);
        return;
    }
    glm::ivec3 pos = target->block;
    BlockId blk = world.get_block(pos.x, pos.y, pos.z);
    const BlockInfo* info = block_info(blk);
    const ItemId held = active_inventory().selected_item().value_or(AIR);
    const auto seconds = break_seconds(held, blk);
    if (!info || (!seconds && !creative) || blk == BEDROCK) {
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
    // Beta: digging is five times slower with your head under water, and again while off the ground.
    double slow = 1;
    if (head_under_water()) slow *= 5;
    if (!player.on_ground && !flying) slow *= 5;
    mining_progress += *seconds <= 0 ? 1.0 : dt / (*seconds * slow);
    hud->set_mining(static_cast<float>(mining_progress));
    if (mining_progress >= 1.0) {
        break_block(pos, blk);
        mining_pos.reset();
        mining_progress = 0;
    }
}

void Game::break_block(glm::ivec3 pos, BlockId blk) {
    const int data = world.get_data(pos.x, pos.y, pos.z);
    renderer->rebuild(world.set_block(pos.x, pos.y, pos.z, AIR));
    {
        // Plants and cake standing on this block fall off.
        FarmEvents ev;
        break_unsupported(world, pos, farm_rng_, ev);
        if (creative) ev.drops.clear();
        apply(ev);
    }
    // Torches that hung on this block fall off.
    for (const FaceDef& f : face_defs()) {
        glm::ivec3 n = pos + f.normal;
        BlockId b = world.get_block(n.x, n.y, n.z);
        if (is_torch(b) && n + torch_support(b) == pos) {
            renderer->rebuild(world.set_block(n.x, n.y, n.z, AIR));
            if (!creative) spawn_drop(TORCH, n.x + 0.5, n.y + 0.3, n.z + 0.5);
        }
    }
    if (is_furnace(blk)) drop_furnace_contents(pos);
    if (blk == TNT) {
        // Breaking TNT lights it (Beta), whatever you hit it with.
        prime_tnt(pos, TNT_FUSE_TICKS);
        if (!creative) vitals.exhaust(EXHAUST_BREAK);
        return;
    }
    if (is_bed(blk)) {
        // Both halves go, and one bed drops.
        glm::ivec3 o = bed_other_half(pos, blk);
        if (is_bed(world.get_block(o.x, o.y, o.z))) renderer->rebuild(world.set_block(o.x, o.y, o.z, AIR));
        if (sleeping_in && (*sleeping_in == pos || *sleeping_in == o)) wake_up(false);
        if (!creative) spawn_drop(BED, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
    }
    if (creative) return;
    vitals.exhaust(EXHAUST_BREAK);
    const ItemId held = inventory.selected_item().value_or(AIR);
    wear_selected(wear_from_breaking(held, blk));
    if (blk == LEAVES && held != SHEARS && rng.random() < 0.1) {
        spawn_drop(APPLE, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
        return;
    }
    if (blk == WHEAT_CROP || blk == TALL_GRASS || blk == FERN) {
        for (ItemId item : farm_drops(blk, data, farm_rng_)) spawn_drop(item, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
        return;
    }
    auto drop = harvest_drop(held, blk);
    if (!drop) return;
    if (blk == GRAVEL && rng.below(10) == 0) drop = FLINT;  // Beta: gravel drops flint 1 time in 10
    const BlockInfo* info = block_info(blk);
    int count = *drop == blk ? 1 : rng.randint(info->drop_min, info->drop_max);
    for (int i = 0; i < count; ++i) spawn_drop(*drop, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
}

void Game::spawn_drop(ItemId item, double x, double y, double z) {
    drops.push_back(std::make_unique<ItemDrop>(world, Stack{item, 1}, x, y, z, rng));
}

void Game::drop_selected() {
    Inventory& inv = active_inventory();
    auto& slot = inv.slots[inv.selected];
    if (!slot) return;
    Stack one = *slot;
    one.count = 1;
    if (!creative && --slot->count == 0) slot.reset();
    throw_stack(one);
}

void Game::throw_stack(const Stack& stack) {
    glm::vec3 e = eye(), f = forward();
    auto drop = std::make_unique<ItemDrop>(world, stack, e.x, e.y - 0.3, e.z, rng);
    drop->body.vx = f.x * 6;
    drop->body.vy = f.y * 6 + 1.5;
    drop->body.vz = f.z * 6;
    drop->pickup_delay = 2.0;  // so it isn't picked straight back up
    drops.push_back(std::move(drop));
}

void Game::use_item_by_hand() {
    const long edits = renderer->edit_count();
    const bool had_gui = gui != nullptr, was_asleep = sleeping_in.has_value();
    use_item();
    if (renderer->edit_count() != edits || (gui && !had_gui) || (sleeping_in && !was_asleep)) hand.swing();
}

void Game::use_item() {
    use_cooldown = 0.25;
    if (target && !targeting_mob() && world.get_block(target->block.x, target->block.y, target->block.z) == CRAFTING_TABLE) {
        mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);
        open_gui(InventoryScreen::Kind::CraftingTable);
        return;
    }
    if (target && !targeting_mob() && is_furnace(world.get_block(target->block.x, target->block.y, target->block.z))) {
        mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);
        open_furnace(target->block);
        return;
    }
    if (target && !targeting_mob() && is_bed(world.get_block(target->block.x, target->block.y, target->block.z))) {
        mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);
        try_sleep(target->block);
        return;
    }
    if (target && !targeting_mob() && world.get_block(target->block.x, target->block.y, target->block.z) == CAKE_BLOCK) {
        // Eat a slice of cake (Beta: right click the cake). The last slice takes the cake away.
        const glm::ivec3 c = target->block;
        if (!creative && vitals.eat(CAKE_SLICE)) {
            const int eaten = world.get_data(c.x, c.y, c.z) + 1;
            renderer->rebuild(eaten >= CAKE_SLICES ? world.set_block(c.x, c.y, c.z, AIR)
                                                   : world.set_block(c.x, c.y, c.z, CAKE_BLOCK, eaten));
            hud->show_message("Yum!", 1);
        }
        return;
    }
    if (target_animal && interact(*target_animal)) return;
    Inventory& inv = active_inventory();
    auto item = inv.selected_item();
    if (!item) return;
    if (tool_kind(*item) == ToolKind::Hoe) {
        FarmEvents ev;
        if (target && !targeting_mob() && till(world, target->block, ev)) {
            apply(ev);
            wear_selected(1);
        }
        return;
    }
    if (*item == BONE_MEAL) {
        FarmEvents ev;
        if (target && !targeting_mob() && bone_meal(world, target->block, farm_rng_, ev)) {
            apply(ev);
            if (!creative) inv.take_selected();
        }
        return;
    }
    if (BlockId p = planted_block(*item); p != AIR) {
        plant(p);
        return;
    }
    if (*item == BOW) {
        shoot_arrow();
        return;
    }
    if (*item == EGG) {
        throw_egg();
        return;
    }
    if (*item == FLINT_AND_STEEL) {
        if (target && !targeting_mob() && world.get_block(target->block.x, target->block.y, target->block.z) == TNT) {
            prime_tnt(target->block, TNT_FUSE_TICKS);  // the flame lights the TNT
            wear_selected(1);
        } else if (target && !targeting_mob() && target->before && light_fire(*target->before)) {
            wear_selected(1);
        }
        return;
    }
    if (*item == MILK_BUCKET) {  // drinking: in Beta milk does nothing else
        if (!creative) swap_selected(BUCKET);
        return;
    }
    if (*item == BUCKET || *item == WATER_BUCKET || *item == LAVA_BUCKET) {
        use_bucket(*item);
        return;
    }
    if (food_info(*item)) {
        if (!creative && vitals.eat(*item)) {
            inv.take_selected();
            if (*item == MUSHROOM_STEW && !inv.slots[inv.selected]) inv.slots[inv.selected] = Stack{BOWL, 1};
            hud->show_message("Yum!", 1);
        }
        return;
    }
    if (*item >= FUTURE_BLOCKS && target && !targeting_mob() && target->before) {
        hud->show_message(item_name(*item) + " can't be placed yet", 2);
        return;
    }
    if (*item == BED && target && !targeting_mob() && target->before) {
        auto bed = bed_placement(world, target->block, *target->before - target->block, facing_from_yaw(yaw));
        if (!bed) return;
        for (glm::ivec3 c : {bed->foot, bed->head}) {
            const Body& b = player;
            if (c.x + 1 > b.x - b.half && c.x < b.x + b.half && c.z + 1 > b.z - b.half && c.z < b.z + b.half &&
                c.y + 1 > b.y && c.y < b.y + b.height)
                return;  // not on top of yourself
        }
        renderer->rebuild(world.set_block(bed->foot.x, bed->foot.y, bed->foot.z, bed_block(bed->dir, false)));
        renderer->rebuild(world.set_block(bed->head.x, bed->head.y, bed->head.z, bed_block(bed->dir, true)));
        if (!creative) inv.take_selected();
        return;
    }
    if (!is_block(*item) || !target || targeting_mob() || !target->before) return;
    glm::ivec3 c = *target->before;
    BlockId place = static_cast<BlockId>(*item);
    if (place == FURNACE) place = furnace_block(facing_from_yaw(yaw) + 2, false);  // its front towards the player
    if (place == TORCH) {
        // Torches go on top of or on the side of a solid block, and nothing stands in their way.
        auto torch = torch_for_face(c - target->block);
        if (!torch || !is_opaque(world.get_block(target->block.x, target->block.y, target->block.z))) return;
        place = *torch;
    }
    std::vector<const Body*> bodies{&player};
    for (const auto& a : animals) bodies.push_back(&a->body);
    for (const auto& m : monsters) bodies.push_back(&m->body);
    for (const Body* b : bodies)
        if (is_solid(place) && c.x + 1 > b->x - b->half && c.x < b->x + b->half && c.z + 1 > b->z - b->half && c.z < b->z + b->half &&
            c.y + 1 > b->y && c.y < b->y + b->height)
            return;  // don't build inside someone
    BlockId here = world.get_block(c.x, c.y, c.z);
    if (here != AIR && !is_fluid(here)) return;
    renderer->rebuild(world.set_block(c.x, c.y, c.z, place));
    if (is_solid(place) && world.get_block(c.x, c.y - 1, c.z) == FARMLAND)
        renderer->rebuild(world.set_block(c.x, c.y - 1, c.z, DIRT));  // Beta: farmland under a block turns to dirt
    if (!creative) inv.take_selected();
}

bool Game::hurt_player(int amount, bool ignore_cooldown) {
    if (creative || amount <= 0 || vitals.dead() || (vitals.hurt_timer > 0 && !ignore_cooldown)) return false;
    amount = absorb_with_armor(inventory, amount, armor_remainder);
    if (!vitals.damage(amount, ignore_cooldown)) {
        vitals.hurt_timer = 0.5;  // armor took it all; still a short invulnerability, like Beta
        return false;
    }
    hud->flash_damage();
    if (sleeping_in) wake_up(false);
    if (vitals.dead()) on_death();
    return true;
}

bool Game::hurt_player_by_monster(int amount, double from_x, double from_z) {
    const int scaled = scale_monster_damage(amount, difficulty());
    const bool fresh = vitals.hurt_timer <= 0;
    if (!hurt_player(scaled)) return false;
    if (fresh) {
        // Knocked back a little, like Beta.
        double dx = player.x - from_x, dz = player.z - from_z, d = std::hypot(dx, dz);
        if (d < 1e-6) {
            dx = 0;
            dz = d = 1;
        }
        player.vx = dx / d * 7;
        player.vz = dz / d * 7;
        if (player.on_ground && !flying) player.vy = 5.5;
    }
    return true;
}

void Game::wear_selected(int uses) {
    if (creative || uses <= 0) return;
    auto& slot = inventory.slots[inventory.selected];
    if (slot && wear_out(*slot, uses)) slot.reset();
}

void Game::swap_selected(ItemId item) {
    Inventory& inv = active_inventory();
    auto& slot = inv.slots[inv.selected];
    if (!slot) return;
    if (slot->count == 1) {
        slot = Stack{item, 1};
        return;
    }
    --slot->count;
    if (inv.add(item, 1)) throw_stack(Stack{item, 1});
}

bool Game::use_bucket(ItemId held) {
    if (held == BUCKET) {
        // An empty bucket scoops up the first water or lava source it points at (Beta looks
        // straight through flowing water).
        auto hit = world.raycast(eye(), forward(), REACH, [this](glm::ivec3 c, BlockId b) {
            return b != AIR && (!is_fluid(b) || world.get_data(c.x, c.y, c.z) == 0);
        });
        if (!hit) return false;
        BlockId b = world.get_block(hit->block.x, hit->block.y, hit->block.z);
        if (!is_fluid(b)) return false;
        renderer->rebuild(world.set_block(hit->block.x, hit->block.y, hit->block.z, AIR));
        if (!creative) swap_selected(b == WATER ? WATER_BUCKET : LAVA_BUCKET);
        return true;
    }
    // A full bucket pours a source where a block would be placed; it flows from there.
    if (!target || targeting_mob() || !target->before) return false;
    glm::ivec3 c = *target->before;
    BlockId here = world.get_block(c.x, c.y, c.z);
    if (here != AIR && !is_fluid(here) && here != FIRE) return false;
    renderer->rebuild(world.set_block(c.x, c.y, c.z, held == WATER_BUCKET ? WATER : LAVA));
    if (!creative) swap_selected(BUCKET);
    return true;
}

bool Game::light_fire(glm::ivec3 pos) {
    // Fire needs an empty cell, and solid ground under it or something next to it that burns.
    if (world.get_block(pos.x, pos.y, pos.z) != AIR || !fire_can_stay(world, pos)) return false;
    renderer->rebuild(world.set_block(pos.x, pos.y, pos.z, FIRE));
    return true;
}

void Game::throw_egg() {
    // Thrown like Beta's eggs and snowballs: as fast as an arrow, a little less accurate.
    if (!creative) active_inventory().take_selected();
    glm::dvec3 dir(forward());
    dir += glm::dvec3(animal_rng_.uniform(-1, 1), animal_rng_.uniform(-1, 1), animal_rng_.uniform(-1, 1)) * 0.0075;
    dir = glm::normalize(dir);
    auto egg = std::make_unique<Arrow>(glm::dvec3(eye()) + dir * 0.3, dir * Arrow::SPEED);
    egg->item = EGG;
    arrows.push_back(std::move(egg));
    hand.swing();
}

bool Game::interact(Animal& a) {
    Inventory& inv = active_inventory();
    const ItemId held = inv.selected_item().value_or(AIR);
    auto use_up = [&] {
        if (!creative) inv.take_selected();
    };
    switch (a.kind) {
        case AnimalKind::Cow:
            if (held != BUCKET) return false;
            if (!creative) swap_selected(MILK_BUCKET);  // milking
            return true;
        case AnimalKind::Pig:
            if (held == SADDLE && !a.saddled) {
                a.saddled = true;
                use_up();
                return true;
            }
            if (!a.saddled || riding) return false;
            // Climb on. Beta's pigs can't be steered: it wanders where it likes. Sneak to get off.
            riding = &a;
            a.ridden = true;
            mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);
            hud->show_message("Sneak to get off the pig", 3);
            return true;
        case AnimalKind::Sheep:
            if (held == SHEARS && !a.sheared) {
                a.sheared = true;
                for (int n = shear_count(animal_rng_); n > 0; --n) {
                    spawn_drop(wool_item(a.wool), a.body.x, a.body.y + 1.0, a.body.z);
                    drops.back()->body.vy = animal_rng_.uniform(3, 5);
                }
                wear_selected(1);
                return true;
            }
            if (auto color = dye_wool_color(held); color && !a.sheared && *color != a.wool) {
                a.wool = *color;
                use_up();
                return true;
            }
            return false;
        case AnimalKind::Wolf:
            if (!a.tamed) {
                if (held != BONE || a.angry) return false;
                use_up();
                if (tame_succeeds(animal_rng_)) {
                    a.tame();
                    add_puffs(puffs, {a.body.x, a.body.y + 1, a.body.z}, 7, 0.4, 0.6, 0.12, animal_rng_);
                    hud->show_message("The wolf is tame", 2);
                } else {
                    add_puffs(puffs, {a.body.x, a.body.y + 1, a.body.z}, 3, 0.4, 0.6, 0.12, animal_rng_);
                }
                return true;
            }
            if (const int heal = wolf_food_heal(held); heal > 0 && a.health < a.max_health()) {
                a.health = std::min(a.max_health(), a.health + heal);
                use_up();
                return true;
            }
            // Anything else: sit down, or get up and follow.
            a.sitting = !a.sitting;
            a.prey = nullptr;
            mouse_buttons_.erase(GLFW_MOUSE_BUTTON_RIGHT);
            return true;
        case AnimalKind::Chicken:
        case AnimalKind::Squid:
            return false;
    }
    return false;
}

void Game::dismount() {
    if (!riding) return;
    Animal& pig = *riding;
    pig.ridden = false;
    riding = nullptr;
    // Stand on top of the pig, or beside it if there's no room above.
    player.x = pig.body.x;
    player.z = pig.body.z;
    player.y = pig.body.y + pig.body.height;
    for (int i = 0; i < 4 && player.collides(); ++i) {
        player.x = pig.body.x + (i & 1 ? 1.0 : -1.0) * (i < 2 ? 1 : 0);
        player.z = pig.body.z + (i & 1 ? 1.0 : -1.0) * (i < 2 ? 0 : 1);
        player.y = pig.body.y;
    }
    if (player.collides()) {
        player.x = pig.body.x;
        player.z = pig.body.z;
        player.y = pig.body.y + pig.body.height;
    }
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
}

void Game::alert_wolves(const void* enemy) {
    for (const auto& w : animals)
        if (w->kind == AnimalKind::Wolf && w->tamed && !w->sitting && w->alive() && w.get() != enemy &&
            std::hypot(w->body.x - player.x, w->body.z - player.z) < 16)
            w->prey = enemy;
}

void Game::shoot_arrow() {
    // Beta's bow fires as soon as you click (no drawing back), using up an arrow.
    if (!creative && !inventory.remove(ARROW, 1)) return;
    glm::dvec3 dir(forward());
    // Beta's slight random spread.
    dir += glm::dvec3(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)) * 0.0075;
    dir = glm::normalize(dir);
    glm::dvec3 from = glm::dvec3(eye()) + dir * 0.3;
    arrows.push_back(std::make_unique<Arrow>(from, dir * Arrow::SPEED));
    wear_selected(1);
}

void Game::on_death() {
    close_gui();
    if (riding) dismount();
    // Everything you carry drops where you died (Beta).
    auto scatter = [&](std::optional<Stack>& slot) {
        if (!slot) return;
        auto drop = std::make_unique<ItemDrop>(world, *slot, player.x, player.y + 1.0, player.z, rng);
        const double a = rng.uniform(0, 2 * PI), f = rng.uniform(0, 4);
        drop->body.vx = std::sin(a) * f;
        drop->body.vz = std::cos(a) * f;
        drop->body.vy = 4;
        drop->pickup_delay = 2.0;
        drops.push_back(std::move(drop));
        slot.reset();
    };
    for (auto& slot : inventory.slots) scatter(slot);
    for (auto& slot : inventory.armor) scatter(slot);
    hud->set_mining(std::nullopt);
    mouse_buttons_.clear();
    set_mouse_captured(false);
}

void Game::respawn() {
    vitals.reset();
    armor_remainder = 0;
    glm::dvec3 at = spawn;
    if (bed_spawn) {
        // Next to the bed last slept in, if it is still there and not walled in (Beta).
        if (auto spot = bed_stand_spot(world, *bed_spawn)) {
            at = glm::dvec3(spot->x + 0.5, spot->y, spot->z + 0.5);
        } else {
            bed_spawn.reset();
            hud->show_message("Your home bed was missing or obstructed", 4);
        }
    }
    player.x = at.x;
    player.y = at.y;
    player.z = at.z;
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
    set_paused(false);
}

// --- weather and beds ----------------------------------------------------------------------------

SkyState Game::current_sky() const {
    const float partial = static_cast<float>(ticks_);
    float flash = lightning_flash > 0 ? std::min(1.0f, lightning_flash - partial) : 0.0f;
    SkyState sky = sky_state(static_cast<double>(level.time) + ticks_, weather.rain_strength(partial),
                             weather.thunder_strength(partial), flash);
    sky.show_clouds = options.clouds;
    sky.fancy_clouds = options.fancy_graphics;
    return sky;
}

void Game::tick_world() {
    weather.tick(weather_rng_);
    if (lightning_flash > 0) --lightning_flash;
    for (size_t i = 0; i < bolts.size();) {
        bool reflash = false;
        if (!bolts[i].tick(weather_rng_, reflash)) {
            bolts.erase(bolts.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (reflash) strike(bolts[i].pos);
        ++i;
    }
    if (weather.is_thundering()) {
        // Beta rolls for lightning in every loaded chunk within 9 chunks of the player.
        const int pcx = chunk_of(ifloor(player.x)), pcz = chunk_of(ifloor(player.z));
        for (int cz = pcz - 9; cz <= pcz + 9; ++cz)
            for (int cx = pcx - 9; cx <= pcx + 9; ++cx)
                if (world.find(cx, cz))
                    if (auto at = roll_lightning(world, cx, cz, weather_rng_)) {
                        bolts.emplace_back(*at, weather_rng_);
                        strike(*at);
                    }
    }
    spawn_monsters();
    tick_spawners();
    random_ticks();
    tick_furnaces();
    tick_blocks();
    tick_player_fire_and_air();
    if (sleeping_in && !vitals.dead() && ++sleep_ticks >= SLEEP_TICKS) {
        // Asleep long enough: skip to the next morning and clear the sky, like Beta.
        level.time = next_morning(level.time);
        weather.clear();
        wake_up(true);
    }
}

void Game::tick_furnaces() {
    const int pcx = chunk_of(ifloor(player.x)), pcz = chunk_of(ifloor(player.z)), r = renderer->render_distance;
    for (int cz = pcz - r; cz <= pcz + r; ++cz)
        for (int cx = pcx - r; cx <= pcx + r; ++cx) {
            Chunk* chunk = world.find(cx, cz);
            if (!chunk || chunk->furnaces.empty()) continue;
            for (auto it = chunk->furnaces.begin(); it != chunk->furnaces.end();) {
                const int i = it->first;
                const BlockId b = chunk->blocks[i];
                if (!is_furnace(b)) {  // the block was replaced some other way
                    it = chunk->furnaces.erase(it);
                    chunk->dirty = true;
                    continue;
                }
                Furnace& f = it->second;
                const bool was_burning = f.burning();
                if (f.tick()) chunk->dirty = true;
                if (f.burning() != was_burning) {
                    // Lit and unlit furnaces are different blocks (the lit one glows); the contents stay.
                    const glm::ivec3 at(cx * CHUNK + (i & 15), i >> 8, cz * CHUNK + ((i >> 4) & 15));
                    renderer->rebuild(world.set_block(at.x, at.y, at.z, furnace_block(furnace_dir(b), f.burning())));
                }
                ++it;
            }
        }
    // A furnace screen closes when its furnace goes or the player walks away from it (Beta: 8 blocks).
    if (furnace_open) {
        const glm::dvec3 c = glm::dvec3(*furnace_open) + 0.5;
        if (!is_furnace(world.get_block(furnace_open->x, furnace_open->y, furnace_open->z)) ||
            glm::length(c - glm::dvec3(player.x, player.y, player.z)) > 8)
            close_gui();
    }
}

void Game::drop_furnace_contents(glm::ivec3 pos) {
    if (furnace_open == pos) close_gui();
    auto f = world.take_furnace(pos);
    if (!f) return;
    for (const auto& s : {f->input, f->fuel, f->output})
        if (s) drops.push_back(std::make_unique<ItemDrop>(world, *s, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, rng));
}

void Game::random_ticks() {
    const TickContext ctx{static_cast<int>(current_sky().darkness), weather.is_raining()};
    FarmEvents ev;
    const int pcx = chunk_of(ifloor(player.x)), pcz = chunk_of(ifloor(player.z));
    for (int cz = pcz - RANDOM_TICK_RADIUS; cz <= pcz + RANDOM_TICK_RADIUS; ++cz)
        for (int cx = pcx - RANDOM_TICK_RADIUS; cx <= pcx + RANDOM_TICK_RADIUS; ++cx)
            if (const Chunk* c = world.find(cx, cz)) tick_chunk(world, *c, farm_rng_, ctx, ev);
    apply(ev);
}

void Game::apply(const FarmEvents& events) {
    if (!events.rebuild.empty()) renderer->rebuild(events.rebuild);
    for (const auto& [pos, item] : events.drops) spawn_drop(item, pos.x + 0.5, pos.y + 0.3, pos.z + 0.5);
}

bool Game::plant(BlockId block) {
    if (!target || targeting_mob() || !target->before) return false;
    const glm::ivec3 c = *target->before;
    if (world.get_block(c.x, c.y, c.z) != AIR || !can_stay(world, c, block)) return false;
    if (block == CAKE_BLOCK) {  // a cake is solid: not on top of the player
        const Body& b = player;
        if (c.x + 1 > b.x - b.half && c.x < b.x + b.half && c.z + 1 > b.z - b.half && c.z < b.z + b.half &&
            c.y + 1 > b.y && c.y < b.y + b.height)
            return false;
    }
    renderer->rebuild(world.set_block(c.x, c.y, c.z, block));
    if (!creative) active_inventory().take_selected();
    return true;
}

void Game::strike(glm::dvec3 pos) {
    lightning_flash = LIGHTNING_FLASH_TICKS;
    auto near = [&](const Body& b) { return b.distance_to(pos.x, pos.y, pos.z) <= LIGHTNING_REACH + 1; };
    if (!creative && !vitals.dead() && near(player)) hurt_player(LIGHTNING_DAMAGE);
    for (const auto& a : animals)
        if (a->alive() && near(a->body)) {
            a->hurt(LIGHTNING_DAMAGE, pos.x, pos.z);
            a->burning = std::max(a->burning, ANIMAL_BURN_SECONDS);
        }
    for (const auto& m : monsters)
        if (m->alive() && near(m->body)) {
            if (m->kind == MobKind::Creeper) m->powered = true;  // a charged creeper
            m->hurt(LIGHTNING_DAMAGE, pos.x, pos.z);
            m->burning = std::max(m->burning, 8.0);
        }
    if (!creative && !vitals.dead() && near(player)) set_player_on_fire(FIRE_BURN_TICKS);
    // On Normal and Hard the bolt sets fire where it lands and a few blocks around (Beta).
    if (difficulty() >= NORMAL) {
        const glm::ivec3 at(ifloor(pos.x), ifloor(pos.y), ifloor(pos.z));
        auto ignite = [&](glm::ivec3 c) {
            if (world.get_block(c.x, c.y, c.z) == AIR && fire_can_stay(world, c)) renderer->rebuild(world.set_block(c.x, c.y, c.z, FIRE));
        };
        ignite(at);
        for (int i = 0; i < 4; ++i)
            ignite(at + glm::ivec3(weather_rng_.below(3) - 1, weather_rng_.below(3) - 1, weather_rng_.below(3) - 1));
    }
}

bool Game::rain_at(glm::ivec3 p) {
    if (!weather.is_raining()) return false;
    const auto top = world.rain_height(p.x, p.z);
    const auto biome = world.biome_at(p.x, p.z);
    return top && biome && *top <= p.y && precipitation(*biome) == Precipitation::RAIN;
}

bool Game::head_under_water() { return under_fluid(world, eye(), WATER); }

void Game::set_player_on_fire(int ticks) {
    // Beta: catching fire while already alight just keeps it going.
    burn_ticks = burn_ticks > 0 ? burn_ticks + 1 : ticks;
}

void Game::tick_blocks() {
    BlockEvents events;
    RainCheck rain = [this](glm::ivec3 p) { return rain_at(p); };
    blocks.tick(rng, rain, events);
    const ChunkPos here = chunk_at(player.x, player.z);
    blocks.random_ticks(here, std::min(9, renderer->render_distance), rng, rain, events);
    apply_block_events(events);
}

void Game::apply_block_events(BlockEvents& events) {
    if (!events.dirty.empty()) renderer->rebuild(events.dirty);
    for (const auto& [p, item] : events.drops) spawn_drop(item, p.x + 0.5, p.y + 0.3, p.z + 0.5);
    for (const auto& [p, block] : events.falling) falling.push_back(std::make_unique<FallingBlock>(world, block, p));
    for (glm::ivec3 p : events.tnt) prime_tnt(p, TNT_FUSE_TICKS);
    for (glm::ivec3 p : events.fizz) add_puffs(puffs, glm::dvec3(p) + 0.5, 6, 0.4, 1.0, 0.2, rng);
}

void Game::tick_player_fire_and_air() {
    if (vitals.dead() || creative) {
        burn_ticks = 0;
        vitals.air = MAX_AIR;
        return;
    }
    if (const int dmg = vitals.breathe(head_under_water())) hurt_player(dmg);
    // Fire and lava the player touches (Beta's Entity.moveEntity and setOnFireFromLava).
    const Body& p = player;
    const double x0 = p.x - p.half, x1 = p.x + p.half - 1e-6, z0 = p.z - p.half, z1 = p.z + p.half - 1e-6;
    const bool in_fire = world.box_in(FIRE, x0 + 0.001, p.y + 0.001, z0 + 0.001, x1 - 0.001, p.y + p.height - 0.001, z1 - 0.001);
    const bool in_lava = world.box_in(LAVA, x0 + 0.1, p.y + 0.4, z0 + 0.1, x1 - 0.1, p.y + p.height - 0.4, z1 - 0.1);
    const bool wet = player.in_water || rain_at({ifloor(p.x), ifloor(p.y), ifloor(p.z)});
    if (in_lava) {
        hurt_player(LAVA_DAMAGE);
        burn_ticks = LAVA_BURN_TICKS;
    }
    if (in_fire) {
        hurt_player(1);
        if (!wet) set_player_on_fire(FIRE_BURN_TICKS);
    }
    if (wet) burn_ticks = 0;
    if (burn_ticks > 0) {
        if (burn_ticks % 20 == 0) hurt_player(1);
        --burn_ticks;
    }
}

void Game::try_sleep(glm::ivec3 pos) {
    if (sleeping_in) return;
    BlockId b = world.get_block(pos.x, pos.y, pos.z);
    if (!is_bed(b)) return;
    if (!bed_is_head(b)) {
        pos = bed_other_half(pos, b);
        b = world.get_block(pos.x, pos.y, pos.z);
        if (!is_bed(b) || !bed_is_head(b)) return;
    }
    if (!can_sleep(current_sky().darkness)) {
        hud->show_message("You can only sleep at night", 2.5f);
        return;
    }
    for (const auto& m : monsters)
        if (m->alive() && m->kind != MobKind::Slime && std::abs(m->body.x - (pos.x + 0.5)) < 8.5 &&
            std::abs(m->body.y - pos.y) < 5.5 && std::abs(m->body.z - (pos.z + 0.5)) < 8.5) {
            hud->show_message("You may not rest now, there are monsters nearby", 2.5f);
            return;
        }
    sleeping_in = pos;
    sleep_dir = bed_dir(b);
    hud->show_message("", 0);
    sleep_ticks = 0;
    flying = false;
    sneaking = false;
    close_gui();
    // Lie on the bed, looking down it towards the foot.
    glm::dvec3 d(FACING_DIRS[sleep_dir]);
    player.x = pos.x + 0.5;
    player.y = pos.y + block_height(b);
    player.z = pos.z + 0.5;
    player.vx = player.vy = player.vz = 0;
    yaw = std::atan2(-d.x, -d.z) * 180 / PI;
    pitch = 0;
    fall_peak = player.y;
}

void Game::wake_up(bool set_spawn) {
    if (!sleeping_in) return;
    glm::ivec3 head = *sleeping_in;
    sleeping_in.reset();
    sleep_ticks = 0;
    // Stand up next to the bed, or on top of it if there's no room.
    glm::dvec3 at(head.x + 0.5, head.y + 1, head.z + 0.5);
    if (auto spot = bed_stand_spot(world, head)) at = glm::dvec3(spot->x + 0.5, spot->y, spot->z + 0.5);
    player.x = at.x;
    player.y = at.y;
    player.z = at.z;
    player.vx = player.vy = player.vz = 0;
    fall_peak = player.y;
    if (set_spawn) bed_spawn = head;
}

// --- animals and drops -------------------------------------------------------------------------------

void Game::update_animals(double dt) {
    // Past the distance where chunk meshes are dropped, so the chunk is shown (and spawns it) again on the way back.
    const double far_away = (renderer->render_distance + 3) * CHUNK;
    // Where a wolf's prey is (a monster or an animal), if it is still about.
    struct Prey {
        Monster* monster = nullptr;
        Animal* animal = nullptr;
        const Body* body = nullptr;
    };
    auto find_prey = [&](const void* who) {
        Prey p;
        if (!who) return p;
        for (const auto& m : monsters)
            if (m.get() == who && m->alive()) p.monster = m.get(), p.body = &m->body;
        for (const auto& o : animals)
            if (o.get() == who && o->alive()) p.animal = o.get(), p.body = &o->body;
        return p;
    };
    AnimalSenses senses;
    senses.player = {player.x, player.y, player.z};
    senses.player_targetable = !creative && !vitals.dead();
    senses.peaceful = difficulty() == PEACEFUL;
    for (size_t i = 0; i < animals.size();) {
        Animal& a = *animals[i];
        // Skip animals on chunks that aren't generated yet so they don't fall through the world.
        if (!world.find(chunk_of(ifloor(a.body.x)), chunk_of(ifloor(a.body.z)))) {
            ++i;
            continue;
        }
        bool remove = false;
        // Lava burns animals too (4 at a time, like everything else) and sets them alight, as does fire.
        const Body& b = a.body;
        if (a.alive() && b.in_lava) {
            if (a.flash <= 0) a.hurt(LAVA_DAMAGE, b.x, b.z);
            a.burning = std::max(a.burning, ANIMAL_BURN_SECONDS * 2);
        } else if (a.alive() && world.box_in(FIRE, b.x - b.half, b.y, b.z - b.half, b.x + b.half, b.y + b.height, b.z + b.half)) {
            a.burning = std::max(a.burning, ANIMAL_BURN_SECONDS);
        }
        // Wild wolves now and then go after a sheep (Beta: one tick in 900).
        if (a.kind == AnimalKind::Wolf && !a.tamed && !a.angry && !a.prey && animal_rng_.random() < dt * 20 / 900) {
            double best = 16;
            for (const auto& o : animals)
                if (o->kind == AnimalKind::Sheep && o->alive()) {
                    const double d = std::hypot(o->body.x - b.x, o->body.z - b.z);
                    if (d < best) best = d, a.prey = o.get();
                }
        }
        Prey prey = find_prey(a.prey);
        if (!prey.body) a.prey = nullptr;
        senses.prey.reset();
        if (prey.body) {
            senses.prey = glm::dvec3(prey.body->x, prey.body->y, prey.body->z);
            senses.prey_half = prey.body->half;
        }
        AnimalActions act;
        if (a.tick(dt, senses, animal_rng_, act)) {
            for (ItemId item : animal_drops(a.kind, a.loot_state(), animal_rng_)) spawn_drop(item, b.x, b.y + 0.5, b.z);
            add_puffs(puffs, {b.x, b.y + b.height * 0.5, b.z}, 6, b.half + 0.2, 1.0, 0.2, animal_rng_);
            remove = true;
        } else if (std::hypot(b.x - player.x, b.z - player.z) > far_away && &a != riding) {
            // Too far to simulate: keep it with its chunk until the player comes back.
            if (a.alive()) stash_entity(animal_record(a));
            remove = true;
        }
        if (act.bite_player > 0 && hurt_player(act.bite_player)) {
            // Pushed back like any hit.
            const double dx = player.x - b.x, dz = player.z - b.z, d = std::max(1e-6, std::hypot(dx, dz));
            player.vx += dx / d * 6;
            player.vz += dz / d * 6;
            player.vy = std::max(player.vy, 4.0);
        }
        if (act.bite_prey > 0) {
            if (prey.monster) prey.monster->hurt(act.bite_prey, b.x, b.z);
            if (prey.animal) prey.animal->hurt(act.bite_prey, b.x, b.z);
        }
        if (act.lay_egg) spawn_drop(EGG, b.x, b.y + 0.3, b.z);
        if (act.teleport) {
            // A tamed wolf left behind turns up next to its owner, on solid ground.
            for (int tries = 0; tries < 10; ++tries) {
                const int x = ifloor(player.x) + animal_rng_.randint(-2, 2), z = ifloor(player.z) + animal_rng_.randint(-2, 2);
                for (int y = ifloor(player.y) + 1; y >= ifloor(player.y) - 2; --y) {
                    if (!world.is_solid_at(x, y - 1, z) || world.is_solid_at(x, y, z) || world.is_solid_at(x, y + 1, z)) continue;
                    a.body.x = x + 0.5;
                    a.body.y = y;
                    a.body.z = z + 0.5;
                    a.body.vx = a.body.vy = a.body.vz = 0;
                    tries = 10;
                    break;
                }
            }
        }
        if (remove) {
            if (target_animal == &a) target_animal = nullptr;
            if (riding == &a) dismount();
            for (const auto& o : animals)
                if (o->prey == &a) o->prey = nullptr;
            animals.erase(animals.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    // Now and then more appear out of sight on grass, and squid in the sea.
    if (land_animal_count() < 10 && animal_rng_.random() < dt * 0.05) spawn_animal_group(player.x, player.z, 24, 48);
    if (animals.size() - land_animal_count() < 3 && animal_rng_.random() < dt * 0.03) spawn_squid(player.x, player.z, 16, 40);
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
        // Items that fall in lava or fire burn up.
        if (b.in_lava || world.box_in(FIRE, b.x - b.half, b.y, b.z - b.half, b.x + b.half, b.y + b.height, b.z + b.half)) {
            add_puffs(puffs, {b.x, b.y + 0.2, b.z}, 3, 0.2, 0.8, 0.15, rng);
            it = drops.erase(it);
            continue;
        }
        double dx = b.x - player.x, dz = b.z - player.z;
        double horiz = std::hypot(dx, dz);
        bool vert_ok = player.y - 1.2 < b.y && b.y < player.y + 2.2;
        if (drop.age > drop.pickup_delay && !vitals.dead() && vert_ok && horiz < 2.5) {
            if (horiz < 1.2) {
                drop.stack.count = inventory.add(drop.stack);
                if (drop.stack.count == 0) {
                    it = drops.erase(it);
                    continue;
                }
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

void Game::update_arrows(double dt) {
    const double far_away = (renderer->render_distance + 3) * CHUNK;
    // What arrows can hit: animals (kind 0), monsters (1) and, for monsters' arrows, the player (2).
    std::vector<ArrowTarget> mobs;
    for (size_t i = 0; i < animals.size(); ++i)
        if (animals[i]->alive() && animals[i].get() != riding) mobs.push_back(animals[i]->target(0, static_cast<int>(i)));
    for (size_t i = 0; i < monsters.size(); ++i)
        if (monsters[i]->alive()) mobs.push_back(monsters[i]->target(1, static_cast<int>(i)));
    std::vector<ArrowTarget> with_player = mobs;
    if (!creative && !vitals.dead())
        with_player.push_back({glm::vec3(player.x - player.half, player.y, player.z - player.half),
                               glm::vec3(player.x + player.half, player.y + player.height, player.z + player.half), 2, 0});
    // A thrown egg breaks on whatever it hits; now and then a chicken hatches from it (Beta).
    auto hatch = [&](const Arrow& egg) {
        add_puffs(puffs, egg.pos, 4, 0.1, 1.0, 0.1, animal_rng_);
        for (int n = chicks_from_egg(animal_rng_); n > 0; --n) {
            Animal* chick = spawn_animal(AnimalKind::Chicken, egg.pos.x, egg.pos.y, egg.pos.z);
            chick->body.y = std::floor(egg.pos.y);
            if (chick->body.collides()) chick->body.y = std::floor(egg.pos.y) + 1;
        }
    };
    for (auto it = arrows.begin(); it != arrows.end();) {
        Arrow& a = **it;
        bool remove = false;
        std::optional<ArrowTarget> hit;
        if (!world.find(chunk_of(ifloor(a.pos.x)), chunk_of(ifloor(a.pos.z)))) {
            remove = true;  // flew off the loaded world (arrows aren't saved)
        } else if ((hit = a.tick(dt, world, a.from_player ? mobs : with_player))) {
            const double fx = a.pos.x - a.vel.x, fz = a.pos.z - a.vel.z;
            if (a.item == EGG) {
                hatch(a);
            } else if (hit->kind == 0) {
                Animal& hit_animal = *animals[hit->index];
                hit_animal.hurt(ARROW_DAMAGE, fx, fz);
                if (hit_animal.kind == AnimalKind::Wolf && !hit_animal.tamed) hit_animal.angry = true;
            } else if (hit->kind == 1) {
                Monster& m = *monsters[hit->index];
                m.hurt(ARROW_DAMAGE, fx, fz);
                if (a.from_player) {
                    m.targeting = true;
                    alert_wolves(&m);
                }
            } else {
                hurt_player_by_monster(ARROW_DAMAGE, fx, fz);
            }
            remove = true;
        } else if (a.item == EGG && a.stuck) {
            hatch(a);
            remove = true;
        } else if (a.expired() || a.age > 120 || a.pos.y < -16 ||
                   std::hypot(a.pos.x - player.x, a.pos.z - player.z) > far_away) {
            remove = true;
        } else if (a.stuck && a.from_player && !vitals.dead() && std::hypot(a.pos.x - player.x, a.pos.z - player.z) < 1.2 &&
                   a.pos.y > player.y - 1 && a.pos.y < player.y + 2.5) {
            remove = inventory.add(ARROW, 1) == 0;  // walk over a stuck arrow to pick it up
        }
        it = remove ? arrows.erase(it) : it + 1;
    }
}

void Game::update_falling(double dt) {
    for (auto it = falling.begin(); it != falling.end();) {
        FallingBlock& f = **it;
        if (!world.find(chunk_of(ifloor(f.body.x)), chunk_of(ifloor(f.body.z))) || !f.tick(dt)) {
            ++it;
            continue;
        }
        // Landed: it becomes the block again, or drops as an item if it can't (on a torch, say).
        const glm::ivec3 c = f.cell();
        if (f.can_land(world)) renderer->rebuild(world.set_block(c.x, c.y, c.z, f.block));
        else spawn_drop(f.block, f.body.x, f.body.y + 0.3, f.body.z);
        it = falling.erase(it);
    }
}

// --- monsters ---------------------------------------------------------------------------------------

Monster* Game::spawn_monster(MobKind kind, double x, double y, double z, int size) {
    monsters.push_back(std::make_unique<Monster>(world, kind, x, y, z, monster_rng_, size));
    return monsters.back().get();
}

bool Game::can_spawn_monster(MobKind kind, double x, double y, double z, int size, bool natural) {
    const int bx = ifloor(x), by = ifloor(y), bz = ifloor(z);
    if (by < 1 || by >= HEIGHT - 2) return false;
    if (natural) {
        // Solid ground under it and room to stand, away from the player and the world spawn.
        if (!is_opaque(world.get_block(bx, by - 1, bz))) return false;
        for (int dy = 0; dy < 2; ++dy) {
            const BlockId b = world.get_block(bx, by + dy, bz);
            if (is_solid(b) || is_fluid(b)) return false;
        }
        const glm::dvec3 at(x, y, z);
        if (glm::length(at - glm::dvec3(player.x, player.y, player.z)) < SPAWN_MIN_DISTANCE) return false;
        if (glm::length(at - spawn) < SPAWN_MIN_DISTANCE) return false;
    }
    if (kind == MobKind::Slime) {
        if (natural && (!is_slime_chunk(level.seed, chunk_of(bx), chunk_of(bz)) || by >= SLIME_MAX_Y)) return false;
    } else {
        // Thunderstorms make it dark enough for monsters to spawn in the day (Beta).
        int darkness = static_cast<int>(current_sky().darkness);
        if (weather.is_thundering()) darkness = std::max(darkness, 10);
        if (!monster_spawn_light_ok(world.sky_light(bx, by, bz), light_at(world, bx, by, bz, darkness), monster_rng_))
            return false;
    }
    const MobInfo info = mob_info(kind, size);
    Body probe(world, x, y, z, info.half_width, info.height);
    if (probe.collides() || probe.check_water()) return false;
    // Not inside another mob.
    auto overlaps = [&](const Body& b) {
        return std::abs(b.x - x) < b.half + info.half_width && std::abs(b.z - z) < b.half + info.half_width &&
               b.y < y + info.height && y < b.y + b.height;
    };
    for (const auto& m : monsters)
        if (overlaps(m->body)) return false;
    for (const auto& a : animals)
        if (overlaps(a->body)) return false;
    return !overlaps(player);
}

void Game::spawn_monsters() {
    if (difficulty() == PEACEFUL) return;
    const int r = std::min(MONSTER_SPAWN_CHUNKS, renderer->render_distance);
    const int cap = monster_cap((2 * r + 1) * (2 * r + 1));
    int count = static_cast<int>(std::count_if(monsters.begin(), monsters.end(), [](const auto& m) { return m->alive(); }));
    if (count >= cap) return;
    Rng& R = monster_rng_;
    const int pcx = chunk_of(ifloor(player.x)), pcz = chunk_of(ifloor(player.z));
    // Beta: one try in every chunk around the player, every tick, while there are too few monsters.
    for (int cz = pcz - r; cz <= pcz + r; ++cz)
        for (int cx = pcx - r; cx <= pcx + r; ++cx) {
            Chunk* chunk = world.find(cx, cz);
            if (!chunk) continue;
            int x = cx * CHUNK + R.below(CHUNK), z = cz * CHUNK + R.below(CHUNK), y = R.below(std::min(HEIGHT - 2, chunk->max_y + 2));
            if (world.get_block(x, y, z) != AIR) continue;
            const MobKind kind = random_monster(R);
            // A small group, each a few steps from the last.
            for (int i = 0; i < 4; ++i) {
                x += R.below(6) - R.below(6);
                z += R.below(6) - R.below(6);
                if (!world.find(chunk_of(x), chunk_of(z))) break;
                const int size = kind == MobKind::Slime ? random_slime_size(R) : 1;
                if (!can_spawn_monster(kind, x + 0.5, y, z + 0.5, size, true)) continue;
                Monster* m = spawn_monster(kind, x + 0.5, y, z + 0.5, size);
                if (kind == MobKind::Spider && spawns_as_jockey(R)) {
                    Monster* rider = spawn_monster(MobKind::Skeleton, x + 0.5, y, z + 0.5);
                    rider->mount = m;
                    m->rider = rider;
                }
                if (++count >= cap) return;
            }
        }
}

void Game::tick_spawners() {
    Rng& R = monster_rng_;
    for (size_t i = 0; i < spawners.size();) {
        Spawner& s = spawners[i];
        if (!world.find(chunk_of(s.pos.x), chunk_of(s.pos.z))) {
            ++i;
            continue;
        }
        if (world.get_block(s.pos.x, s.pos.y, s.pos.z) != SPAWNER) {  // broken
            spawners[i] = spawners.back();
            spawners.pop_back();
            continue;
        }
        ++i;
        const glm::dvec3 c = glm::dvec3(s.pos) + 0.5;
        if (difficulty() == PEACEFUL || glm::length(c - glm::dvec3(player.x, player.y + 1, player.z)) > SPAWNER_RANGE) continue;
        if (--s.delay > 0) continue;
        s.delay = spawner_delay(R);
        const MobKind kind = spawner_mob(level.seed, s.pos);
        for (int n = 0; n < 4; ++n) {
            // Never more than six of its monsters around it.
            const int near = static_cast<int>(std::count_if(monsters.begin(), monsters.end(), [&](const auto& m) {
                return m->alive() && m->kind == kind && std::abs(m->body.x - c.x) < 8.5 && std::abs(m->body.y - c.y) < 4.5 &&
                       std::abs(m->body.z - c.z) < 8.5;
            }));
            if (near >= SPAWNER_MAX_NEARBY) break;
            const double x = c.x + (R.random() - R.random()) * 4, z = c.z + (R.random() - R.random()) * 4;
            const double y = s.pos.y + R.below(3) - 1;
            if (!can_spawn_monster(kind, x, y, z, 1, false)) continue;
            spawn_monster(kind, x, y, z);
            add_puffs(puffs, {x, y + 0.5, z}, 6, 0.4, 1.0, 0.25, R);
        }
    }
}

void Game::loot(Monster& m, std::vector<std::unique_ptr<Monster>>& born) {
    m.looted = true;
    if (m.exploded || m.vanish) return;
    for (ItemId item : mob_drops(m.kind, m.size, monster_rng_)) spawn_drop(item, m.body.x, m.body.y + 0.5, m.body.z);
    // A big slime breaks into smaller ones.
    for (int n = slime_split_count(m.size, monster_rng_); n > 0; --n) {
        const double ox = (monster_rng_.random() - 0.5) * m.size * 0.5, oz = (monster_rng_.random() - 0.5) * m.size * 0.5;
        auto child = std::make_unique<Monster>(world, MobKind::Slime, m.body.x + ox, m.body.y + 0.5, m.body.z + oz, monster_rng_,
                                               m.size / 2);
        if (child->body.collides()) {
            child->body.x = m.body.x;
            child->body.z = m.body.z;
        }
        child->targeting = m.targeting;
        born.push_back(std::move(child));
    }
}

void Game::update_monsters(double dt) {
    auto remove_at = [&](size_t i) {
        Monster& m = *monsters[i];
        if (m.rider) m.rider->mount = nullptr;
        if (m.mount) m.mount->rider = nullptr;
        if (target_monster == &m) target_monster = nullptr;
        for (const auto& a : animals)
            if (a->prey == &m) a->prey = nullptr;
        monsters.erase(monsters.begin() + static_cast<std::ptrdiff_t>(i));
    };
    if (difficulty() == PEACEFUL) {
        // Peaceful: monsters vanish (Beta).
        while (!monsters.empty()) remove_at(monsters.size() - 1);
        return;
    }
    MonsterSenses senses;
    senses.player = {player.x, player.y, player.z};
    senses.player_height = player.height;
    senses.player_targetable = !creative && !vitals.dead();
    senses.sky_darkness = static_cast<int>(current_sky().darkness);
    senses.raining = weather.is_raining();
    const double far_away = (renderer->render_distance + 3) * CHUNK;
    std::vector<std::unique_ptr<Monster>> born;
    for (size_t i = 0; i < monsters.size();) {
        Monster& m = *monsters[i];
        if (m.vanish) {
            remove_at(i);
            continue;
        }
        // Monsters on chunks that aren't generated yet wait, so they don't fall through the world.
        if (!world.find(chunk_of(ifloor(m.body.x)), chunk_of(ifloor(m.body.z)))) {
            ++i;
            continue;
        }
        const double dist = std::hypot(m.body.x - player.x, m.body.z - player.z);
        // Beta's despawning: at once when very far, now and then when out of sight range a while.
        if (dist < DESPAWN_SAFE_DISTANCE) m.age = 0;
        const bool despawn =
            dist > DESPAWN_DISTANCE || (m.age > 30 && m.alive() && monster_rng_.random() < dt * 20 / 800);
        if (despawn && !m.mount) {
            if (m.rider) m.rider->vanish = true;
            remove_at(i);
            continue;
        }
        if (dist > far_away && !m.mount) {
            // Too far to simulate: keep it with its chunk until the player comes back.
            if (m.alive()) stash_entity(monster_record(m));
            if (m.rider) m.rider->vanish = true;
            remove_at(i);
            continue;
        }
        MonsterActions act;
        const bool done = m.tick(dt, world, senses, monster_rng_, act);
        if (act.melee > 0 && hurt_player_by_monster(act.melee, m.body.x, m.body.z)) alert_wolves(&m);
        if (act.arrow_from) {
            auto arrow = std::make_unique<Arrow>(*act.arrow_from, act.arrow_vel);
            arrow->shooter = &m;
            arrow->from_player = false;
            arrows.push_back(std::move(arrow));
        }
        if (act.hiss) play_sound(Sound::Fuse, {m.body.x, m.body.y + 1, m.body.z});
        if (act.explode) {
            // The creeper is gone in its own blast.
            m.exploded = true;
            m.health = 0;
            m.looted = true;
            const glm::dvec3 at(m.body.x, m.body.y, m.body.z);
            const double power = m.powered ? CHARGED_CREEPER_POWER : CREEPER_POWER;
            remove_at(i);
            explode(at, power, true);
            continue;
        }
        if (!m.alive() && !m.looted) loot(m, born);
        if (done) {
            add_puffs(puffs, {m.body.x, m.body.y + m.body.height * 0.5, m.body.z}, 8, m.body.half + 0.2, 1.2, 0.25, monster_rng_);
            remove_at(i);
            continue;
        }
        ++i;
    }
    for (auto& b : born) monsters.push_back(std::move(b));
}

void Game::prime_tnt(glm::ivec3 pos, int fuse_ticks) {
    if (world.get_block(pos.x, pos.y, pos.z) == TNT) renderer->rebuild(world.set_block(pos.x, pos.y, pos.z, AIR));
    tnts.push_back(std::make_unique<PrimedTnt>(world, pos.x + 0.5, pos.y, pos.z + 0.5, fuse_ticks, rng));
    play_sound(Sound::Fuse, glm::dvec3(pos) + 0.5);
}

void Game::play_sound(Sound sound, glm::dvec3 pos, float pitch) {
    sounds.push_back({sound, pos, pitch});
    ++sounds_started[static_cast<int>(sound)];
}

void Game::update_tnts(double dt) {
    for (size_t i = 0; i < tnts.size();) {
        PrimedTnt& t = *tnts[i];
        if (!world.find(chunk_of(ifloor(t.body.x)), chunk_of(ifloor(t.body.z))) || !t.tick(dt)) {
            ++i;
            continue;
        }
        const glm::dvec3 at(t.body.x, t.body.y + 0.49, t.body.z);
        tnts.erase(tnts.begin() + static_cast<std::ptrdiff_t>(i));
        explode(at, TNT_POWER, false);
    }
}

void Game::explode(glm::dvec3 center, double power, bool from_monster) {
    // Beta pitches each blast a little differently (picked from the frame number, so random
    // numbers the rest of the game uses stay the same).
    play_sound(Sound::Explosion, center, static_cast<float>(0.75 + std::fmod(frame * 0.618034, 1.0) * 0.25));
    add_puffs(puffs, center, static_cast<int>(power * 8), power * 0.6, power * 1.6, 0.5 + power * 0.15, rng);
    const double reach = power * 2;

    // First everything near is hurt and pushed, with the blocks still standing (Beta's order).
    auto hit = [&](glm::dvec3 at, glm::dvec3 lo, glm::dvec3 hi, glm::dvec3& push) {
        const glm::dvec3 d = at - center;
        const double dist = glm::length(d);
        if (dist > reach) return 0;
        const double exposure = explosion_exposure(world, center, lo, hi);
        const double p = explosion_push(power, dist, exposure);
        push = (dist > 1e-6 ? d / dist : glm::dvec3(0, 1, 0)) * p * 16.0;
        return explosion_damage(power, dist, exposure);
    };
    glm::dvec3 push(0);
    if (!creative && !vitals.dead()) {
        const Body& b = player;
        // Beta measures to the player's eyes.
        const int dmg = hit({b.x, b.y + EYE_HEIGHT, b.z}, {b.x - b.half, b.y, b.z - b.half}, {b.x + b.half, b.y + b.height, b.z + b.half}, push);
        if (dmg > 0) {
            if (from_monster) hurt_player_by_monster(dmg, center.x, center.z);
            else hurt_player(dmg);
            player.vx += push.x;
            player.vy += push.y;
            player.vz += push.z;
        }
    }
    for (const auto& a : animals) {
        if (!a->alive()) continue;
        const Body& b = a->body;
        if (int dmg = hit({b.x, b.y, b.z}, {b.x - b.half, b.y, b.z - b.half}, {b.x + b.half, b.y + b.height, b.z + b.half}, push))
            a->hurt(dmg, center.x, center.z);
    }
    for (const auto& m : monsters) {
        if (!m->alive()) continue;
        Body& b = m->body;
        const int dmg = hit({b.x, b.y, b.z}, {b.x - b.half, b.y, b.z - b.half}, {b.x + b.half, b.y + b.height, b.z + b.half}, push);
        if (dmg <= 0) continue;
        m->hurt(dmg, center.x, center.z, false);
        m->knock_x += push.x;
        m->knock_z += push.z;
        b.vy += push.y;
    }
    for (const auto& t : tnts) {
        Body& b = t->body;
        if (hit({b.x, b.y, b.z}, {b.x - b.half, b.y, b.z - b.half}, {b.x + b.half, b.y + b.height, b.z + b.half}, push)) {
            b.vx += push.x;
            b.vy += push.y;
            b.vz += push.z;
        }
    }
    // Dropped items caught in a blast are destroyed (Beta's items have 5 health).
    drops.erase(std::remove_if(drops.begin(), drops.end(),
                               [&](const auto& d) {
                                   const Body& b = d->body;
                                   return hit({b.x, b.y, b.z}, {b.x - b.half, b.y, b.z - b.half},
                                              {b.x + b.half, b.y + b.height, b.z + b.half}, push) >= 5;
                               }),
                drops.end());

    // Then the blocks go. 30% of them drop; TNT in the blast is lit with a short fuse.
    std::vector<glm::ivec3> cells = explosion_blocks(world, center, power, rng);
    std::vector<ChunkPos> dirty;
    auto clear = [&](glm::ivec3 c) {
        for (ChunkPos p : world.set_block(c.x, c.y, c.z, AIR))
            if (std::find(dirty.begin(), dirty.end(), p) == dirty.end()) dirty.push_back(p);
    };
    std::vector<glm::ivec3> tnt_cells;
    for (glm::ivec3 c : cells) {
        const BlockId b = world.get_block(c.x, c.y, c.z);
        if (b == AIR) continue;
        if (b == TNT) {
            tnt_cells.push_back(c);
            continue;
        }
        if (is_bed(b)) {
            const glm::ivec3 o = bed_other_half(c, b);
            if (sleeping_in && (*sleeping_in == c || *sleeping_in == o)) wake_up(false);
            if (is_bed(world.get_block(o.x, o.y, o.z))) clear(o);
            if (bed_is_head(b) && rng.random() < EXPLOSION_DROP_CHANCE) spawn_drop(BED, c.x + 0.5, c.y + 0.3, c.z + 0.5);
            clear(c);
            continue;
        }
        if (is_furnace(b)) drop_furnace_contents(c);
        clear(c);
        const BlockInfo* info = block_info(b);
        if (info && info->drop && rng.random() < EXPLOSION_DROP_CHANCE) {
            const int n = *info->drop == b ? 1 : rng.randint(info->drop_min, info->drop_max);
            for (int i = 0; i < n; ++i) spawn_drop(*info->drop, c.x + 0.5, c.y + 0.3, c.z + 0.5);
        }
    }
    // Torches left hanging on nothing fall off.
    for (glm::ivec3 c : cells)
        for (const FaceDef& f : face_defs()) {
            const glm::ivec3 n = c + f.normal;
            const BlockId b = world.get_block(n.x, n.y, n.z);
            if (is_torch(b) && world.get_block(n.x + torch_support(b).x, n.y + torch_support(b).y, n.z + torch_support(b).z) == AIR) {
                clear(n);
                spawn_drop(TORCH, n.x + 0.5, n.y + 0.3, n.z + 0.5);
            }
        }
    if (!dirty.empty()) renderer->rebuild(dirty);
    for (glm::ivec3 c : tnt_cells) prime_tnt(c, rng.below(TNT_FUSE_TICKS / 4) + TNT_FUSE_TICKS / 8);
}
