#include "autotest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <GLFW/glfw3.h>

#include "app.h"
#include "game.h"
#include "menu.h"

namespace {
constexpr double PI = 3.14159265358979323846;
double degrees(double r) { return r * 180.0 / PI; }
}  // namespace

AutoTest::AutoTest(std::string out_dir, uint64_t seed) : out_(std::move(out_dir)), seed_(seed) {
    // Start from nothing, so every run makes the same world.
    std::filesystem::create_directories(out_);
    std::filesystem::remove_all(std::filesystem::path(out_) / "saves");
    std::filesystem::remove(std::filesystem::path(out_) / "options.txt");
}

void AutoTest::log(const std::string& msg) {
    std::printf("[autotest] %s\n", msg.c_str());
    std::fflush(stdout);
    lines_.push_back(msg);
}

void AutoTest::check(const std::string& name, bool ok) {
    if (!ok) ++failures_;
    log((ok ? "PASS " : "FAIL ") + name);
}

void AutoTest::shot(App& app, const std::string& name) {
    if (Game* g = app.game.get()) {
        // Finish streaming everything in view first, so screenshots don't depend on timing.
        g->renderer->load_all(g->player.x, g->player.z, g->renderer->render_distance);
        g->update_target();
    }
    app.draw();
    bool ok = app.screenshot(std::filesystem::path(out_) / name);
    log((ok ? "screenshot " : "could not save screenshot ") + name);
}

void AutoTest::face(Game& g, double x, double z, double pitch) {
    g.yaw = degrees(std::atan2(x - g.player.x, z - g.player.z));
    g.pitch = pitch;
}

void AutoTest::finish(App& app) {
    log(failures_ ? std::to_string(failures_) + " check(s) failed" : "all checks passed");
    std::ofstream report(std::filesystem::path(out_) / "report.txt");
    for (const auto& line : lines_) report << line << "\n";
    app.quit = true;
}

void AutoTest::operator()(App& app) {
    if (phase_ == 0) menus(app);
    else if (phase_ == 1 && app.game) play(app, *app.game);
    else if (phase_ >= 2) reload(app);
}

void AutoTest::menus(App& app) {
    const long f = app.frame;
    if (f == 2) shot(app, "0_title.png");
    if (f == 3) app.open(options_screen(app, [&app] { return title_screen(app); }));
    if (f == 5) shot(app, "0_options.png");
    if (f == 6) app.open(create_world_screen(app, "Autotest", std::to_string(seed_)));
    if (f == 8) shot(app, "0_create_world.png");
    if (f == 9) {
        check("world created from the menu", app.create_world("Autotest", std::to_string(seed_)));
        phase_ = 1;
    }
}

void AutoTest::play(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    auto dist_to = [&](const Cow* c) { return std::hypot(c->body.x - p.x, c->body.z - p.z); };
    auto cow_alive_in_world = [&] {
        return std::any_of(g.cows.begin(), g.cows.end(), [&](const auto& c) { return c.get() == cow_; });
    };

    if (f == 2) g.hud->show_help = false;
    if (f == 120) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "seed %llu, chunks meshed %zu, cows %zu", static_cast<unsigned long long>(g.level.seed),
                      g.renderer->mesh_count(), g.cows.size());
        log(buf);
        shot(app, "1_spawn.png");
        check("cows spawned near the player", !g.cows.empty());
        if (!g.cows.empty()) {
            cow_ = std::min_element(g.cows.begin(), g.cows.end(),
                                    [&](const auto& a, const auto& b) { return dist_to(a.get()) < dist_to(b.get()); })
                       ->get();
            cow_->state = Cow::State::Idle;
            cow_->state_timer = 1000;
            cow_->graze = false;
            // Walk the player up to the cow and look at it.
            double d = dist_to(cow_);
            p.x += (cow_->body.x - p.x) * (d - 3) / d;
            p.z += (cow_->body.z - p.z) * (d - 3) / d;
            p.y = g.world.surface_y(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.z)));
            cow_start_x_ = cow_->body.x;
            cow_start_z_ = cow_->body.z;
        }
    }
    if (f > 120 && f < 200 && cow_) {
        double d = dist_to(cow_);
        face(g, cow_->body.x, cow_->body.z, degrees(std::atan2(p.y + EYE_HEIGHT - (cow_->body.y + 0.8), d)));
    }
    if (f == 160 && cow_) {
        shot(app, "2_cow.png");
        check("cow is targetable by the crosshair", g.target_cow == cow_);
    }
    if (f == 200 && cow_) {
        double moved = std::hypot(cow_->body.x - cow_start_x_, cow_->body.z - cow_start_z_);
        char buf[96];
        std::snprintf(buf, sizeof buf, "cow wandered %.2f blocks; on ground=%s", moved, cow_->body.on_ground ? "True" : "False");
        log(buf);
        for (int i = 0; i < 5; ++i) cow_->hurt(2, p.x, p.z);
        check("cow dies after 5 punches", !cow_->alive());
    }
    if (f == 260) {
        check("dead cow removed", !cow_alive_in_world());
        cow_ = nullptr;
        bool any_beef = g.inventory.count(BEEF) > 0;
        for (auto& d : g.drops)
            if (d->item == BEEF) {
                any_beef = true;
                d->body.x = p.x;
                d->body.y = p.y + 0.5;
                d->body.z = p.z;
            }
        check("cow dropped beef", any_beef);
    }
    if (f == 290) {
        check("beef picked up", g.inventory.count(BEEF) > 0);
        g.vitals.hunger = 8;
        int slot = g.inventory.find(BEEF);
        if (slot >= 0) {
            g.inventory.selected = slot;
            g.use_item();
        }
        check("eating beef restores hunger (8 -> " + std::to_string(g.vitals.hunger) + ")", g.vitals.hunger > 8);
        g.pitch = 70;
    }
    if (f == 300) {
        check("looking at a block", g.target.has_value());
        if (g.target) {
            broken_ = g.target->block;
            broken_block_ = g.world.get_block(broken_->x, broken_->y, broken_->z);
            g.break_block(*broken_, broken_block_);
            check("block removed from world", g.world.get_block(broken_->x, broken_->y, broken_->z) == AIR);
        }
    }
    if (f == 340 && broken_) {
        auto drop = block_info(broken_block_) ? block_info(broken_block_)->drop : std::nullopt;
        check("broken block collected into inventory", !drop || g.inventory.count(*drop) > 0);
        if (drop && g.inventory.count(*drop)) {
            g.inventory.selected = g.inventory.find(*drop);
            g.update_target();
            std::optional<glm::ivec3> place = g.target ? g.target->before : std::nullopt;
            g.use_item();
            check("block placed back", place && g.world.get_block(place->x, place->y, place->z) == *drop);
        }
        g.pitch = 10;
    }
    if (f == 360) {
        hp_before_fall_ = g.vitals.health;
        p.y += 12;
        p.vy = 0;
        g.fall_peak = p.y;
    }
    if (f == 460) {
        check("fall damage (health " + std::to_string(hp_before_fall_) + " -> " + std::to_string(g.vitals.health) + ")",
              g.vitals.health < hp_before_fall_);
        shot(app, "3_after_fall.png");
        g.creative = true;
        g.flying = true;
        p.y += 25;
        g.pitch = 35;
    }
    if (f == 520) {
        shot(app, "4_overview.png");
        g.flying = false;  // land (creative, so no fall damage) for the third-person picture
    }
    if (f == 615) {
        g.third_person = true;  // the model is added to the scene in the next update
        g.pitch = 20;
    }
    if (f == 620) {
        shot(app, "5_third_person.png");
        g.third_person = false;
        g.debug_screen = true;
        shot(app, "6_debug.png");
        g.debug_screen = false;
    }
    if (f == 630) {
        app.pause();
        check("Escape menu pauses the game", g.paused && app.screen());
        shot(app, "7_pause.png");
    }
    if (f == 640) {
        // Change something, then save and quit to the title.
        marker_ = {static_cast<int>(std::floor(p.x)) + 2, static_cast<int>(std::floor(p.y)) + 1, static_cast<int>(std::floor(p.z))};
        g.renderer->rebuild(g.world.set_block(marker_.x, marker_.y, marker_.z, BRICK));
        g.inventory.add(PLANKS, 7);
        saved_player_ = g.player_data();
        saved_cows_ = g.cows.size();
        app.save_and_quit_to_title();
        check("Save and Quit to Title closes the world", !app.game);
        phase_ = 2;
        phase_start_ = app.frame;
    }
}

void AutoTest::reload(App& app) {
    const long f = app.frame - phase_start_;
    if (phase_ == 2 && f == 2) app.open(select_world_screen(app));
    if (phase_ == 2 && f == 4) {
        auto worlds = list_worlds(app.saves_dir);
        check("the world is in the world list", worlds.size() == 1 && worlds[0].name == "Autotest");
        shot(app, "8_world_list.png");
        if (worlds.empty()) {
            finish(app);
            return;
        }
        check("the world loads again", app.play_world(worlds[0].folder));
        phase_ = 3;
        phase_start_ = app.frame;
    }
    if (phase_ == 3 && f == 3 && app.game) {
        Game& g = *app.game;
        check("blocks changed before quitting are still there",
              g.world.get_block(marker_.x, marker_.y, marker_.z) == BRICK);
        PlayerData now = g.player_data();
        check("player position, health, hunger and inventory are restored",
              std::abs(now.pos.x - saved_player_.pos.x) < 1e-6 && std::abs(now.pos.z - saved_player_.pos.z) < 1e-6 &&
                  now.health == saved_player_.health && now.hunger == saved_player_.hunger &&
                  now.inventory.slots == saved_player_.inventory.slots && now.creative == saved_player_.creative);
        check("cows near the player are back (" + std::to_string(g.cows.size()) + " of " + std::to_string(saved_cows_) + ")",
              !g.cows.empty());
        shot(app, "9_reloaded.png");
        finish(app);
    }
}
