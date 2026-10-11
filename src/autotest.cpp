#include "autotest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

#include "app.h"
#include "farming.h"
#include "bed.h"
#include "building.h"
#include "chest.h"
#include "game.h"
#include "inventory_screen.h"
#include "menu.h"
#include "rails.h"
#include "tools.h"

namespace {
constexpr double PI = 3.14159265358979323846;
double degrees(double r) { return r * 180.0 / PI; }

// Nearest dry land (searching outwards in a square spiral) where the biome is one of `wanted`.
std::optional<glm::ivec2> find_biome(const TerrainGenerator& gen, int x, int z, std::initializer_list<Biome> wanted) {
    for (int r = 0; r < 6000; r += 48)
        for (int i = -r; i <= r; i += 48)
            for (glm::ivec2 d : {glm::ivec2(i, -r), glm::ivec2(i, r), glm::ivec2(-r, i), glm::ivec2(r, i)})
                for (Biome b : wanted)
                    if (gen.biome_at(x + d.x, z + d.y) == b && gen.density(x + d.x, SEA_LEVEL + 4, z + d.y) > 0)
                        return glm::ivec2(x + d.x, z + d.y);
    return std::nullopt;
}
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
    auto dist_to = [&](const Animal* c) { return std::hypot(c->body.x - p.x, c->body.z - p.z); };
    auto cow_alive_in_world = [&] {
        return std::any_of(g.animals.begin(), g.animals.end(), [&](const auto& c) { return c.get() == cow_; });
    };

    if (f == 2) g.hud->show_help = false;
    if (f == 120) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "seed %llu, chunks meshed %zu, animals %zu", static_cast<unsigned long long>(g.level.seed),
                      g.renderer->mesh_count(), g.animals.size());
        log(buf);
        shot(app, "1_spawn.png");
        for (const auto& a : g.animals)
            if (a->kind == AnimalKind::Cow && (!cow_ || dist_to(a.get()) < dist_to(cow_))) cow_ = a.get();
        check("cows spawned near the player", cow_ != nullptr);
        if (cow_) {
            cow_->state = Animal::State::Idle;
            cow_->state_timer = 1000;
            cow_->graze = false;
            // Walk the player up to the cow and look at it.
            double d = dist_to(cow_);
            p.x += (cow_->body.x - p.x) * (d - 3) / d;
            p.z += (cow_->body.z - p.z) * (d - 3) / d;
            // Stand at the cow's height (within hitting reach even on a slope): room to stand and ground underfoot.
            p.y = std::floor(cow_->body.y);
            int bx = static_cast<int>(std::floor(p.x)), by = static_cast<int>(p.y), bz = static_cast<int>(std::floor(p.z));
            for (int dy = 0; dy < 2; ++dy) g.renderer->rebuild(g.world.set_block(bx, by + dy, bz, AIR));
            if (!g.world.is_solid_at(bx, by - 1, bz)) g.renderer->rebuild(g.world.set_block(bx, by - 1, bz, DIRT));
            cow_start_x_ = cow_->body.x;
            cow_start_z_ = cow_->body.z;
        }
    }
    if (f > 120 && f < 200 && cow_) {
        double d = dist_to(cow_);
        face(g, cow_->body.x, cow_->body.z, degrees(std::atan2(p.y + EYE_HEIGHT - (cow_->body.y + 0.8), d)));
    }
    if ((f == 155 || f == 159) && cow_) {
        // Leaves or a hill between the player and the cow would hide it: clear the line of sight,
        // all the way to the middle of the cow (that point is inside the cow, so never in a block),
        // and again just before the check in case the cow shuffled.
        glm::vec3 e = g.eye(), to(cow_->body.x, cow_->body.y + 0.8, cow_->body.z);
        float d = glm::length(to - e);
        glm::vec3 dir = (to - e) / d;
        // Up to where the ray meets the cow's box (a leaf just in front of it would still hide it).
        const float reach = cow_->ray_hit(e, dir, d + 1).value_or(d - 0.7f);
        while (auto hit = g.world.raycast(e, dir, reach)) {
            log("cleared " + item_name(g.world.get_block(hit->block.x, hit->block.y, hit->block.z)) + " in front of the cow");
            g.renderer->rebuild(g.world.set_block(hit->block.x, hit->block.y, hit->block.z, AIR));
        }
    }
    if (f == 160 && cow_) {
        shot(app, "2_cow.png");
        check("cow is targetable by the crosshair", g.target_animal == cow_);
        if (g.target_animal != cow_) {
            auto d = cow_->ray_hit(g.eye(), g.forward(), 10.0f);
            char buf[200];
            std::snprintf(buf, sizeof buf, "  cow ray distance %.2f, crosshair block %s at %.2f, other animal %s",
                          d ? *d : -1.0f,
                          g.target ? item_name(g.world.get_block(g.target->block.x, g.target->block.y, g.target->block.z)).c_str() : "none",
                          g.target ? g.target->distance : -1.0f, g.target_animal ? "yes" : "no");
            log(buf);
        }
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
            if (d->stack.item == BEEF) {
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
            log("breaking " + item_name(broken_block_));
            g.break_block(*broken_, broken_block_);
            check("block removed from world", g.world.get_block(broken_->x, broken_->y, broken_->z) == AIR);
        }
    }
    if (f == 320)  // the drop may have bounced into water or down a slope; bring it over like the beef
        for (auto& d : g.drops) {
            d->body.x = p.x;
            d->body.y = p.y + 0.5;
            d->body.z = p.z;
        }
    if (f == 340 && broken_) {
        auto drop = harvest_drop(g.inventory.selected_item().value_or(AIR), broken_block_);
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
    if (f == 344) crafting(app, g);
    if (f >= 345 && f <= 349) furnaces(app, g);
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
    if (f == 520) shot(app, "4_overview.png");
    if (f >= 525 && f < 600) tour(app, g);
    if (f >= 600 && f <= 614) tools(app, g);
    if (f == 621) g.inventory.armor = {};  // armor off again after the third-person picture, so later damage checks see full hits
    if (f == 615) {
        g.third_person = true;  // the model is added to the scene in the next update
        g.pitch = 20;
    }
    if (f == 620) {
        shot(app, "5_third_person.png");
        g.third_person = false;  // the model leaves the scene in the next update
    }
    if (f == 622) {
        g.debug_screen = true;
        shot(app, "6_debug.png");
        g.debug_screen = false;
        // Light: a torch and a block of glowstone on the ground a few blocks ahead.
        int x = static_cast<int>(std::floor(p.x)), z = static_cast<int>(std::floor(p.z)) + 4;
        torch_ = {x, g.world.surface_y(x, z), z};
        glowstone_ = {x + 4, g.world.surface_y(x + 4, z + 2), z + 2};
        int before = g.world.block_light(torch_.x + 2, torch_.y, torch_.z);
        g.renderer->rebuild(g.world.set_block(torch_.x, torch_.y, torch_.z, TORCH));
        g.renderer->rebuild(g.world.set_block(glowstone_.x, glowstone_.y, glowstone_.z, GLOWSTONE));
        int after = g.world.block_light(torch_.x + 2, torch_.y, torch_.z);
        check("a torch lights the ground around it (2 blocks away: " + std::to_string(before) + " -> " +
                  std::to_string(after) + ")",
              g.world.block_light(torch_.x, torch_.y, torch_.z) == 14 && after >= 12);
        check("glowstone gives light 15", g.world.block_light(glowstone_.x, glowstone_.y + 1, glowstone_.z) == 14);
        day_time_ = g.level.time;
        g.level.time = 18000;  // midnight
        face(g, torch_.x + 0.5, torch_.z + 0.5, 25);
    }
    if (f == 624) {
        shot(app, "6_night_torch.png");
        g.debug_screen = true;
        shot(app, "6_night_debug.png");
        g.debug_screen = false;
        g.renderer->rebuild(g.world.set_block(torch_.x, torch_.y, torch_.z, AIR));
        // A torch on the side of the glowstone falls off when the glowstone is broken.
        glm::ivec3 wall = glowstone_ + glm::ivec3(1, 0, 0);
        g.renderer->rebuild(g.world.set_block(wall.x, wall.y, wall.z, TORCH_WALL_WEST));
        g.break_block(glowstone_, GLOWSTONE);
        check("a wall torch falls off when its block is broken", g.world.get_block(wall.x, wall.y, wall.z) == AIR);
        check("removing the torch and glowstone takes their light away",
              g.world.block_light(torch_.x + 2, torch_.y, torch_.z) == 0);
        g.level.time = 12500;  // sunset
        g.yaw = -90;                 // look west, at the setting sun
        g.pitch = -8;
        ground_y_ = p.y;
        p.y += 15;  // above the trees
        g.flying = true;
    }
    if (f == 626) {
        shot(app, "6_sunset.png");
        g.level.time = day_time_;
        p.y = ground_y_;
        g.flying = false;
    }
    if (f >= 627 && f < 700) weather_and_beds(app, g);
    if (f >= 700 && f < 1194) monsters(app, g);
    if (f >= 1194 && f < 1200) hand(app, g);
    if (f >= 1200 && f < 1350) fluids_and_fire(app, g);
    if (f >= 1350 && f < 1450) farming(app, g);
    if (f >= 1380 && f < 1400) building(app, g);
    if (f >= 1450 && f < 1650) animals(app, g);
    if (f >= 1650 && f < 1720) transport(app, g);
    if (f == 1720) {
        app.pause();
        check("Escape menu pauses the game", g.paused && app.screen());
        shot(app, "7_pause.png");
    }
    if (f == 1730) {
        // Change something, then save and quit to the title.
        marker_ = {static_cast<int>(std::floor(p.x)) + 2, static_cast<int>(std::floor(p.y)) + 1, static_cast<int>(std::floor(p.z))};
        g.renderer->rebuild(g.world.set_block(marker_.x, marker_.y, marker_.z, BRICK));
        g.inventory.add(PLANKS, 7);
        // A furnace on top of it, half way through smelting sand.
        g.renderer->rebuild(g.world.set_block(marker_.x, marker_.y + 1, marker_.z, furnace_block(1, true)));
        Furnace& fur = g.world.furnace(marker_ + glm::ivec3(0, 1, 0));
        fur = Furnace{Stack{SAND, 5}, Stack{COAL, 2}, Stack{GLASS, 3}, 1000, 1600, 50};
        saved_player_ = g.player_data();
        saved_raining_ = g.weather.state.raining;
        saved_animals_ = g.animals.size();
        // A zombie next to the player, to see it again after loading.
        g.monsters.clear();
        g.target_monster = nullptr;
        g.spawn_monster(MobKind::Zombie, p.x + 3, p.y + 1, p.z);
        app.save_and_quit_to_title();
        check("Save and Quit to Title closes the world", !app.game);
        phase_ = 2;
        phase_start_ = app.frame;
    }
}

void AutoTest::weather_and_beds(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    World& w = g.world;
    Rng rng(9);
    auto columns = [&] {
        glm::vec3 e = g.eye();
        return precipitation_columns(w, e, 10, g.weather.rain_strength());
    };
    if (f == 627) {
        // Rain: a storm that has been going a while (full strength), clear of thunder.
        g.weather = Weather(WeatherState{true, false, 100000, 100000});
        g.yaw = 30;
        g.pitch = 5;
    }
    if (f == 629) {
        auto cols = columns();
        float dark = g.current_sky().darkness;
        check("rain falls around the player (" + std::to_string(cols.size()) + " columns) and dims the day (darkness " +
                  std::to_string(dark) + ")",
              !cols.empty() && !cols[0].snow && dark > 3 && dark < 4);
        shot(app, "m5_rain.png");
        // A thunderstorm, with a bolt striking 14 blocks ahead.
        g.weather = Weather(WeatherState{true, true, 100000, 100000});
        double yr = g.yaw * PI / 180;
        int bx = static_cast<int>(std::floor(p.x + std::sin(yr) * 14)), bz = static_cast<int>(std::floor(p.z + std::cos(yr) * 14));
        auto top = w.rain_height(bx, bz);
        g.bolts.emplace_back(glm::dvec3(bx + 0.5, top ? *top : p.y, bz + 0.5), rng);
        g.lightning_flash = LIGHTNING_FLASH_TICKS;
    }
    if (f == 631) {
        check("a thunderstorm is going and the bolt shows", g.weather.is_thundering() && !g.bolts.empty() && g.bolts[0].visible());
        shot(app, "m5_thunder.png");
        g.bolts.clear();
        // Snow: the same storm over a cold biome.
        auto at = find_biome(w.generator(), static_cast<int>(p.x), static_cast<int>(p.z),
                             {Biome::TAIGA, Biome::TUNDRA, Biome::ICE_DESERT});
        check("found a cold biome for snow", at.has_value());
        if (at) {
            g.weather = Weather(WeatherState{true, false, 100000, 100000});
            p.x = at->x + 0.5;
            p.z = at->y + 0.5;
            p.y = w.surface_y(at->x, at->y);
            p.vy = 0;
            g.flying = true;
            g.yaw = 30;
            g.pitch = 10;
        }
    }
    if (f == 634) {
        shot(app, "m5_snow.png");
        auto cols = columns();
        int snow = static_cast<int>(std::count_if(cols.begin(), cols.end(), [](const PrecipColumn& c) { return c.snow; }));
        check("snow falls in the cold biome (" + std::to_string(snow) + " of " + std::to_string(cols.size()) + " columns)",
              snow > static_cast<int>(cols.size()) / 2);
    }
    if (f == 636) {
        // A bed: find four blocks of flat open ground in a row near the start, stand at one end
        // facing east and right-click the ground two blocks ahead.
        g.weather.clear();
        g.flying = false;
        g.creative = false;
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;  // noon
        std::optional<glm::ivec3> spot;
        int x0 = static_cast<int>(tour_start_.x), z0 = static_cast<int>(tour_start_.z);
        for (int r = 0; r < 40 && !spot; ++r)
            for (int dx = -r; dx <= r && !spot; ++dx)
                for (int dz = -r; dz <= r && !spot; ++dz) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                    int x = x0 + dx, z = z0 + dz, sy = w.surface_y(x, z);
                    bool ok = sy > SEA_LEVEL;
                    for (int k = -2; k <= 1 && ok; ++k)
                        ok = w.surface_y(x + k, z) == sy && is_opaque(w.get_block(x + k, sy - 1, z)) &&
                             w.get_block(x + k, sy, z) == AIR && w.get_block(x + k, sy + 1, z) == AIR &&
                             w.get_block(x + k, sy + 2, z) == AIR;
                    if (ok) spot = glm::ivec3(x, sy, z);
                }
        check("found flat ground for a bed", spot.has_value());
        if (!spot) return;
        p.x = spot->x - 1.5;
        p.y = spot->y;
        p.z = spot->z + 0.5;
        p.vx = p.vy = p.vz = 0;
        g.yaw = 90;
        g.pitch = degrees(std::atan2(EYE_HEIGHT, 2.0));
        g.inventory.slots[g.inventory.selected] = Stack{BED, 1};
        g.update_target();
        g.use_item();
        bed_foot_ = *spot;
        bed_head_ = *spot + glm::ivec3(1, 0, 0);
        bed_placed_ = w.get_block(bed_foot_.x, bed_foot_.y, bed_foot_.z) == BED_FOOT_EAST &&
                      w.get_block(bed_head_.x, bed_head_.y, bed_head_.z) == BED_HEAD_EAST;
        check("right-clicking the ground with a bed places both halves, facing the way the player looks", bed_placed_);
        check("placing the bed used it up", !g.inventory.slots[g.inventory.selected]);
        g.try_sleep(bed_foot_);
        check("you can't sleep at noon", !g.sleeping_in);
        // Step back and look at it.
        p.x -= 1.5;
        p.z -= 1.5;
        face(g, bed_head_.x, bed_head_.z + 0.5, 30);
    }
    if (!bed_placed_) return;
    if (f == 638) {
        shot(app, "m5_bed.png");
        g.level.time += 12000;  // midnight
        g.try_sleep(bed_foot_);
        check("at night, right-clicking the bed lies you down in it", g.sleeping_in == bed_head_);
        g.sleep_ticks = SLEEP_TICKS / 2;
    }
    if (f == 640) {
        shot(app, "m5_sleeping.png");
        g.sleep_ticks = SLEEP_TICKS - 1;
        g.weather = Weather(WeatherState{true, false, 100000, 100000});
    }
    if (f == 645) {
        check("after sleeping the player gets up in the morning, by the bed",
              !g.sleeping_in && g.level.time % DAY_TICKS < 100 &&
                  std::hypot(p.x - (bed_head_.x + 0.5), p.z - (bed_head_.z + 0.5)) < 2.5);
        check("sleeping clears the rain and sets the spawn point", !g.weather.state.raining && g.bed_spawn == bed_head_);
        face(g, bed_head_.x + 0.5, bed_head_.z + 0.5, 30);
    }
    if (f == 647) {
        shot(app, "m5_morning.png");
        p.x += 40;
        g.respawn();
        check("respawning puts you by your bed",
              std::hypot(p.x - (bed_head_.x + 0.5), p.z - (bed_head_.z + 0.5)) < 2.5 && std::abs(p.y - bed_head_.y) < 0.01);
        size_t drops = g.drops.size();
        g.break_block(bed_foot_, w.get_block(bed_foot_.x, bed_foot_.y, bed_foot_.z));
        check("breaking one half removes the whole bed and drops it",
              w.get_block(bed_head_.x, bed_head_.y, bed_head_.z) == AIR && g.drops.size() == drops + 1 &&
                  g.drops.back()->stack.item == BED);
        g.respawn();
        check("with the bed gone you respawn at the world spawn",
              !g.bed_spawn && std::hypot(p.x - g.spawn.x, p.z - g.spawn.z) < 0.01);
        // Lightning hurts what it hits (not in creative).
        int before = g.vitals.health;
        g.strike({p.x + 1, p.y, p.z});
        check("lightning does 5 damage (" + std::to_string(before) + " -> " + std::to_string(g.vitals.health) + ")",
              g.vitals.health == before - LIGHTNING_DAMAGE);
        g.vitals.health = before;
        g.bolts.clear();
        // Keep a bed spawn and some rain for the save and reload check.
        g.bed_spawn = bed_head_;
        g.weather = Weather(WeatherState{true, false, 100000, 100000});
        g.creative = true;
        p.x = tour_start_.x;
        p.z = tour_start_.z;
        p.y = ground_y_;
    }
}

void AutoTest::monsters(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    World& w = g.world;
    auto set = [&](int x, int y, int z, BlockId b) { g.renderer->rebuild(w.set_block(x, y, z, b)); };
    auto exists = [&](const Monster* m) {
        return m && std::any_of(g.monsters.begin(), g.monsters.end(), [&](const auto& o) { return o.get() == m && o->alive(); });
    };
    auto place = [&](double dx, double dz) {
        p.x = arena_.x + 0.5 + dx;
        p.y = arena_.y;
        p.z = arena_.z + 0.5 + dz;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
    };
    auto spawn = [&](MobKind kind, double dx, double dz, int size = 1) {
        Monster* m = g.spawn_monster(kind, arena_.x + 0.5 + dx, arena_.y, arena_.z + 0.5 + dz, size);
        m->yaw = m->target_yaw = 180;  // facing the player
        return m;
    };
    auto clear_mobs = [&] {
        g.monsters.clear();
        g.target_monster = nullptr;
        g.arrows.clear();
        mob_ = nullptr;
    };
    auto heal = [&] {
        g.vitals.reset();
        g.armor_remainder = 0;
    };
    const glm::ivec3 a = arena_;

    if (f == 700) {
        // A flat arena of grass in front of the player (facing +z), at noon in clear weather.
        g.weather.clear();
        g.creative = true;  // monsters leave a creative player alone, so they stand for the picture
        g.flying = false;
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;
        app.options.difficulty = NORMAL;
        clear_mobs();
        g.inventory.armor = {};
        heal();
        arena_ = {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z))};
        for (int dx = -9; dx <= 9; ++dx)
            for (int dz = -4; dz <= 13; ++dz) {
                set(arena_.x + dx, arena_.y - 1, arena_.z + dz, GRASS);
                for (int dy = 0; dy <= 5; ++dy) set(arena_.x + dx, arena_.y + dy, arena_.z + dz, AIR);
            }
        place(0, 0);
        g.yaw = 0;
        g.pitch = 12;
        // Animals in the arena walk behind the player, out of the pictures.
        for (auto& c : g.animals)
            if (std::hypot(c->body.x - p.x, c->body.z - p.z) < 24) {
                c->body.x = p.x + (c->body.x - p.x) * 0.3;
                c->body.z = p.z - 26;
                c->body.y = w.surface_y(static_cast<int>(std::floor(c->body.x)), static_cast<int>(std::floor(c->body.z)));
                c->body.vx = c->body.vy = c->body.vz = 0;
            }
        g.target_animal = nullptr;
        g.hud->show_message("", 0);
        spawn(MobKind::Zombie, -4, 6);
        spawn(MobKind::Skeleton, -2, 6);
        spawn(MobKind::Creeper, 0, 6);
        spawn(MobKind::Spider, 2.2, 6);
        spawn(MobKind::Slime, 4.6, 6, 2);
        Monster* spider = spawn(MobKind::Spider, 7, 8);
        Monster* rider = spawn(MobKind::Skeleton, 7, 8);
        rider->mount = spider;
        spider->rider = rider;
    }
    if (f == 702) {
        for (auto& m : g.monsters) m->burning = 0;
        shot(app, "m10_monsters.png");
        // (Counting only the arena: one may have spawned in a cave somewhere in the meantime.)
        const auto in_arena = std::count_if(g.monsters.begin(), g.monsters.end(), [&](const auto& m) {
            return std::hypot(m->body.x - arena_.x, m->body.z - arena_.z) < 16;
        });
        check("zombie, skeleton, creeper, spider, slime and a spider jockey stand in the arena", in_arena == 7);
        check("the jockey's skeleton rides on its spider",
              g.monsters[6]->mount == g.monsters[5].get() && g.monsters[6]->body.y > g.monsters[5]->body.y + 0.2);
    }
    if (f == 704) {
        // A zombie right in front of a survival player hits them.
        clear_mobs();
        g.creative = false;
        heal();
        place(0, 0);
        mob_ = spawn(MobKind::Zombie, 0, 1.4);
        mob_->targeting = true;
    }
    if (f == 714) {
        check("a zombie hits for 5 on Normal (health " + std::to_string(g.vitals.health) + ")", g.vitals.health == MAX_HEALTH - 5);
        check("the hit knocks the player back", p.z < a.z + 0.4);
        place(0, 0);
        g.yaw = 0;
        g.pitch = 25;
        mob_->body.x = a.x + 0.5;
        mob_->body.z = a.z + 2.2;
        mob_->attack_timer = 100;  // it holds still for the next checks
        g.inventory.slots[g.inventory.selected] = Stack{DIAMOND_SWORD, 1};
        g.update_target();
        check("the crosshair finds the zombie", g.target_monster == mob_);
        g.attack_cooldown = 0;
        g.try_attack();
        check("a diamond sword hits the zombie for " + std::to_string(attack_damage(DIAMOND_SWORD)),
              exists(mob_) && mob_->health == 20 - attack_damage(DIAMOND_SWORD) && mob_->flash > 0);
        int before = mob_->health;
        g.attack_cooldown = 0;
        g.try_attack();
        check("hitting it again straight away does nothing (it's invulnerable for half a second)", mob_->health == before);
    }
    if (f == 716) shot(app, "m10_zombie_hurt.png");
    if (f == 718 && exists(mob_)) {
        drops_before_death_ = g.drops.size();
        mob_->hurt(100, p.x, p.z);
    }
    if (f == 719 && mob_) {
        // Its drops come the moment it dies.
        check("a killed zombie starts its death animation", !mob_->alive() && mob_->dying > 0 && mob_->looted);
        log("the zombie dropped " + std::to_string(g.drops.size() - drops_before_death_) + " feather(s)");
    }
    if (f == 730) shot(app, "m10_zombie_dying.png");
    if (f == 785) {
        check("the dead zombie is gone after its death animation", std::none_of(g.monsters.begin(), g.monsters.end(), [&](const auto& m) {
                  return m->kind == MobKind::Zombie && std::hypot(m->body.x - a.x, m->body.z - a.z) < 12;
              }));
        // A skeleton eight blocks off shoots at the player.
        clear_mobs();
        heal();
        place(0, 0);
        mob_ = spawn(MobKind::Skeleton, 0, 8);
        mob_->targeting = true;
        mob_->attack_timer = 0.3;
    }
    if (f > 785 && f < 840 && !skeleton_shot_) {
        for (const auto& arrow : g.arrows)
            if (!arrow->from_player && !arrow->stuck) {
                skeleton_shot_ = true;
                shot(app, "m10_skeleton_arrow.png");
                break;
            }
    }
    if (f == 840) {
        check("a skeleton shoots arrows at the player", skeleton_shot_);
        log("health after the skeleton's arrows: " + std::to_string(g.vitals.health));
        // A creeper walks up, hisses and blows up. Diamond armor keeps the player alive.
        clear_mobs();
        heal();
        place(0, 0);
        g.inventory.armor = {Stack{DIAMOND_HELMET, 1}, Stack{DIAMOND_CHESTPLATE, 1}, Stack{DIAMOND_LEGGINGS, 1}, Stack{DIAMOND_BOOTS, 1}};
        mob_ = spawn(MobKind::Creeper, 0, 2.6);
        mob_->targeting = true;
        blocks_before_ = 0;
        for (int dx = -4; dx <= 4; ++dx)
            for (int dy = -4; dy <= 2; ++dy)
                for (int dz = -2; dz <= 6; ++dz) blocks_before_ += w.get_block(a.x + dx, a.y + dy, a.z + dz) != AIR;
    }
    if (f == 860) {
        check("the creeper's fuse is burning", exists(mob_) && mob_->fuse > 0);
        check("the creeper hisses", g.sounds_started[static_cast<int>(Sound::Fuse)] == 1);
    }
    if (f == 900 && exists(mob_)) shot(app, "m10_creeper_fuse.png");
    if (f == 940) {
        int after = 0;
        for (int dx = -4; dx <= 4; ++dx)
            for (int dy = -4; dy <= 2; ++dy)
                for (int dz = -2; dz <= 6; ++dz) after += w.get_block(a.x + dx, a.y + dy, a.z + dz) != AIR;
        check("the creeper blew up (it's gone)", std::none_of(g.monsters.begin(), g.monsters.end(), [&](const auto& m) {
                  return m->kind == MobKind::Creeper && std::hypot(m->body.x - a.x, m->body.z - a.z) < 12;
              }));
        check("the creeper's blast is heard", g.sounds_started[static_cast<int>(Sound::Explosion)] == 1);
        check("the blast destroyed blocks (" + std::to_string(blocks_before_ - after) + ")", after < blocks_before_ - 5);
        check("the blast hurt the player through diamond armor (health " + std::to_string(g.vitals.health) + ")",
              g.vitals.health < MAX_HEALTH && !g.vitals.dead());
        g.inventory.armor = {};
        heal();
        place(-6, -2);
        g.yaw = 20;
        g.pitch = 30;
    }
    if (f == 942) shot(app, "m10_crater.png");
    if (f == 944) {
        // A spider climbs a wall to get at a player on top of it, at night.
        clear_mobs();
        heal();
        g.level.time += 12000;
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -1; dy <= 3; ++dy) set(a.x + dx - 6, a.y + dy, a.z + 9, STONE);
        p.x = a.x - 5.5;
        p.y = a.y + 4;
        p.z = a.z + 9.5;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
        g.creative = false;
        mob_ = spawn(MobKind::Spider, -6, 7.2);
        mob_->yaw = mob_->target_yaw = 0;
        spider_top_ = mob_->body.y;
    }
    if (f > 944 && f < 1040 && exists(mob_)) spider_top_ = std::max(spider_top_, mob_->body.y);
    if (f == 975) {
        g.yaw = 180;
        g.pitch = 60;
        shot(app, "m10_spider_climb.png");
    }
    if (f == 1040) {
        check("a spider climbs straight up a wall (rose " + std::to_string(spider_top_ - a.y) + " blocks)",
              spider_top_ >= a.y + 2.5);
        // A big slime splits in two to four smaller ones when it dies.
        clear_mobs();
        heal();
        g.creative = true;
        g.level.time -= 12000;
        place(0, 0);
        g.yaw = 0;
        g.pitch = 15;
        mob_ = spawn(MobKind::Slime, 0, 6, 4);
        mob_->hurt(100, p.x, p.z);
    }
    if (f == 1042) {
        int smaller = static_cast<int>(std::count_if(g.monsters.begin(), g.monsters.end(), [](const auto& m) {
            return m->alive() && m->kind == MobKind::Slime && m->size == 2;
        }));
        check("a size 4 slime splits into " + std::to_string(smaller) + " size 2 slimes", smaller >= 2 && smaller <= 4);
    }
    if (f == 1044) shot(app, "m10_slimes.png");
    if (f == 1050) {
        // A zombie burning in the sun, and the fire hurting it.
        clear_mobs();
        mob_ = spawn(MobKind::Zombie, 0, 5);
        mob_->burning = 5;
    }
    if (f == 1054) shot(app, "m10_burning.png");
    if (f == 1130) {
        check("a burning zombie takes damage (health " + std::to_string(exists(mob_) ? mob_->health : 0) + ")",
              !exists(mob_) || mob_->health < 20);
        // TNT: breaking it lights it; its blast lights the TNT next to it.
        clear_mobs();
        place(0, -3);
        g.yaw = 0;
        g.pitch = 20;
        set(a.x, a.y, a.z + 5, TNT);
        set(a.x, a.y, a.z + 6, TNT);
        g.break_block({a.x, a.y, a.z + 5}, TNT);
        check("breaking TNT lights it", g.tnts.size() == 1 && w.get_block(a.x, a.y, a.z + 5) == AIR);
        if (!g.tnts.empty()) g.tnts[0]->fuse = 0.5;
    }
    if (f == 1136) shot(app, "m10_tnt.png");
    if (f == 1170) {
        check("the TNT went off and lit the TNT next to it", g.tnts.size() == 1 && w.get_block(a.x, a.y, a.z + 6) == AIR);
        check("the blast left a crater", w.get_block(a.x, a.y - 1, a.z + 5) == AIR);
        if (!g.tnts.empty()) g.tnts[0]->fuse = 0.05;
    }
    if (f == 1176) {
        check("the second TNT went off too", g.tnts.empty());
        check("lit TNT hisses and both blasts are heard", g.sounds_started[static_cast<int>(Sound::Fuse)] == 3 &&
                                                              g.sounds_started[static_cast<int>(Sound::Explosion)] == 3);
        shot(app, "m10_tnt_crater.png");
        // A mob spawner in a dark room underground makes its monster.
        glm::ivec3 c(a.x, 20, a.z);
        for (int dx = -4; dx <= 4; ++dx)
            for (int dy = -1; dy <= 4; ++dy)
                for (int dz = -4; dz <= 4; ++dz) {
                    bool shell = std::abs(dx) == 4 || std::abs(dz) == 4 || dy == -1 || dy == 4;
                    set(c.x + dx, c.y + dy, c.z + dz, shell ? MOSSY_COBBLE : AIR);
                }
        set(c.x, c.y, c.z, SPAWNER);
        g.spawners.push_back({c, 1});
        p.x = c.x + 2.5;
        p.y = c.y;
        p.z = c.z - 2.5;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
        mob_ = nullptr;
    }
    if (f == 1180) {
        const glm::ivec3 c(a.x, 20, a.z);
        const MobKind kind = spawner_mob(g.level.seed, c);
        int made = static_cast<int>(std::count_if(g.monsters.begin(), g.monsters.end(), [&](const auto& m) { return m->kind == kind; }));
        check("a mob spawner in the dark makes " + std::string(mob_name(kind)) + "s (" + std::to_string(made) + ")", made > 0);
        set(c.x - 3, c.y, c.z - 3, TORCH);
        face(g, c.x + 0.5, c.z + 0.5, 20);
    }
    if (f == 1182) shot(app, "m10_spawner.png");
    if (f == 1184) {
        // Peaceful: every monster vanishes and none spawn.
        app.options.difficulty = PEACEFUL;
        place(0, 0);
    }
    if (f == 1186) {
        g.spawn_monsters();
        check("on Peaceful the monsters vanish", g.monsters.empty() && g.vitals.peaceful);
        app.options.difficulty = NORMAL;
        // Dying: everything carried drops, and the death screen comes up.
        g.creative = false;
        g.inventory.add(COBBLE, 10);
        g.inventory.add(IRON_SWORD, 1);
        drops_before_death_ = g.drops.size();
        g.vitals.health = 3;
        g.vitals.hurt_timer = 0;
        g.hurt_player(10);
    }
    if (f == 1188) {
        bool empty = std::none_of(g.inventory.slots.begin(), g.inventory.slots.end(), [](const auto& s) { return s.has_value(); });
        check("dying drops everything carried (" + std::to_string(g.drops.size() - drops_before_death_) + " stacks)",
              g.vitals.dead() && empty && g.drops.size() > drops_before_death_);
        check("the death screen comes up", app.screen() != nullptr && !g.paused);
        shot(app, "m10_death.png");
        if (app.screen()) app.screen()->on_button(0);  // Respawn
    }
    if (f == 1190) {
        // (The bed itself was broken earlier, so it's the world spawn.)
        check("Respawn brings the player back to life at the spawn point",
              !g.vitals.dead() && !app.screen() && std::hypot(p.x - g.spawn.x, p.z - g.spawn.z) < 0.01);
        g.bed_spawn = bed_head_;  // kept for the save and reload check
        // Natural spawning: at midnight monsters appear in the dark around the player.
        clear_mobs();
        g.creative = true;
        g.level.time = 18000 + (g.level.time / DAY_TICKS) * DAY_TICKS;
        for (int i = 0; i < 40; ++i) g.spawn_monsters();
        bool far = std::all_of(g.monsters.begin(), g.monsters.end(), [&](const auto& m) {
            return std::hypot(m->body.x - p.x, m->body.y - p.y, m->body.z - p.z) >= SPAWN_MIN_DISTANCE - 0.01;
        });
        check("monsters spawn in the dark around the player (" + std::to_string(g.monsters.size()) + "), never close by",
              !g.monsters.empty() && far);
        g.debug_screen = true;
    }
    if (f == 1192) {
        shot(app, "m10_debug.png");
        g.debug_screen = false;
        g.level.time = day_time_;
        g.weather = Weather(WeatherState{true, false, 100000, 100000});  // the rain the save check looks for
    }
}

void AutoTest::farming(App& app, Game& g) {
    const long f = g.frame - 150;  // written for frames 1200..1299, runs after fluids and fire
    Body& p = g.player;
    World& w = g.world;
    const glm::ivec3 a = arena_;
    auto set = [&](glm::ivec3 b, BlockId id, int data = 0) { g.renderer->rebuild(w.set_block(b.x, b.y, b.z, id, data)); };
    // Look at the middle of a block's top and pick it as the target.
    auto aim = [&](glm::ivec3 b) {
        glm::vec3 e = g.eye();
        glm::vec3 d = glm::vec3(b) + glm::vec3(0.5f, 0.95f, 0.5f) - e;
        g.yaw = degrees(std::atan2(d.x, d.z));
        g.pitch = -degrees(std::asin(d.y / glm::length(d)));
        g.update_target();
        return g.target && g.target->block == b;
    };
    auto hold = [&](ItemId item, int count = 1) { g.inventory.slots[g.inventory.selected] = Stack{item, count}; };
    // The field: farmland in rows 2..4 in front of the player (+z) with water in the middle.
    const glm::ivec3 water = a + glm::ivec3(0, -1, 4);

    if (f == 1200) {
        g.monsters.clear();
        g.target_monster = nullptr;
        g.arrows.clear();
        g.drops.clear();
        g.puffs.clear();
        g.weather.clear();
        g.creative = false;
        g.vitals.reset();
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;  // noon
        p.x = a.x + 0.5;
        p.y = a.y;
        p.z = a.z + 0.5;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
        for (int dx = -4; dx <= 4; ++dx)
            for (int dz = 1; dz <= 9; ++dz) {
                set(a + glm::ivec3(dx, -1, dz), GRASS);
                for (int dy = 0; dy <= 4; ++dy) set(a + glm::ivec3(dx, dy, dz), AIR);
            }
        // A hoe tills the grass in front of the player.
        hold(WOOD_HOE);
        const glm::ivec3 first = a + glm::ivec3(0, -1, 2);
        check("aiming at the grass in front", aim(first));
        g.use_item();
        check("a hoe turns grass into farmland", w.get_block(first.x, first.y, first.z) == FARMLAND);
        check("tilling wears the hoe", g.inventory.slots[g.inventory.selected]->damage == 1);
        set(water, WATER);
        FarmEvents ev;
        for (int dx = -2; dx <= 2; ++dx)
            for (int dz = 2; dz <= 6; ++dz) till(w, a + glm::ivec3(dx, -1, dz), ev);
        g.apply(ev);
        set(water, WATER);
        // Seeds planted with a right click.
        hold(SEEDS, 5);
        check("aiming at the farmland", aim(first));
        g.use_item();
        check("seeds plant wheat on farmland", w.get_block(first.x, first.y + 1, first.z) == WHEAT_CROP &&
                                                   g.inventory.slots[g.inventory.selected]->count == 4);
        // Wheat in every stage across the field, ripest at the back.
        for (int dx = -2; dx <= 2; ++dx)
            for (int dz = 2; dz <= 6; ++dz) {
                glm::ivec3 c = a + glm::ivec3(dx, 0, dz);
                if (c + glm::ivec3(0, -1, 0) == water || c == first + glm::ivec3(0, 1, 0)) continue;
                set(c, WHEAT_CROP, std::min(WHEAT_RIPE, (dz - 2) * 2 + (dx + 2) % 2));
            }
        // Sugar cane by the water, wild plants and a cake to the side.
        for (int k = 0; k < 3; ++k) set(a + glm::ivec3(3, k, 4), SUGAR_CANE_PLANT);
        set(a + glm::ivec3(2, -1, 4), DIRT);
        set(a + glm::ivec3(3, -1, 5), WATER);
        set(a + glm::ivec3(-3, 0, 2), TALL_GRASS);
        set(a + glm::ivec3(-3, 0, 3), FERN);
        set(a + glm::ivec3(-3, 0, 4), YELLOW_FLOWER);
        set(a + glm::ivec3(-3, 0, 5), RED_ROSE);
        set(a + glm::ivec3(-4, 0, 3), BROWN_MUSHROOM);
        set(a + glm::ivec3(-4, 0, 4), RED_MUSHROOM);
        set(a + glm::ivec3(-4, 0, 6), TALL_GRASS);
        set(a + glm::ivec3(3, 0, 7), CAKE_BLOCK, 2);
        // The field's wetness: everything near the water is wet after a few random ticks.
        Rng rng(1);
        for (int i = 0; i < 100; ++i)
            for (int dx = -2; dx <= 2; ++dx)
                for (int dz = 2; dz <= 6; ++dz) random_tick(w, a + glm::ivec3(dx, -1, dz), rng, {}, ev);
        g.apply(ev);
        check("farmland near water gets wet", w.get_data(first.x, first.y, first.z) == FARMLAND_WET);
        g.yaw = 0;
        g.pitch = 32;
        g.hud->show_message("", 0);
    }
    if (f == 1202) shot(app, "m8_farm.png");
    if (f == 1204) {
        // Bone meal ripens a young crop; breaking ripe wheat drops wheat (and maybe seeds).
        const glm::ivec3 young = a + glm::ivec3(-1, 0, 2);
        set(young, WHEAT_CROP, 1);
        hold(BONE_MEAL, 2);
        check("aiming at a young crop", aim(young));
        g.use_item();
        check("bone meal ripens wheat at once", w.get_data(young.x, young.y, young.z) == WHEAT_RIPE &&
                                                    g.inventory.slots[g.inventory.selected]->count == 1);
        const size_t drops = g.drops.size();
        g.break_block(young, WHEAT_CROP);
        bool wheat = std::any_of(g.drops.begin() + static_cast<std::ptrdiff_t>(drops), g.drops.end(),
                                 [](const auto& d) { return d->stack.item == WHEAT; });
        check("harvesting ripe wheat drops wheat", wheat);
        // Breaking the farmland under a crop pops the crop off.
        const glm::ivec3 crop = a + glm::ivec3(1, 0, 3);
        g.break_block(crop - glm::ivec3(0, 1, 0), FARMLAND);
        check("a crop pops off when its farmland is broken", w.get_block(crop.x, crop.y, crop.z) == AIR);
        // A slice of cake.
        const glm::ivec3 cake = a + glm::ivec3(3, 0, 7);
        p.x = cake.x + 0.5;
        p.z = cake.z - 1.5;
        g.vitals.hunger = 10;
        hold(STICK);
        check("aiming at the cake", aim(cake));
        g.use_item();
        check("eating a slice of cake fills 2 hunger and leaves 3 slices",
              g.vitals.hunger == 12 && w.get_data(cake.x, cake.y, cake.z) == 3);
        g.yaw = 0;
        g.pitch = 45;
    }
    if (f == 1206) shot(app, "m8_cake.png");
    if (f == 1208) {
        // Wild plants in the generated world.
        int grass = 0, flowers = 0, cane = 0;
        for (Chunk* c : w.all_chunks())
            for (BlockId b : c->blocks) {
                grass += b == TALL_GRASS || b == FERN;
                flowers += b == YELLOW_FLOWER || b == RED_ROSE;
                cane += b == SUGAR_CANE_PLANT;
            }
        log("wild plants in the loaded world: " + std::to_string(grass) + " tall grass, " + std::to_string(flowers) +
            " flowers, " + std::to_string(cane) + " sugar cane");
        check("the world grows tall grass, flowers and sugar cane", grass > 50 && flowers > 0 && cane > 0);
        g.creative = true;
        if (auto at = find_biome(w.generator(), a.x, a.z, {Biome::PLAINS, Biome::RAINFOREST})) {
            p.x = at->x + 0.5;
            p.z = at->y + 0.5;
            g.renderer->load_all(p.x, p.z, 2);
            p.y = w.surface_y(at->x, at->y) + 1.5;
            g.flying = true;
            g.yaw = 30;
            g.pitch = 25;
        }
        g.hud->show_message("", 0);
    }
    if (f == 1212) shot(app, "m8_meadow.png");
    if (f == 1214) {
        g.flying = false;
        p.x = a.x + 0.5;
        p.y = a.y;
        p.z = a.z + 0.5;
        g.fall_peak = p.y;
        g.weather = Weather(WeatherState{true, false, 100000, 100000});  // the rain the save check looks for
    }
}

void AutoTest::animals(App& app, Game& g) {
    const long f = g.frame - 1450;  // runs after farming
    Body& p = g.player;
    World& w = g.world;
    // A flat grass pen behind the farm: the animals stand in a row along its middle (z = 2).
    const glm::ivec3 b = arena_ + glm::ivec3(0, 0, -20);
    auto set = [&](glm::ivec3 c, BlockId id) { g.renderer->rebuild(w.set_block(c.x, c.y, c.z, id)); };
    auto hold = [&](ItemId item, int count = 1) { g.inventory.slots[g.inventory.selected] = Stack{item, count}; };
    auto held = [&] { return g.inventory.slots[g.inventory.selected]; };
    auto stand = [&](double x, double z) {
        p.x = x;
        p.y = b.y;
        p.z = z;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
    };
    // Stand 2.5 blocks in front of an animal and look at its middle.
    auto aim = [&](Animal* a) {
        if (!a) return false;
        stand(a->body.x, a->body.z - 2.5);
        const glm::vec3 e = g.eye(), c(a->body.x, a->body.y + a->body.height * 0.5, a->body.z);
        const glm::vec3 d = c - e;
        g.yaw = degrees(std::atan2(d.x, d.z));
        g.pitch = -degrees(std::asin(d.y / glm::length(d)));
        g.update_target();
        return g.target_animal == a;
    };
    auto alive = [&](const Animal* a) {
        return std::any_of(g.animals.begin(), g.animals.end(), [&](const auto& o) { return o.get() == a && o->alive(); });
    };
    auto count = [&](AnimalKind k) {
        return std::count_if(g.animals.begin(), g.animals.end(), [&](const auto& a) { return a->kind == k && a->alive(); });
    };
    auto exists = [&](const Monster* m) {
        return std::any_of(g.monsters.begin(), g.monsters.end(), [&](const auto& o) { return o.get() == m && o->alive(); });
    };
    auto dropped = [&](ItemId item) {
        int n = 0;
        for (const auto& d : g.drops) n += d->stack.item == item ? d->stack.count : 0;
        return n;
    };

    if (f == 0) {
        g.monsters.clear();
        g.target_monster = nullptr;
        g.arrows.clear();
        g.drops.clear();
        g.puffs.clear();
        g.animals.clear();
        g.target_animal = nullptr;
        g.riding = nullptr;
        g.weather.clear();
        g.creative = false;
        g.flying = false;
        g.vitals.reset();
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;  // noon
        for (int dx = -7; dx <= 7; ++dx)
            for (int dz = -16; dz <= 6; ++dz) {
                set(b + glm::ivec3(dx, -2, dz), DIRT);
                set(b + glm::ivec3(dx, -1, dz), GRASS);
                for (int dy = 0; dy <= 5; ++dy) set(b + glm::ivec3(dx, dy, dz), AIR);
            }
        // A pond for the squid in the far corner.
        for (int dx = 4; dx <= 6; ++dx)
            for (int dz = 3; dz <= 5; ++dz)
                for (int dy = -4; dy <= -1; ++dy) set(b + glm::ivec3(dx, dy, dz), dy == -4 ? DIRT : WATER);
        const AnimalKind row[] = {AnimalKind::Cow, AnimalKind::Pig, AnimalKind::Sheep, AnimalKind::Sheep, AnimalKind::Chicken,
                                  AnimalKind::Wolf};
        for (int i = 0; i < 6; ++i) {
            Animal* a = g.spawn_animal(row[i], b.x - 5.5 + i * 2, b.y, b.z + 2.5);
            a->state = Animal::State::Idle;
            a->state_timer = 1000;
            a->graze = false;
            a->yaw = a->target_yaw = 180;  // looking at the player
            if (i == 2) a->wool = 0;
            if (i == 3) a->wool = 6;  // a rare pink sheep
            pen_[i] = a;
        }
        pen_[6] = g.spawn_animal(AnimalKind::Squid, b.x + 5.5, b.y - 2.5, b.z + 4.5);
        check("every kind of animal can be made", std::all_of(pen_.begin(), pen_.end(), [](Animal* a) { return a != nullptr; }));
        stand(b.x + 0.5, b.z - 4.5);
        g.yaw = 0;
        g.pitch = 18;
        g.hud->show_message("", 0);
    }
    if (f == 2) shot(app, "m9_animals.png");
    if (f == 4) {
        Animal *cow = pen_[0], *pig = pen_[1], *sheep = pen_[2], *pink = pen_[3], *wolf = pen_[5];
        // A saddle on the pig.
        hold(SADDLE);
        check("aiming at the pig", aim(pig));
        g.use_item();
        check("a saddle goes on a pig", pig->saddled && !held());
        // Shears take 1 to 3 wool off a sheep, and it stays bare.
        hold(SHEARS);
        check("aiming at the white sheep", aim(sheep));
        g.use_item();
        const int wool = dropped(WHITE_WOOL);
        check("shearing a sheep drops 1 to 3 white wool (" + std::to_string(wool) + ")", sheep->sheared && wool >= 1 && wool <= 3);
        check("shearing wears the shears", held() && held()->damage == 1);
        g.use_item();
        check("a sheared sheep can't be sheared again", dropped(WHITE_WOOL) == wool && held()->damage == 1);
        // Dye turns a sheep's wool.
        hold(ROSE_RED, 2);
        check("aiming at the pink sheep", aim(pink));
        g.use_item();
        check("rose red dyes a sheep red", pink->wool == 14 && held()->count == 1);
        // Milk from the cow.
        hold(BUCKET);
        check("aiming at the cow", aim(cow));
        g.use_item();
        check("a bucket milks a cow", held() && held()->item == MILK_BUCKET);
        // Bones until the wolf is tame (one in three works).
        hold(BONE, 30);
        check("aiming at the wolf", aim(wolf));
        int bones = 0;
        while (!wolf->tamed && held() && bones < 30) {
            g.use_item();
            ++bones;
        }
        log("the wolf took " + std::to_string(bones) + " bone(s) to tame");
        check("bones tame a wolf, and it sits down", wolf->tamed && wolf->sitting && wolf->health == TAMED_WOLF_HEALTH);
        hold(STICK);
        stand(b.x + 0.5, b.z - 4.5);
        g.yaw = 0;
        g.pitch = 18;
        g.hud->show_message("", 0);
    }
    if (f == 6) shot(app, "m9_sheared_saddled_tamed.png");
    if (f == 8) {
        // Right click the saddled pig with an empty hand to ride it.
        g.inventory.slots[g.inventory.selected].reset();
        check("aiming at the saddled pig", aim(pen_[1]));
        g.use_item();
        check("right clicking a saddled pig rides it", g.riding == pen_[1] && pen_[1]->ridden);
        g.third_person = true;
        g.yaw = 30;
        g.pitch = 25;
    }
    if (f == 10) {
        Animal* pig = pen_[1];
        check("the rider sits on the pig", std::abs(p.y - (pig->body.y + RIDE_HEIGHT)) < 1e-6 &&
                                               std::hypot(p.x - pig->body.x, p.z - pig->body.z) < 1e-6);
        shot(app, "m9_riding.png");
        g.dismount();
        g.third_person = false;
        check("getting off leaves the player beside or on the pig", !g.riding && !pig->ridden && !p.collides());
    }
    if (f == 12) {
        // The tamed wolf stands up, and the player walks off: it turns up next to them.
        pen_[5]->sitting = false;
        stand(b.x + 0.5, b.z - 14.5);
        g.yaw = 0;
        g.pitch = 10;
    }
    if (f == 30) {
        Animal* wolf = pen_[5];
        const double d = std::hypot(wolf->body.x - p.x, wolf->body.z - p.z);
        check("a tamed wolf follows its owner (" + std::to_string(d).substr(0, 4) + " blocks away)", d < WOLF_FOLLOW_DISTANCE + 1);
        // A zombie: when the player hits it, the wolf goes for it too.
        mob_ = g.spawn_monster(MobKind::Zombie, p.x + 1.5, p.y, p.z + 3);
        g.update_target();
        g.target_animal = nullptr;
        g.target_monster = mob_;
        g.attack_cooldown = 0;
        g.inventory.slots[g.inventory.selected].reset();
        g.try_attack();
        check("hitting a monster sends the tamed wolf after it", wolf->prey == mob_);
    }
    if (f == 32) shot(app, "m9_wolf_fights.png");
    if (f == 90) {
        const bool bitten = !exists(mob_) || mob_->health < mob_->info().health - 1;
        if (!bitten) {
            char buf[200];
            const Animal* wolf = pen_[5];
            std::snprintf(buf, sizeof buf, "  wolf at %.1f %.1f %.1f (sitting %d, prey %d), zombie at %.1f %.1f %.1f with %d health",
                          wolf->body.x, wolf->body.y, wolf->body.z, wolf->sitting, wolf->prey == mob_, mob_->body.x, mob_->body.y,
                          mob_->body.z, mob_->health);
            log(buf);
        }
        check("the wolf bites the zombie", bitten);
        g.monsters.clear();
        g.target_monster = nullptr;
        mob_ = nullptr;
        g.vitals.reset();
        // A wild wolf turns on whoever hits it.
        Animal* wild = g.spawn_animal(AnimalKind::Wolf, p.x - 3, p.y, p.z + 2);
        g.update_target();
        g.target_monster = nullptr;
        g.target_animal = wild;
        g.attack_cooldown = 0;
        g.try_attack();
        check("a wild wolf gets angry when hit, a tamed one doesn't", wild->angry && !pen_[5]->angry);
        g.animals.erase(std::remove_if(g.animals.begin(), g.animals.end(), [&](const auto& a) { return a.get() == wild; }),
                        g.animals.end());
        for (const auto& a : g.animals)
            if (a->prey == wild) a->prey = nullptr;
        g.target_animal = nullptr;
        // The chicken lays an egg.
        pen_[4]->egg_timer = 0;
        chickens_ = count(AnimalKind::Chicken);
    }
    if (f == 92) {
        check("a chicken lays an egg", dropped(EGG) == 1);
        // Throw a lot of eggs on the ground in front: some hatch.
        g.drops.clear();
        stand(b.x + 0.5, b.z - 10.5);
        g.yaw = 0;
        g.pitch = 40;
        hold(EGG, 16);
        for (int i = 0; i < 64; ++i) {
            if (!held()) hold(EGG, 16);
            g.throw_egg();
        }
        check("throwing eggs uses them up", !held() || held()->count < 16);
    }
    if (f == 110) {
        const bool eggs_gone = std::none_of(g.arrows.begin(), g.arrows.end(), [](const auto& a) { return a->item == EGG; });
        const auto now = count(AnimalKind::Chicken);
        check("thrown eggs break, and chickens hatch from some (" + std::to_string(now - chickens_) + ")",
              eggs_gone && now > chickens_);
        // The pig (with its saddle) and the squid die and drop their things.
        drops_before_death_ = g.drops.size();
        pen_[1]->hurt(20, p.x, p.z);
        pen_[6]->hurt(20, p.x, p.z);
    }
    if (f == 160) {
        check("a saddled pig drops its saddle", dropped(SADDLE) == 1 && !alive(pen_[1]));
        check("a squid drops ink sacs", dropped(INK_SAC) >= 1 && !alive(pen_[6]));
        // Leave the tamed wolf (sitting) and the sheared sheep by the farm, to see them after loading.
        const glm::dvec3 home(arena_.x + 0.5, arena_.y, arena_.z + 0.5);
        pen_[5]->sitting = true;
        pen_[5]->body.x = home.x + 2;
        pen_[5]->body.y = home.y;
        pen_[5]->body.z = home.z - 1;
        pen_[2]->body.x = home.x - 2;
        pen_[2]->body.y = home.y;
        pen_[2]->body.z = home.z - 1;
        stand(home.x, home.z);
        p.y = home.y;
        g.yaw = 0;
        g.pitch = 0;
        g.weather = Weather(WeatherState{true, false, 100000, 100000});  // the rain the save check looks for
    }
}

void AutoTest::building(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    World& w = g.world;
    const glm::ivec3& o = build_;
    auto set = [&](glm::ivec3 b, BlockId id, int data = 0) { g.renderer->rebuild(w.set_block(b.x, b.y, b.z, id, data)); };
    auto at = [&](glm::ivec3 b) { return w.get_block(b.x, b.y, b.z); };
    auto stand = [&](double x, double z) {
        p.x = o.x + 0.5 + x;
        p.y = o.y;
        p.z = o.z + 0.5 + z;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
    };
    // Aim at a point inside a block, just behind the face `face` (0 for the top), and pick it.
    auto aim = [&](glm::ivec3 b, glm::ivec3 face = {0, 1, 0}) {
        const glm::vec3 e = g.eye();
        glm::vec3 point = glm::vec3(b) + glm::vec3(0.5f) + glm::vec3(face) * 0.45f;
        if (face.y > 0) {  // just under the top of what is there (half way up for a slab)
            float top = 0;
            for (const Box& box : block_boxes(w, b, false)) top = std::max(top, box.hi.y);
            point.y = b.y + top - 0.05f;
        }
        const glm::vec3 d = point - e;
        g.yaw = degrees(std::atan2(d.x, d.z));
        g.pitch = -degrees(std::asin(d.y / glm::length(d)));
        g.update_target();
        return g.target && g.target->block == b;
    };
    auto hold = [&](ItemId item, int count = 1) { g.inventory.slots[g.inventory.selected] = Stack{item, count}; };
    // Right-click the face of a block with an item, standing in front of a side face or a little
    // south of a top.
    auto use_on = [&](glm::ivec3 clicked, glm::ivec3 face, ItemId item, int count = 1) {
        if (face.y == 0)
            stand(clicked.x - o.x + face.x * 2.5, clicked.z - o.z + face.z * 2.5);
        else
            stand(clicked.x - o.x + 0.3, clicked.z - o.z - 2.6);
        hold(item, count);
        const bool aimed = aim(clicked, face);
        g.use_item();
        return aimed;
    };
    const glm::ivec3 UP(0, 1, 0), NORTH(0, 0, -1);
    const int wall_z = 10;

    if (f == 1380) {
        build_return_ = {p.x, p.y, p.z};
        build_weather_ = g.weather;
        build_time_ = g.level.time;
        build_creative_ = g.creative;
        g.monsters.clear();
        g.target_monster = nullptr;
        g.weather.clear();
        g.creative = false;
        g.flying = false;
        g.vitals.reset();
        g.burn_ticks = 0;
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;
        g.drops.clear();
        // Below the clouds, somewhere the ground is well below it.
        build_ = {static_cast<int>(std::floor(p.x)) - 60, 98, static_cast<int>(std::floor(p.z)) + 40};
        for (int tries = 0; tries < 20; ++tries) {
            g.renderer->load_all(o.x + 0.5, o.z + 0.5, 2);
            int top = 0;
            for (int dx = -10; dx <= 10; dx += 2)
                for (int dz = -5; dz <= 12; dz += 2) top = std::max(top, w.surface_y(o.x + dx, o.z + dz));
            if (top < 90) break;
            build_.x -= 48;
        }
        for (int dx = -10; dx <= 10; ++dx)
            for (int dz = -5; dz <= 12; ++dz) {
                set(o + glm::ivec3(dx, -1, dz), STONE);
                for (int dy = 0; dy < 7; ++dy) set(o + glm::ivec3(dx, dy, dz), AIR);
            }
        // The back wall: wool in all 16 colours, a row of the new blocks, and stone for pictures.
        const BlockId row[16] = {GOLD_BLOCK, IRON_BLOCK, DIAMOND_BLOCK, LAPIS_BLOCK, BOOKSHELF, OBSIDIAN, MOSSY_COBBLE, SANDSTONE,
                                 NETHERRACK, SOUL_SAND, GLASS, SNOW_BLOCK, ICE, CLAY_BLOCK, BRICK, JUKEBOX};
        for (int i = 0; i < 16; ++i) {
            set(o + glm::ivec3(i - 8, 0, wall_z), WOOL, i);
            set(o + glm::ivec3(i - 8, 1, wall_z), row[i]);
            for (int dy = 2; dy < 6; ++dy) set(o + glm::ivec3(i - 8, dy, wall_z), STONE);
        }
        // Pictures on the wall, hung like a player does.
        bool hung = true;
        for (int x : {-5, 3}) {
            const glm::ivec3 spot = o + glm::ivec3(x, 3, wall_z);
            use_on(spot, NORTH, PAINTING);
            hung &= at(spot - glm::ivec3(0, 0, 1)) == PAINTING_BLOCK;
        }
        check("paintings hang on a wall", hung);
        // A double chest from two chests side by side.
        chest_pos_ = o + glm::ivec3(-6, 0, 6);
        use_on(chest_pos_ - UP, UP, CHEST);
        use_on(chest_pos_ + glm::ivec3(1, -1, 0), UP, CHEST);
        check("two chests side by side make a double chest", at(chest_pos_) == CHEST && chest_partner(w, chest_pos_) == chest_pos_ + glm::ivec3(1, 0, 0));
        // Stairs, slabs and a double slab.
        use_on(o + glm::ivec3(-3, -1, 6), UP, WOOD_STAIRS);
        use_on(o + glm::ivec3(-2, -1, 6), UP, COBBLE_STAIRS);
        check("stairs are placed with their back away from the player", at(o + glm::ivec3(-3, 0, 6)) == WOOD_STAIRS &&
                                                                            w.get_data(o.x - 3, o.y, o.z + 6) == 0);
        for (int k = 0; k < 4; ++k) use_on(o + glm::ivec3(4 + k, -1, 4), UP, STONE_SLAB + k);
        use_on(o + glm::ivec3(7, 0, 4), UP, COBBLE_SLAB);
        check("a slab on a slab of its kind makes a double slab", at(o + glm::ivec3(7, 0, 4)) == DOUBLE_SLAB &&
                                                                     at(o + glm::ivec3(6, 0, 4)) == SLAB);
        // A fence with a door in it, and an iron door.
        for (int x : {0, 1, 2, 4, 5}) set(o + glm::ivec3(x, 0, 7), FENCE);
        set(o + glm::ivec3(0, 1, 7), FENCE);
        use_on(o + glm::ivec3(3, -1, 7), UP, WOOD_DOOR);
        check("a wooden door stands two blocks high", at(o + glm::ivec3(3, 0, 7)) == WOOD_DOOR_BLOCK && at(o + glm::ivec3(3, 1, 7)) == WOOD_DOOR_BLOCK);
        use_on(o + glm::ivec3(7, -1, 7), UP, IRON_DOOR);
        // A ladder up a pillar, a trapdoor beside it.
        for (int dy = 0; dy < 4; ++dy) set(o + glm::ivec3(-9, dy, 2), STONE);
        for (int dy = 0; dy < 4; ++dy) use_on(o + glm::ivec3(-9, dy, 2), glm::ivec3(1, 0, 0), LADDER);
        check("ladders go up the side of a pillar", at(o + glm::ivec3(-8, 3, 2)) == LADDER);
        use_on(o + glm::ivec3(-9, 1, 2), NORTH, TRAPDOOR);
        check("a trapdoor hangs on the side of a block", at(o + glm::ivec3(-9, 1, 1)) == TRAPDOOR);
        // A note block on stone and a jukebox.
        note_pos_ = o + glm::ivec3(1, 0, 4);
        jukebox_pos_ = o + glm::ivec3(2, 0, 4);
        use_on(note_pos_ - UP, UP, NOTE_BLOCK);
        use_on(jukebox_pos_ - UP, UP, JUKEBOX);
        const int drums = g.sounds_started[static_cast<int>(Sound::NoteBassDrum)];
        use_on(note_pos_, UP, STICK);
        check("right-clicking a note block raises it a note and plays a drum on stone",
              w.chunk_at(note_pos_)->notes[cell_index(note_pos_)] == 1 && g.sounds_started[static_cast<int>(Sound::NoteBassDrum)] == drums + 1);
        use_on(jukebox_pos_, UP, RECORD_CAT);
        check("a jukebox takes a disc and plays it", w.chunk_at(jukebox_pos_)->records[cell_index(jukebox_pos_)] == RECORD_CAT &&
                                                         g.sounds_started[static_cast<int>(Sound::RecordCat)] == 1 &&
                                                         !g.inventory.slots[g.inventory.selected]);
        // A wall sign with something on it.
        const glm::ivec3 wall_sign = o + glm::ivec3(0, 2, wall_z - 1);
        set(wall_sign, WALL_SIGN, 0);
        w.chunk_at(wall_sign)->signs[cell_index(wall_sign)] = {"", "Gallery", "of blocks", ""};
        g.renderer->rebuild({ChunkPos{chunk_of(wall_sign.x), chunk_of(wall_sign.z)}});
    }
    if (f == 1381) {
        // A sign post: placing it opens the sign screen.
        sign_pos_ = o + glm::ivec3(-1, 0, 4);
        use_on(sign_pos_ - UP, UP, SIGN);
        check("placing a sign starts typing its text", at(sign_pos_) == SIGN_POST && g.sign_editing == sign_pos_);
    }
    if (f == 1383) {
        check("the sign screen is open", app.screen() != nullptr && g.paused);
        if (Screen* s = app.screen()) {
            auto type = [&](const std::string& text) {
                for (char c : text) s->character(static_cast<unsigned char>(c));
            };
            type("Welcome to");
            s->key(GLFW_KEY_DOWN);
            type("PetrutzCraft");
            s->key(GLFW_KEY_DOWN);
            type("Beta 1.7.3");
            g.hud->show_message("", 0);
            shot(app, "m11_sign_editor.png");
            s->key(GLFW_KEY_ESCAPE);
        }
    }
    if (f == 1385) {
        const SignText* t = w.chunk_at(sign_pos_) ? &w.chunk_at(sign_pos_)->signs[cell_index(sign_pos_)] : nullptr;
        check("the typed text is on the sign", t && (*t)[0] == "Welcome to" && (*t)[1] == "PetrutzCraft" && (*t)[2] == "Beta 1.7.3");
        check("the sign screen closed", !app.screen() && !g.paused && !g.sign_editing);
        // Open the chest and put things in it.
        use_on(chest_pos_, UP, STICK);
        check("right-clicking a double chest opens its large screen",
              g.gui && g.gui->kind == InventoryScreen::Kind::Chest && g.gui->crafting && g.gui->crafting->chest_slots() == 2 * CHEST_SLOTS);
        if (g.gui && g.gui->crafting) {
            CraftingMenu& m = *g.gui->crafting;
            m.at({Slot::Chest, 0}) = Stack{DIAMOND, 5};
            m.at({Slot::Chest, 4}) = Stack{IRON_INGOT, 12};
            m.at({Slot::Chest, 30}) = Stack{RECORD_13, 1};
            m.at({Slot::Chest, 52}) = Stack{RED_WOOL, 64};
            g.inventory.slots[0] = Stack{COBBLE, 64};
            m.click({Slot::Inv, 0}, 0, true);
            check("shift-click moves a stack from the inventory into the chest",
                  !g.inventory.slots[0] && m.at({Slot::Chest, 1}) && m.at({Slot::Chest, 1})->item == COBBLE);
        }
        g.hud->show_message("", 0);
    }
    if (f == 1387) shot(app, "m11_chest.png");
    if (f == 1388) {
        g.close_gui();
        // Open the door, then look at it all.
        use_on(o + glm::ivec3(3, 0, 7), UP, STICK);
        check("right-clicking a wooden door opens both halves", door_open(w.get_data(o.x + 3, o.y, o.z + 7)) &&
                                                                    door_open(w.get_data(o.x + 3, o.y + 1, o.z + 7)));
        use_on(o + glm::ivec3(7, 0, 7), UP, STICK);
        check("an iron door doesn't open by hand", !door_open(w.get_data(o.x + 7, o.y, o.z + 7)));
        stand(-1, -1);
        g.yaw = 0;
        g.pitch = 14;
        g.inventory.slots[g.inventory.selected].reset();
        g.hud->show_message("", 0);
    }
    if (f == 1390) shot(app, "m11_building.png");
    if (f == 1391) {
        // Breaking a door's bottom drops one door and takes the top too; the chest spills.
        const size_t before = g.drops.size();
        g.break_block(o + glm::ivec3(3, 0, 7), WOOD_DOOR_BLOCK);
        int doors = 0;
        for (size_t i = before; i < g.drops.size(); ++i) doors += g.drops[i]->stack.item == WOOD_DOOR;
        check("breaking a door removes both halves and drops one door", doors == 1 && at(o + glm::ivec3(3, 1, 7)) == AIR);
        // The stone behind the ladders goes: they fall off.
        g.break_block(o + glm::ivec3(-9, 3, 2), STONE);
        check("a ladder falls when its wall goes", at(o + glm::ivec3(-8, 3, 2)) == AIR && at(o + glm::ivec3(-8, 2, 2)) == LADDER);
        // Falling down a ladder is slow.
        p.x = o.x - 8 + 0.5;
        p.z = o.z + 2 + 0.5;
        p.y = o.y + 1;
        p.vy = -10;
        g.update(0.05);
        --g.frame;  // that update was extra: keep the scene's frame numbers
        check("a ladder slows a fall", p.vy >= -3.01);
        // Soul sand.
        set(o + glm::ivec3(-1, -1, 0), SOUL_SAND);
        check("soul sand is lower than a block", block_height(SOUL_SAND) < 1.0f);
    }
    if (f == 1392) {
        // Back to where the farm scene left off.
        g.drops.clear();
        p.x = build_return_.x;
        p.y = build_return_.y;
        p.z = build_return_.z;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
        g.weather = build_weather_;
        g.level.time = build_time_;
        g.creative = build_creative_;
        g.renderer->load_all(p.x, p.z, 2);
    }
}

void AutoTest::tour(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    World& w = g.world;
    const TerrainGenerator& gen = w.generator();
    auto fly_to = [&](int x, int z, double up, double pitch) {
        p.x = x + 0.5;
        p.z = z + 0.5;
        p.y = w.surface_y(x, z) + up;
        p.vy = 0;
        g.yaw = 45;
        g.pitch = pitch;
    };
    auto column_has = [&](int x, int z, BlockId b) {
        for (int y = 0; y < HEIGHT; ++y)
            if (w.get_block(x, y, z) == b) return true;
        return false;
    };
    if (f == 525) tour_start_ = {p.x, p.y, p.z};
    if (f == 530) {
        auto at = find_biome(gen, static_cast<int>(p.x), static_cast<int>(p.z), {Biome::DESERT});
        check("found a desert", at.has_value());
        if (at) {
            fly_to(at->x, at->y, 14, 30);
            int columns = 0, sand = 0, sandstone = 0;
            for (int dx = -10; dx <= 10; ++dx)
                for (int dz = -10; dz <= 10; ++dz) {
                    int x = at->x + dx, z = at->y + dz;
                    if (gen.biome_at(x, z) != Biome::DESERT) continue;
                    ++columns;
                    sand += w.get_block(x, w.surface_y(x, z) - 1, z) == SAND;
                    sandstone += column_has(x, z, SANDSTONE);
                }
            check("desert is sand on top with sandstone under it (" + std::to_string(sand) + " sand, " +
                      std::to_string(sandstone) + " sandstone of " + std::to_string(columns) + " columns)",
                  sand > columns / 2 && sandstone > columns / 2);
            shot(app, "m4_desert.png");
        }
    }
    if (f == 540) {
        // Hills: the nearest ground above y 100, seen from the side.
        std::optional<glm::ivec2> hill;
        int x0 = static_cast<int>(tour_start_.x), z0 = static_cast<int>(tour_start_.z);
        for (int r = 0; r < 3000 && !hill; r += 24)
            for (int i = -r; i <= r && !hill; i += 24)
                for (glm::ivec2 d : {glm::ivec2(i, -r), glm::ivec2(i, r), glm::ivec2(-r, i), glm::ivec2(r, i)})
                    if (!hill && gen.density(x0 + d.x, 100, z0 + d.y) > 0) hill = glm::ivec2(x0 + d.x, z0 + d.y);
        check("found hills above y 100", hill.has_value());
        if (hill) {
            fly_to(hill->x - 30, hill->y - 30, 0, 5);
            p.y = std::max(p.y + 4, 92.0);
            face(g, hill->x, hill->y, 5);
            shot(app, "m4_hills.png");
        }
    }
    if (f == 550) {
        auto at = find_biome(gen, static_cast<int>(p.x), static_cast<int>(p.z), {Biome::TAIGA});
        check("found a taiga", at.has_value());
        if (at) {
            fly_to(at->x, at->y, 16, 35);
            int logs = 0, ice = 0, unfrozen = 0;
            for (int dx = -40; dx <= 40; ++dx)
                for (int dz = -40; dz <= 40; ++dz) {
                    int x = at->x + dx, z = at->y + dz, top = w.surface_y(x, z);
                    for (int y = std::max(0, top - 12); y <= std::min(HEIGHT - 1, top + 12); ++y) {
                        BlockId b = w.get_block(x, y, z);
                        logs += b == SPRUCE_LOG;
                    }
                    for (int y = HEIGHT - 1; y > 0; --y) {
                        BlockId b = w.get_block(x, y, z);
                        if (b == AIR) continue;
                        ice += b == ICE;
                        unfrozen += b == WATER && is_cold(gen.biome_at(x, z));
                        break;
                    }
                }
            check("taiga has spruce trees (" + std::to_string(logs) + " spruce logs)", logs > 0);
            check("water is frozen in cold biomes (" + std::to_string(ice) + " ice on top, " + std::to_string(unfrozen) +
                      " open water)", unfrozen == 0);
            shot(app, "m4_taiga.png");
        }
    }
    if (f == 558) {
        // A cave: an open pocket underground, at least 5 blocks of air in a row.
        std::optional<glm::ivec3> cave;
        int cx0 = chunk_of(static_cast<int>(tour_start_.x)), cz0 = chunk_of(static_cast<int>(tour_start_.z));
        for (int r = 0; r <= 6 && !cave; ++r)
            for (int cx = cx0 - r; cx <= cx0 + r && !cave; ++cx)
                for (int cz = cz0 - r; cz <= cz0 + r && !cave; ++cz)
                    for (int i = 0; i < CHUNK * CHUNK && !cave; i += 3)
                        for (int y = 16; y < 50 && !cave; ++y) {
                            int x = cx * CHUNK + (i & 15), z = cz * CHUNK + (i >> 4);
                            bool open = true;
                            for (int k = -3; k <= 3; ++k)
                                open &= w.get_block(x + k, y, z) == AIR && w.get_block(x + k, y + 1, z) == AIR;
                            if (open && w.get_block(x, y - 1, z) != AIR && w.get_block(x, y + 8, z) == STONE)
                                cave = glm::ivec3(x, y, z);
                        }
        check("found a cave underground", cave.has_value());
        if (cave) {
            // Caves are dark: light this one with torches on its floor.
            for (int k : {-3, 3}) {
                glm::ivec3 t = *cave + glm::ivec3(k, 0, 0);
                if (is_opaque(w.get_block(t.x, t.y - 1, t.z)))
                    g.renderer->rebuild(w.set_block(t.x, t.y, t.z, TORCH));
            }
            p.x = cave->x + 0.5;
            p.y = cave->y;
            p.z = cave->z + 0.5;
            p.vy = 0;
            g.yaw = 90;
            g.pitch = 0;
            cave_ = *cave;
        }
    }
    if (f == 565 && cave_) shot(app, "m4_cave.png");
    if (f == 575) {
        // Ores and dungeons, counted over the chunks around the start.
        int ores[BLOCK_COUNT] = {};
        int spawners = 0;
        int cx0 = chunk_of(static_cast<int>(tour_start_.x)), cz0 = chunk_of(static_cast<int>(tour_start_.z));
        for (int cx = cx0 - 5; cx <= cx0 + 5; ++cx)
            for (int cz = cz0 - 5; cz <= cz0 + 5; ++cz)
                for (BlockId b : w.chunk(cx, cz).blocks) {
                    ++ores[b];
                    spawners += b == SPAWNER;
                }
        char buf[200];
        std::snprintf(buf, sizeof buf, "in 121 chunks: coal %d, iron %d, gold %d, redstone %d, diamond %d, lapis %d, spawners %d",
                      ores[COAL_ORE], ores[IRON_ORE], ores[GOLD_ORE], ores[REDSTONE_ORE], ores[DIAMOND_ORE], ores[LAPIS_ORE],
                      spawners);
        log(buf);
        check("every ore is generated", ores[COAL_ORE] && ores[IRON_ORE] && ores[GOLD_ORE] && ores[REDSTONE_ORE] &&
                                            ores[DIAMOND_ORE] && ores[LAPIS_ORE]);
    }
    if (f == 590) {
        // Back to where the tour started, standing on the ground for the third-person picture.
        g.flying = false;
        p.x = tour_start_.x;
        p.z = tour_start_.z;
        p.y = w.surface_y(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.z)));
        p.vy = 0;
    }
}

void AutoTest::crafting(App& app, Game& g) {
    using Kind = InventoryScreen::Kind;
    auto inv_slot = [](int i) { return Slot{Slot::Inv, i}; };
    auto grid = [](int i) { return Slot{Slot::Grid, i}; };
    const Slot result{Slot::Result, 0};

    // The inventory: logs into planks (shift-click crafts them all), planks into a crafting table.
    g.inventory.selected = 0;
    g.inventory.slots[30] = Stack{LOG, 3};
    g.inventory.slots[31] = Stack{IRON_CHESTPLATE, 1};
    g.open_gui(Kind::Inventory);
    check("E opens the inventory", g.gui && g.gui->kind == Kind::Inventory);
    if (!g.gui) return;
    CraftingMenu& inv = *g.gui->crafting;
    int planks_before = g.inventory.count(PLANKS);
    inv.click(inv_slot(30), 0, false);  // pick up the logs
    inv.click(grid(0), 0, false);       // put them in the grid
    check("logs in the grid make planks", inv.result == Stack{PLANKS, 4});
    inv.click(result, 0, true);
    check("shift-clicking the result crafts all the planks (" + std::to_string(g.inventory.count(PLANKS) - planks_before) + ")",
          g.inventory.count(PLANKS) - planks_before == 12 && !inv.grid[0]);
    inv.click(inv_slot(31), 0, false);
    inv.click(Slot{Slot::Armor, 1}, 0, false);
    check("the chestplate goes in its armor slot", g.inventory.armor[1] == Stack{IRON_CHESTPLATE, 1});
    int planks_slot = g.inventory.find(PLANKS);
    inv.click(inv_slot(planks_slot), 0, false);
    for (int i = 0; i < 4; ++i) inv.click(grid(i), 1, false);  // right click: one plank in each cell
    check("2x2 planks make a crafting table", inv.result == Stack{CRAFTING_TABLE, 1});
    inv.click(inv_slot(planks_slot), 0, false);  // put the rest back
    shot(app, "10_inventory.png");
    inv.click(result, 0, false);
    check("the crafting table is on the mouse", inv.cursor == Stack{CRAFTING_TABLE, 1});
    inv.click(inv_slot(0), 0, false);
    // Something left in the grid goes back into the inventory when the screen closes.
    inv.click(inv_slot(planks_slot), 1, false);  // half the planks
    inv.click(grid(3), 0, false);
    int planks_total = g.inventory.count(PLANKS) + (inv.grid[3] ? inv.grid[3]->count : 0);
    g.close_gui();
    check("closing the inventory puts grid items back", !g.gui && g.inventory.count(PLANKS) == planks_total);

    // Place the crafting table in front of the player and right-click it.
    g.yaw = 0;
    g.pitch = 0;
    glm::vec3 e = g.eye();
    glm::ivec3 table{static_cast<int>(std::floor(e.x)), static_cast<int>(std::floor(e.y)), static_cast<int>(std::floor(e.z)) + 2};
    g.renderer->rebuild(g.world.set_block(table.x, table.y, table.z, AIR));
    g.renderer->rebuild(g.world.set_block(table.x, table.y, table.z - 1, AIR));
    g.renderer->rebuild(g.world.set_block(table.x, table.y, table.z, CRAFTING_TABLE));
    g.update_target();
    g.use_item();
    check("right-clicking a crafting table opens its 3x3 grid", g.gui && g.gui->kind == Kind::CraftingTable);
    if (!g.gui) return;
    CraftingMenu& table_menu = *g.gui->crafting;
    g.inventory.slots[32] = Stack{STICK, 2};
    planks_slot = g.inventory.find(PLANKS);
    table_menu.click(inv_slot(planks_slot), 0, false);
    for (int i = 0; i < 3; ++i) table_menu.click(grid(i), 1, false);
    table_menu.click(inv_slot(planks_slot), 0, false);
    table_menu.click(inv_slot(32), 0, false);
    table_menu.click(grid(4), 1, false);
    table_menu.click(grid(7), 1, false);
    check("planks and sticks make a wooden pickaxe", table_menu.result == Stack{WOOD_PICKAXE, 1});
    shot(app, "11_crafting_table.png");
    table_menu.click(result, 0, false);
    table_menu.click(inv_slot(1), 0, false);
    check("the pickaxe is in the hotbar", g.inventory.slots[1] == Stack{WOOD_PICKAXE, 1});
    g.close_gui();
    g.renderer->rebuild(g.world.set_block(table.x, table.y, table.z, AIR));

    // The creative item picker.
    g.creative = true;
    g.open_gui(Kind::Creative);
    check("E in creative opens the item picker", g.gui && g.gui->kind == Kind::Creative);
    if (g.gui) {
        const auto& items = all_items();
        int tnt = static_cast<int>(std::find(items.begin(), items.end(), TNT) - items.begin());
        g.gui->creative->click_item(tnt, 0, false);
        g.gui->creative->click_hotbar(8, 0, false);
        check("picking TNT puts a stack in the creative hotbar", g.creative_inventory.slots[8] == Stack{TNT, 64});
        shot(app, "12_creative_items.png");
        g.close_gui();
    }
    g.creative = false;
}

void AutoTest::furnaces(App& app, Game& g) {
    const long f = g.frame;
    World& w = g.world;
    auto set = [&](glm::ivec3 c, BlockId b) { g.renderer->rebuild(w.set_block(c.x, c.y, c.z, b)); };
    if (f == 345) {
        // Place a furnace from the hotbar two blocks ahead (against a stone block), looking south.
        g.yaw = 0;
        g.pitch = 0;
        const glm::vec3 e = g.eye();
        furnace_ = {static_cast<int>(std::floor(e.x)), static_cast<int>(std::floor(e.y)), static_cast<int>(std::floor(e.z)) + 2};
        set(furnace_ - glm::ivec3(0, 0, 1), AIR);
        set(furnace_, AIR);
        set(furnace_ + glm::ivec3(0, 0, 1), STONE);
        set(furnace_ - glm::ivec3(0, 1, 0), STONE);
        g.inventory.slots[2] = Stack{FURNACE, 1};
        g.inventory.selected = 2;
        g.update_target();
        g.use_item();
        check("a placed furnace faces the player", w.get_block(furnace_.x, furnace_.y, furnace_.z) == furnace_block(2, false));
        g.update_target();
        g.use_item();
        check("right-clicking a furnace opens its screen", g.gui && g.gui->kind == InventoryScreen::Kind::Furnace);
        if (!g.gui) return;
        // Iron ore in, coal as fuel, then 10 seconds of game ticks.
        CraftingMenu& menu = *g.gui->crafting;
        g.inventory.slots[33] = Stack{IRON_ORE, 3};
        g.inventory.slots[34] = Stack{COAL, 1};
        menu.click({Slot::Inv, 33}, 0, false);
        menu.click({Slot::Furnace, FURNACE_INPUT}, 0, false);
        menu.click({Slot::Inv, 34}, 0, false);
        menu.click({Slot::Furnace, FURNACE_FUEL}, 0, false);
        for (int i = 0; i < SMELT_TICKS + 60; ++i) g.tick_furnaces();
        const Furnace* fur = w.furnace_at(furnace_);
        check("10 seconds in a lit furnace smelts iron ore into an ingot",
              fur && fur->output == Stack{IRON_INGOT, 1} && fur->input == Stack{IRON_ORE, 2} && !fur->fuel);
        check("a burning furnace is lit and glows (light " +
                  std::to_string(w.block_light(furnace_.x, furnace_.y, furnace_.z - 1)) + " in front)",
              is_lit_furnace(w.get_block(furnace_.x, furnace_.y, furnace_.z)) &&
                  w.block_light(furnace_.x, furnace_.y, furnace_.z - 1) == 12);
        shot(app, "m7_furnace_screen.png");
        menu.click({Slot::Furnace, FURNACE_OUTPUT}, 0, true);
        check("shift-clicking the output takes the ingot", g.inventory.count(IRON_INGOT) == 1 && !fur->output);
        g.close_gui();
        check("the furnace keeps smelting with its screen closed", !g.gui && w.furnace_at(furnace_)->burning());
        // Glass next to it, and night, for a picture of the glowing furnace.
        set(furnace_ + glm::ivec3(1, 0, 0), GLASS);
        set(furnace_ + glm::ivec3(-1, 0, 0), GLASS);
        furnace_time_ = g.level.time;
        g.level.time = 18000;
        g.hud->show_message("", 0);
        g.player.z -= 1.5;
        g.pitch = 15;
    }
    if (f == 347) shot(app, "m7_lit_furnace.png");
    if (f == 348) {
        g.level.time = furnace_time_;
        g.player.z += 1.5;
        const size_t before = g.drops.size();
        g.break_block(furnace_, w.get_block(furnace_.x, furnace_.y, furnace_.z));
        check("breaking a furnace drops what was in it",
              g.drops.size() > before && g.drops.back()->stack == Stack{IRON_ORE, 2} && !w.furnace_at(furnace_));
        const size_t after = g.drops.size();
        g.break_block(furnace_ + glm::ivec3(1, 0, 0), GLASS);
        g.break_block(furnace_ + glm::ivec3(-1, 0, 0), GLASS);
        check("glass drops nothing", g.drops.size() == after);
    }
}

void AutoTest::tools(App& app, Game& g) {
    const long f = g.frame;
    World& w = g.world;
    Inventory& inv = g.inventory;
    auto set = [&](int x, int y, int z, BlockId b) { g.renderer->rebuild(w.set_block(x, y, z, b)); };
    // The range, facing +z: open air from the player to a stone wall 5 blocks ahead, a stone floor
    // 2 below eye level. What was there before is put back afterwards.
    auto build = [&] {
        const glm::ivec3 o = range_;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = 1; dz <= 5; ++dz)
                for (int dy = -2; dy <= 1; ++dy) {
                    glm::ivec3 c = o + glm::ivec3(dx, dy, dz);
                    range_blocks_.push_back({c, w.get_block(c.x, c.y, c.z)});
                    set(c.x, c.y, c.z, (dz == 5 || dy == -2) ? STONE : AIR);
                }
    };

    if (f == 600) {
        g.flying = false;
        g.creative = false;
        g.yaw = 0;
        g.pitch = 0;
        glm::vec3 e = g.eye();
        range_ = {static_cast<int>(std::floor(e.x)), static_cast<int>(std::floor(e.y)), static_cast<int>(std::floor(e.z))};
        build();

        // Stone drops nothing by hand, and cobblestone with a pickaxe, which wears.
        glm::ivec3 rock = range_ + glm::ivec3(0, 0, 2);
        inv.slots[8].reset();
        inv.selected = 8;
        size_t drops = g.drops.size();
        set(rock.x, rock.y, rock.z, STONE);
        g.break_block(rock, STONE);
        check("stone mined by hand drops nothing", g.drops.size() == drops);
        inv.slots[8] = Stack{WOOD_PICKAXE, 1};
        set(rock.x, rock.y, rock.z, STONE);
        g.break_block(rock, STONE);
        check("stone mined with a wooden pickaxe drops cobblestone",
              g.drops.size() == drops + 1 && g.drops.back()->stack.item == COBBLE);
        check("the pickaxe wore by one use", inv.slots[8] && inv.slots[8]->damage == 1);
        inv.slots[8] = Stack{WOOD_PICKAXE, 1};
        set(rock.x, rock.y, rock.z, IRON_ORE);
        g.break_block(rock, IRON_ORE);
        check("iron ore needs a stone pickaxe", g.drops.size() == drops + 1);
        g.drops.pop_back();

        // A full set of armor takes 80% off: 10 damage becomes 2.
        for (int p = 0; p < 4; ++p) inv.armor[p] = Stack{armor(Material::Iron, static_cast<ArmorPiece>(p)), 1};
        g.vitals.health = MAX_HEALTH;
        g.vitals.hurt_timer = 0;
        g.armor_remainder = 0;
        check("the armor bar is full", armor_value(inv) == 20);
        g.hurt_player(10, true);
        check("full iron armor turns 10 damage into 2 (health " + std::to_string(g.vitals.health) + ")",
              g.vitals.health == MAX_HEALTH - 2 && inv.armor[0]->damage == 10);
        g.vitals.health = MAX_HEALTH;
        // A mixed set, for the third-person picture.
        inv.armor = {Stack{DIAMOND_HELMET, 1}, Stack{GOLD_CHESTPLATE, 1}, Stack{IRON_LEGGINGS, 1}, Stack{LEATHER_BOOTS, 1}};

        // The bow fires at once and uses up an arrow.
        inv.slots[6] = Stack{BOW, 1};
        inv.slots[7] = Stack{ARROW, 5};
        inv.selected = 6;
        arrows_before_ = inv.count(ARROW);
        g.use_item();
        g.yaw = 6;
        g.pitch = -4;
        g.use_item();
        g.yaw = 0;
        check("the bow shot two arrows", g.arrows.size() == 2 && inv.count(ARROW) == arrows_before_ - 2);
        check("the bow wore by two uses", inv.slots[6] && inv.slots[6]->damage == 2);
    }
    if (f == 612) {
        bool stuck = !g.arrows.empty();
        for (const auto& a : g.arrows) stuck &= a->stuck && a->stuck_in.z == range_.z + 5;
        check("the arrows stuck in the wall", stuck);

        // Flint and steel lights a fire on the floor in front of the wall.
        inv.slots[7] = Stack{FLINT_AND_STEEL, 1};
        inv.selected = 7;
        glm::ivec3 floor = range_ + glm::ivec3(0, -2, 4);
        face(g, floor.x + 0.5, floor.z + 0.5, 0);
        glm::vec3 e = g.eye();
        g.pitch = degrees(std::atan2(e.y - (floor.y + 1), std::hypot(floor.x + 0.5 - e.x, floor.z + 0.5 - e.z)));
        g.update_target();
        g.use_item();
        check("flint and steel lights a fire", w.get_block(floor.x, floor.y + 1, floor.z) == FIRE);
        check("fire gives light 15", w.block_light(floor.x, floor.y + 1, floor.z) == 15);

        // A water bucket pours water next to it; an empty bucket scoops it up again.
        glm::ivec3 pool = range_ + glm::ivec3(1, -2, 3);
        inv.slots[8] = Stack{WATER_BUCKET, 1};
        inv.selected = 8;
        face(g, pool.x + 0.5, pool.z + 0.5, 0);
        g.pitch = degrees(std::atan2(e.y - (pool.y + 1), std::hypot(pool.x + 0.5 - e.x, pool.z + 0.5 - e.z)));
        g.update_target();
        g.use_item();
        check("a water bucket pours water and leaves an empty bucket",
              w.get_block(pool.x, pool.y + 1, pool.z) == WATER && inv.slots[8] == Stack{BUCKET, 1});
        g.use_item();
        check("an empty bucket scoops the water back up",
              w.get_block(pool.x, pool.y + 1, pool.z) == AIR && inv.slots[8] == Stack{WATER_BUCKET, 1});

        inv.selected = 6;  // the bow, in the hotbar picture
        g.yaw = 0;
        g.pitch = 12;
        shot(app, "m3_bow_fire.png");
        // From the side, to see the arrows sticking out of the wall.
        g.player.x -= 1.0;
        g.player.z += 3.2;
        face(g, range_.x + 0.5, range_.z + 5, 10);
        shot(app, "m3_arrows.png");
        g.player.x += 1.0;
        g.player.z -= 3.2;
    }
    if (f == 614) {
        for (auto it = range_blocks_.rbegin(); it != range_blocks_.rend(); ++it) set(it->first.x, it->first.y, it->first.z, it->second);
        range_blocks_.clear();
        for (const auto& a : g.arrows) inv.add(ARROW, 1);  // picked up, as if walked over
        g.arrows.clear();
        g.pitch = 0;
    }
}

void AutoTest::fluids_and_fire(App& app, Game& g) {
    const long f = g.frame;
    Body& p = g.player;
    World& w = g.world;
    auto set = [&](glm::ivec3 c, BlockId b) { g.renderer->rebuild(w.set_block(c.x, c.y, c.z, b)); };
    auto at = [&](glm::ivec3 c) { return w.get_block(c.x, c.y, c.z); };
    auto run_ticks = [&](int n) {
        for (int i = 0; i < n; ++i) g.tick_blocks();
    };
    const glm::ivec3& o = pad_;
    auto stand = [&](double dx, double dz) {
        p.x = o.x + 0.5 + dx;
        p.y = o.y;
        p.z = o.z + 0.5 + dz;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
    };
    // The scene, relative to the middle of the platform's near edge (the player looks along +z).
    const glm::ivec3 spring = o + glm::ivec3(-4, 3, 7);  // water on top of a 3-high pillar
    const glm::ivec3 lava = o + glm::ivec3(4, 0, 6), lava_water = lava + glm::ivec3(1, 0, 0);
    const glm::ivec3 sand_top = o + glm::ivec3(0, 4, 5);  // sand and gravel held up by one stone
    const glm::ivec3 hut = o + glm::ivec3(3, 0, 11);      // a little plank hut that catches fire

    if (f == 1200) {
        // A stone platform at y = 99 with nothing around it, so nothing else flows in, and wide
        // enough that the water never runs off its edge.
        // Fire spreading is down to chance: start from a fixed random state so what happened
        // earlier in the test doesn't change how far it gets.
        g.rng = Rng(1);
        g.monsters.clear();
        g.target_monster = nullptr;
        g.weather.clear();
        g.creative = true;
        g.flying = false;
        g.vitals.reset();
        g.burn_ticks = 0;
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;
        // Somewhere the ground is well below it (not inside a mountain).
        pad_ = {static_cast<int>(std::floor(p.x)) + 40, 100, static_cast<int>(std::floor(p.z))};
        for (int tries = 0; tries < 20; ++tries) {
            int top = 0;
            for (int dx = -14; dx <= 14; dx += 2)
                for (int dz = -6; dz <= 20; dz += 2) top = std::max(top, w.surface_y(pad_.x + dx, pad_.z + dz));
            if (top < 92) break;
            pad_.x += 48;
        }
        const glm::ivec3 q = pad_;
        for (int dx = -14; dx <= 14; ++dx)
            for (int dz = -6; dz <= 20; ++dz) set(q + glm::ivec3(dx, -1, dz), STONE);
        for (int dy = 0; dy < 3; ++dy) set(q + glm::ivec3(-4, dy, 7), STONE);
        set(q + glm::ivec3(-4, 3, 7), WATER);
        set(q + glm::ivec3(4, 0, 6), LAVA);
        set(q + glm::ivec3(0, 3, 5), STONE);
        for (int dy = 4; dy < 7; ++dy) set(q + glm::ivec3(0, dy, 5), dy == 6 ? GRAVEL : SAND);
        for (int dx = 0; dx < 3; ++dx)
            for (int dz = 0; dz < 3; ++dz) {
                set(q + glm::ivec3(3 + dx, 0, 11 + dz), PLANKS);
                if (dx != 1 || dz != 1) set(q + glm::ivec3(3 + dx, 1, 11 + dz), dz == 2 ? LOG : PLANKS);
            }
        stand(0, -3);
        g.yaw = 0;
        g.pitch = 25;
        g.hud->show_message("", 0);
    }
    if (f == 1202) {
        // Pour water beside the lava, knock out the stone under the sand, light the hut.
        set(lava_water, WATER);
        set(sand_top - glm::ivec3(0, 1, 0), AIR);
        set(hut + glm::ivec3(1, 1, 1), FIRE);
        run_ticks(12);  // each block of the column lets go 3 ticks after the one under it
        check("lava touching water turns to obsidian", at(lava) == OBSIDIAN);
        check("the sand and gravel start falling", g.falling.size() == 3 && at(sand_top) == AIR);
        g.pitch = 0;
    }
    if (f == 1204) shot(app, "m6_sand_falling.png");
    if (f >= 1205 && f < 1230) run_ticks(12);  // fast-forward the water and the fire (300 game ticks)
    if (f == 1230) {
        check("water falls off its pillar", at(spring + glm::ivec3(1, 0, 0)) == WATER && at(spring + glm::ivec3(1, -2, 0)) == WATER &&
                                                 w.get_data(spring.x + 1, spring.y - 2, spring.z) >= 8);
        check("and spreads out over the floor", at(spring + glm::ivec3(5, -3, 0)) == WATER && at(spring + glm::ivec3(1, -3, 4)) == WATER);
        int fire = 0, burnt = 0;
        for (int dx = -1; dx <= 3; ++dx)
            for (int dz = -1; dz <= 3; ++dz)
                for (int dy = 0; dy <= 3; ++dy) {
                    const BlockId b = at(hut + glm::ivec3(dx, dy, dz));
                    fire += b == FIRE;
                    burnt += dy <= 1 && dx >= 0 && dx < 3 && dz >= 0 && dz < 3 && !(dx == 1 && dz == 1 && dy == 1) && b != PLANKS &&
                             b != LOG;
                }
        check("the fire spread through the hut (" + std::to_string(fire) + " fires, " + std::to_string(burnt) + " blocks burnt)",
              fire > 1 && burnt > 0);
        g.yaw = -12;
        g.pitch = 22;
        shot(app, "m6_water_lava_fire.png");
    }
    if (f == 1232) {
        // Drowning: a survival player with their head under water loses air, then health.
        g.creative = false;
        g.vitals.reset();
        for (int dy = 0; dy < 3; ++dy) set(o + glm::ivec3(-8, dy, 12), WATER);
        stand(-8, 12);
        g.vitals.air = 20;
        for (int i = 0; i < 45; ++i) g.tick_player_fire_and_air();
        check("under water the air runs out and the player drowns (health " + std::to_string(g.vitals.health) + ")",
              g.head_under_water() && g.vitals.air <= 0 && g.vitals.health == MAX_HEALTH - DROWN_DAMAGE);
        g.vitals.air = 160;
        g.yaw = 90;
        g.pitch = 0;
    }
    if (f == 1256) shot(app, "m6_underwater.png");  // once the red flash of the hurt has faded
    if (f == 1258) {
        for (int dy = 0; dy < 3; ++dy) set(o + glm::ivec3(-8, dy, 12), AIR);
        // Lava burns 4 at a time and sets the player alight; water puts the fire out.
        g.vitals.reset();
        const glm::ivec3 pool = o + glm::ivec3(-7, 0, 0);
        set(pool, LAVA);
        set(pool + glm::ivec3(0, 1, 0), LAVA);
        stand(-7, 0);
        p.check_water();
        g.tick_player_fire_and_air();
        check("lava burns the player and sets them on fire", g.vitals.health == MAX_HEALTH - LAVA_DAMAGE && g.burn_ticks > 0);
        set(pool, STONE);
        set(pool + glm::ivec3(0, 1, 0), AIR);
        stand(-3, -3);
        g.vitals.hurt_timer = 0;
        g.yaw = 0;
        g.pitch = 25;
    }
    if (f == 1284) shot(app, "m6_burning.png");
    if (f == 1286) {
        check("burning keeps hurting after leaving the lava (health " + std::to_string(g.vitals.health) + ")",
              g.burn_ticks > 0 && g.vitals.health <= MAX_HEALTH - LAVA_DAMAGE);
        p.in_water = true;
        g.tick_player_fire_and_air();
        check("water puts the burning player out", g.burn_ticks == 0);
        g.vitals.reset();
        // A current: stand still in the stream running away from the pillar.
        stand(-1.5, 7);
        current_start_x_ = p.x;
    }
    if (f == 1316) {
        check("the falling sand and gravel land as blocks",
              g.falling.empty() && at(o + glm::ivec3(0, 0, 5)) == SAND && at(o + glm::ivec3(0, 1, 5)) == SAND &&
                  at(o + glm::ivec3(0, 2, 5)) == GRAVEL);
        check("the current carries the player downstream (" + std::to_string(p.x - current_start_x_) + " blocks)",
              p.x - current_start_x_ > 0.3);
        // Lightning on Normal sets fire where it strikes.
        const glm::ivec3 bolt = o + glm::ivec3(8, 0, 0);
        g.strike(glm::dvec3(bolt) + glm::dvec3(0.5, 0, 0.5));
        check("lightning sets a fire where it strikes", at(bolt) == FIRE);
        g.creative = true;
        g.vitals.reset();
        g.burn_ticks = 0;
        stand(0, -3);
        g.weather = Weather(WeatherState{true, false, 100000, 100000});  // the rain the save check looks for
    }
}

void AutoTest::hand(App& app, Game& g) {
    const long f = g.frame;
    Inventory& inv = g.active_inventory();
    World& w = g.world;
    if (f == 1194) {
        // A quiet, sunny moment: no monsters, no rain, no red flash or messages over the picture.
        g.monsters.clear();
        g.target_monster = nullptr;
        hand_weather_ = g.weather;
        g.weather = Weather(WeatherState{false, false, 100000, 100000});
        hand_time_ = g.level.time;
        g.level.time = 6000;
        g.hud->update(10);
        g.hud->show_help = false;
        g.yaw = 30;
        g.pitch = -10;
        hand_slots_ = {inv.slots[0], inv.slots[1], inv.slots[2], inv.slots[3]};
        hand_selected_ = inv.selected;
        inv.slots[0].reset();
        inv.slots[1] = Stack{GRASS, 10};
        inv.slots[2] = Stack{DIAMOND_SWORD, 1};
        inv.slots[3] = Stack{TORCH, 10};
    }
    if (f == 1196) {
        // Let the hand settle on the selected slot, as it does over a few frames in play.
        auto hold = [&](int slot) {
            inv.selected = slot;
            g.hand.update(1.0, inv.selected_item(), slot);
        };
        hold(0);
        auto arm = g.hand.parts();
        check("an empty hand shows the arm", arm.size() == 1 && arm[0].model == MODEL_PLAYER_ARM);
        shot(app, "hand_empty.png");
        hold(1);
        auto held = g.hand.parts();
        check("the selected block is held in the hand", held.size() == 1 && held[0].model == HELD_MODEL_BASE + GRASS);
        shot(app, "hand_block.png");
        hold(3);
        shot(app, "hand_torch.png");
        // Switching to the sword: the torch dips out of view, then the sword comes up.
        inv.selected = 2;
        g.hand.update(0.1, inv.selected_item(), 2);
        check("switching items lowers the hand", g.hand.equipped() < 0.5f);
        shot(app, "hand_switching.png");
        hold(2);
        check("the new item comes back up", g.hand.equipped() == 1 && g.hand.shown() == std::optional<ItemId>(DIAMOND_SWORD));
        held = g.hand.parts();
        shot(app, "hand_sword.png");

        // A left click swings the hand, even at nothing.
        check("the hand is still before a click", !g.hand.swinging());
        g.on_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS);
        g.on_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE);
        check("a left click swings the hand", g.hand.swinging());
        g.hand.update(Hand::SWING_SECONDS * 0.4, inv.selected_item(), inv.selected);
        auto swung = g.hand.parts();
        check("the sword moves during the swing", swung[0].transform != held[0].transform);
        shot(app, "hand_swing.png");
        g.hand.update(Hand::SWING_SECONDS, inv.selected_item(), inv.selected);
        check("the swing ends", !g.hand.swinging() && g.hand.swing_progress() == 0);

        // Placing a block with a right click swings the hand; eating doesn't.
        const glm::ivec3 spot(static_cast<int>(std::floor(g.player.x)), static_cast<int>(std::floor(g.player.y)),
                              static_cast<int>(std::floor(g.player.z)) + 2);
        for (int dy = -1; dy <= 1; ++dy) hand_blocks_.push_back({spot + glm::ivec3(0, dy, 0), w.get_block(spot.x, spot.y + dy, spot.z)});
        g.renderer->rebuild(w.set_block(spot.x, spot.y - 1, spot.z, STONE));
        g.renderer->rebuild(w.set_block(spot.x, spot.y, spot.z, AIR));
        g.renderer->rebuild(w.set_block(spot.x, spot.y + 1, spot.z, AIR));
        face(g, spot.x + 0.5, spot.z + 0.5, 0);
        glm::vec3 e = g.eye();
        g.pitch = degrees(std::atan2(e.y - spot.y, std::hypot(spot.x + 0.5 - e.x, spot.z + 0.5 - e.z)));
        g.update_target();
        inv.selected = 1;
        g.use_item_by_hand();
        check("placing a block with a right click swings the hand",
              g.hand.swinging() && w.get_block(spot.x, spot.y, spot.z) == GRASS);
        g.hand.update(Hand::SWING_SECONDS, inv.selected_item(), inv.selected);
        inv.slots[1] = Stack{BREAD, 1};
        g.use_item_by_hand();
        check("eating doesn't swing the hand", !g.hand.swinging());

        for (auto it = hand_blocks_.rbegin(); it != hand_blocks_.rend(); ++it)
            g.renderer->rebuild(w.set_block(it->first.x, it->first.y, it->first.z, it->second));
        for (int i = 0; i < 4; ++i) inv.slots[i] = hand_slots_[i];
        inv.selected = hand_selected_;
        g.weather = hand_weather_;
        g.level.time = hand_time_;
        g.pitch = 0;
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
        const Furnace* fur = g.world.furnace_at(marker_ + glm::ivec3(0, 1, 0));
        check("a furnace keeps what is in it and its fire through saving",
              fur && fur->input && fur->input->item == SAND && fur->output && fur->output->item == GLASS && fur->fuel &&
                  fur->burning() && is_lit_furnace(g.world.get_block(marker_.x, marker_.y + 1, marker_.z)));
        const glm::ivec3 ripe = arena_ + glm::ivec3(2, 0, 6), wet = arena_ + glm::ivec3(-2, -1, 3);
        check("wheat keeps its stage and farmland its wetness",
              g.world.get_block(ripe.x, ripe.y, ripe.z) == WHEAT_CROP && g.world.get_data(ripe.x, ripe.y, ripe.z) == WHEAT_RIPE &&
                  g.world.get_block(wet.x, wet.y, wet.z) == FARMLAND && g.world.get_data(wet.x, wet.y, wet.z) > 0);
        PlayerData now = g.player_data();
        check("player position, health, hunger and inventory are restored",
              std::abs(now.pos.x - saved_player_.pos.x) < 1e-6 && std::abs(now.pos.z - saved_player_.pos.z) < 1e-6 &&
                  now.health == saved_player_.health && now.hunger == saved_player_.hunger &&
                  now.inventory == saved_player_.inventory && now.creative == saved_player_.creative);
        check("the bed spawn point and the weather are restored",
              now.bed == saved_player_.bed && now.bed && g.weather.state.raining == saved_raining_ && saved_raining_);
        check("animals near the player are back (" + std::to_string(g.animals.size()) + " of " + std::to_string(saved_animals_) + ")",
              !g.animals.empty());
        check("the tamed wolf is back, still tame and sitting",
              std::any_of(g.animals.begin(), g.animals.end(),
                          [](const auto& a) { return a->kind == AnimalKind::Wolf && a->tamed && a->sitting; }));
        check("the sheared sheep is back, still bare",
              std::any_of(g.animals.begin(), g.animals.end(),
                          [](const auto& a) { return a->kind == AnimalKind::Sheep && a->sheared; }));
        check("the zombie saved next to the player is back",
              std::any_of(g.monsters.begin(), g.monsters.end(), [&](const auto& m) {
                  return m->kind == MobKind::Zombie && std::hypot(m->body.x - g.player.x, m->body.z - g.player.z) < 6;
              }));
        // The building scene is far away: load it to look.
        g.renderer->load_all(build_.x + 0.5, build_.z + 0.5, 1);
        Chunk* sc = g.world.chunk_at(sign_pos_);
        check("a sign keeps its text through saving",
              sc && sc->signs.count(cell_index(sign_pos_)) && sc->signs[cell_index(sign_pos_)][1] == "PetrutzCraft");
        ChestSlots* cs = chest_slots(g.world, chest_pos_);
        check("a chest keeps what is in it through saving", cs && (*cs)[0] && (*cs)[0]->item == DIAMOND && (*cs)[0]->count == 5);
        Chunk* nc = g.world.chunk_at(note_pos_);
        check("a note block keeps its pitch", nc && nc->notes.count(cell_index(note_pos_)) && nc->notes[cell_index(note_pos_)] == 1);
        Chunk* jc = g.world.chunk_at(jukebox_pos_);
        check("a jukebox keeps its disc", jc && jc->records.count(cell_index(jukebox_pos_)) &&
                                              jc->records[cell_index(jukebox_pos_)] == RECORD_CAT);
        shot(app, "9_reloaded.png");
        finish(app);
    }
}

void AutoTest::transport(App& app, Game& g) {
    const long f = g.frame - 1650;  // runs after the animals
    Body& p = g.player;
    World& w = g.world;
    Inventory& inv = g.inventory;
    auto set = [&](glm::ivec3 c, BlockId b) { g.renderer->rebuild(w.set_block(c.x, c.y, c.z, b)); };
    auto at = [&](glm::ivec3 c) { return w.get_block(c.x, c.y, c.z); };
    auto hold = [&](ItemId item, int count = 1) { inv.slots[inv.selected] = Stack{item, count}; };
    auto stand = [&](glm::dvec3 q) {
        p.x = q.x;
        p.y = q.y;
        p.z = q.z;
        p.vx = p.vy = p.vz = 0;
        g.fall_peak = p.y;
    };
    // Look at a point and pick what is there as the target.
    auto look_at = [&](glm::dvec3 q) {
        glm::vec3 e = g.eye();
        glm::vec3 d = glm::vec3(q) - e;
        g.yaw = degrees(std::atan2(d.x, d.z));
        g.pitch = -degrees(std::asin(d.y / glm::length(d)));
        g.update_target();
    };
    auto aim_vehicle = [&](const Vehicle& v) {
        look_at(glm::dvec3(v.body.x, v.body.y + 0.4, v.body.z));
        return g.target_vehicle == &v;
    };
    auto count_drops = [&](ItemId item) {
        int n = 0;
        for (const auto& d : g.drops)
            if (d->stack.item == item) n += d->stack.count;
        return n;
    };
    auto run = [&](int ticks) {
        for (int i = 0; i < ticks; ++i) {
            g.tick_vehicles();
            g.update_vehicles(0);
            if (p.y > track_.y + 0.5) rode_over_hump_ = true;
            if (!g.pressed_detectors.empty()) pressed_detector_ = true;
        }
    };
    if (f == 0) {
        before_track_ = {p.x, p.y, p.z};
        track_ = {static_cast<int>(std::floor(p.x)) - 40, 98, static_cast<int>(std::floor(p.z))};  // under the clouds
    }
    // The track, from its start: a powered rail against a wall, a detector rail, a hump one block
    // high, then a curve to the south and a wall at the end.
    const glm::ivec3 o = track_;
    const glm::ivec3 detector = o + glm::ivec3(4, 0, 0), corner = o + glm::ivec3(12, 0, 0);

    if (f == 0) {
        g.monsters.clear();
        g.target_monster = nullptr;
        g.arrows.clear();
        g.drops.clear();
        g.puffs.clear();
        g.creative = false;
        g.flying = false;
        g.vitals.reset();
        g.level.time = 6000 + (g.level.time / DAY_TICKS) * DAY_TICKS;  // noon
        // Clear skies for the pictures (the rain comes back afterwards: the save check looks for it).
        hand_weather_ = g.weather;
        g.weather = Weather(WeatherState{false, false, 100000, 100000});
        const glm::ivec3 t = track_;
        // A stone platform in the sky, with a pool along its north side for the boat.
        for (int dx = -3; dx <= 20; ++dx)
            for (int dz = -8; dz <= 12; ++dz) {
                set(t + glm::ivec3(dx, -2, dz), STONE);
                set(t + glm::ivec3(dx, -1, dz), dz <= -3 && dz >= -6 && dx >= -1 && dx <= 16 ? WATER : STONE);
                for (int dy = 0; dy <= 4; ++dy) set(t + glm::ivec3(dx, dy, dz), AIR);
            }
        for (int dz = -6; dz <= -3; ++dz) set(t + glm::ivec3(17, 0, dz), STONE);  // the far end of the pool
        set(t + glm::ivec3(-1, 0, 0), STONE);       // the wall behind the start
        set(t + glm::ivec3(7, 0, 0), STONE);        // the hump
        set(t + glm::ivec3(12, 0, 10), STONE);      // the wall at the end
        // One rail put down by hand: right click on the floor.
        stand(glm::dvec3(o) + glm::dvec3(1.5, 0, -1.5));
        hold(RAIL, 64);
        look_at(glm::dvec3(o) + glm::dvec3(1.5, 0, 0.5));
        g.use_item();
        check("a rail goes down with a right click on the floor", at(o + glm::ivec3(1, 0, 0)) == RAIL &&
                                                                         inv.slots[inv.selected]->count == 63);
        // The rest of the track, laid one after another the same way.
        auto lay = [&](glm::ivec3 c, BlockId b) {
            set(c, b);
            if (b == POWERED_RAIL) g.renderer->rebuild(w.set_data(c.x, c.y, c.z, RAIL_POWERED));
            g.lay_rail(c);
        };
        lay(o, POWERED_RAIL);
        for (int dx = 2; dx <= 12; ++dx)
            lay(o + glm::ivec3(dx, dx == 7 ? 1 : 0, 0), dx == 4 ? DETECTOR_RAIL : RAIL);
        for (int dz = 1; dz <= 9; ++dz) lay(corner + glm::ivec3(0, 0, dz), RAIL);
        auto shape = [&](glm::ivec3 c) { return rail_shape(at(c), w.get_data(c.x, c.y, c.z)); };
        check("rails join into a line", shape(o + glm::ivec3(2, 0, 0)) == RAIL_EAST_WEST);
        check("rails slope up onto the hump and down again", shape(o + glm::ivec3(6, 0, 0)) == RAIL_ASCENDING_EAST &&
                                                                    shape(o + glm::ivec3(8, 0, 0)) == RAIL_ASCENDING_WEST);
        check("the corner curves", shape(corner) == RAIL_SOUTH_WEST);
    }
    if (f == 1) {
        // From above, before anything runs on it.
        stand(glm::dvec3(o) + glm::dvec3(5, 2.5, -5));
        g.flying = true;
        look_at(glm::dvec3(o) + glm::dvec3(8, 0, 3));
    }
    if (f == 3) {
        shot(app, "m12_rails.png");
        g.flying = false;
        // A minecart on the powered rail at the start, then get in.
        stand(glm::dvec3(o) + glm::dvec3(0.5, 0, -1.5));
        hold(MINECART);
        look_at(glm::dvec3(o) + glm::dvec3(0.5, 0.1, 0.5));
        g.use_item();
        check("a minecart goes on a rail", g.vehicles.size() == 1 && !inv.slots[inv.selected]);
        if (!g.vehicles.empty()) {
            Vehicle& cart = *g.vehicles[0];
            check("aiming at the minecart", aim_vehicle(cart));
            g.use_item();
            check("right-clicking a minecart gets in", g.in_vehicle == &cart);
        }
    }
    if (f == 5) {
        run(20);  // a second: the powered rail sends it off the wall
        check("the powered rail starts the minecart and the player rides along (x " + std::to_string(p.x - o.x) + ")",
              g.in_vehicle && p.x > o.x + 3);
        g.yaw = 90;  // looking down the track
        g.pitch = 20;
    }
    if (f == 7) shot(app, "m12_riding.png");
    if (f == 8) g.third_person = true;
    if (f == 10) {
        shot(app, "m12_riding_third_person.png");
        g.third_person = false;
    }
    if (f == 11) {
        run(100);
        check("the minecart pressed the detector rail on the way", pressed_detector_);
        check("the minecart went over the hump", rode_over_hump_);
        check("detector rails let go once the minecart has gone",
              !rail_powered(DETECTOR_RAIL, w.get_data(detector.x, detector.y, detector.z)));
        check("the minecart came round the curve to the end of the track (z " + std::to_string(p.z - o.z) + ")",
              g.in_vehicle && p.z > o.z + 6 && p.x > corner.x && p.x < corner.x + 1);
        Vehicle* cart = g.in_vehicle;
        g.get_out();
        check("sneaking gets out on top of the minecart", !g.in_vehicle && cart && p.y >= cart->body.y + cart->body.height - 0.01);
    }
    if (f == 13) {
        // A minecart with a chest: right click opens the chest.
        stand(glm::dvec3(o) + glm::dvec3(2.5, 0, -1.5));
        hold(STORAGE_MINECART);
        look_at(glm::dvec3(o) + glm::dvec3(2.5, 0.1, 0.5));
        g.use_item();
        Vehicle* chest = g.vehicles.size() == 2 ? g.vehicles[1].get() : nullptr;
        check("a storage minecart goes on a rail", chest && chest->kind == VehicleKind::StorageMinecart);
        if (chest) {
            check("aiming at the storage minecart", aim_vehicle(*chest));
            inv.slots[inv.selected] = Stack{COBBLE, 20};
            inv.slots[5] = Stack{IRON_INGOT, 7};
            g.use_item();
            check("right-clicking a storage minecart opens its chest", g.gui && g.gui->kind == InventoryScreen::Kind::Chest &&
                                                                          g.chest_vehicle == chest);
            if (g.gui && g.gui->crafting) {
                g.gui->crafting->click({Slot::Inv, inv.selected}, 0, true);
                g.gui->crafting->click({Slot::Inv, 5}, 0, true);
            }
            check("shift-clicking moves stacks into the chest", chest->chest[0] == Stack{COBBLE, 20} &&
                                                                    chest->chest[1] == Stack{IRON_INGOT, 7});
        }
    }
    if (f == 15) shot(app, "m12_chest.png");
    if (f == 17) {
        g.close_gui();
        // A minecart with a furnace, further along: coal sends it off, away from the player.
        stand(glm::dvec3(corner) + glm::dvec3(-0.5, 0, 1.5));
        hold(POWERED_MINECART);
        look_at(glm::dvec3(corner) + glm::dvec3(0.5, 0.1, 2.5));
        g.use_item();
        Vehicle* engine = g.vehicles.size() == 3 ? g.vehicles[2].get() : nullptr;
        check("a powered minecart goes on a rail", engine && engine->kind == VehicleKind::PoweredMinecart);
        if (engine) {
            hold(COAL, 3);
            check("aiming at the powered minecart", aim_vehicle(*engine));
            g.use_item();
            check("coal goes into the powered minecart", engine->fuel == COAL_FUEL_TICKS && inv.slots[inv.selected]->count == 2);
            const double z0 = engine->body.z;
            g.puffs.clear();
            run(40);
            check("the powered minecart drives itself along the rails (" + std::to_string(engine->body.z - z0) + " blocks)",
                  engine->body.z > z0 + 2);
            check("and smokes while it burns", !g.puffs.empty());
        }
    }
    if (f == 19) {
        // Hitting the storage minecart breaks it: it drops a minecart, the chest and what was in it.
        Vehicle* chest = g.vehicles.size() >= 2 ? g.vehicles[1].get() : nullptr;
        if (chest) {
            stand(glm::dvec3(chest->body.x, o.y, chest->body.z - 1.5));
            g.drops.clear();
            inv.slots[inv.selected].reset();  // bare hands
            int hits = 0;
            while (hits < 20 && std::find_if(g.vehicles.begin(), g.vehicles.end(), [&](const auto& v) { return v.get() == chest; }) !=
                                    g.vehicles.end()) {
                aim_vehicle(*chest);
                g.attack_cooldown = 0;
                g.try_attack();
                ++hits;
            }
            check("five punches break a minecart (" + std::to_string(hits) + ")", hits == 5);
            check("a broken storage minecart drops the minecart, the chest and what was in it",
                  count_drops(MINECART) == 1 && count_drops(CHEST) == 1 && count_drops(COBBLE) == 20 && count_drops(IRON_INGOT) == 7);
        }
        g.drops.clear();
    }
    if (f == 21) {
        // A boat on the pool, and in.
        stand(glm::dvec3(o) + glm::dvec3(2.5, 0, -1.5));
        hold(BOAT);
        look_at(glm::dvec3(o) + glm::dvec3(2.5, -0.1, -3.5));
        const size_t before = g.vehicles.size();
        g.use_item();
        Vehicle* boat = g.vehicles.size() == before + 1 ? g.vehicles.back().get() : nullptr;
        check("a boat goes on the water", boat && boat->kind == VehicleKind::Boat);
        if (boat) {
            run(40);
            check("the boat floats (bottom " + std::to_string(boat->body.y - o.y) + ")",
                  boat->body.y > o.y - 0.9 && boat->body.y < o.y + 0.1);
            check("aiming at the boat", aim_vehicle(*boat));
            g.use_item();
            check("right-clicking a boat gets in", g.in_vehicle == boat);
            g.yaw = 60;
            g.pitch = 10;
            g.third_person = true;
        }
    }
    if (f == 23) {
        shot(app, "m12_boat.png");
        g.third_person = false;
    }
    if (f == 25 && g.in_vehicle) {
        // Full speed into the end of the pool: the boat breaks up into planks and sticks.
        g.drops.clear();
        g.in_vehicle->motion = {VEHICLE_MAX_SPEED, 0, 0};
        Vehicle* boat = g.in_vehicle;
        for (int i = 0; i < 80 && g.in_vehicle; ++i) {
            boat->motion.x = VEHICLE_MAX_SPEED;
            run(1);
        }
        check("a boat crashing into a wall breaks up and drops the rider",
              !g.in_vehicle && std::none_of(g.vehicles.begin(), g.vehicles.end(), [&](const auto& v) { return v.get() == boat; }));
        check("into 3 planks and 2 sticks", count_drops(PLANKS) == 3 && count_drops(STICK) == 2);
    }
    if (f == 27) {
        // Tidy up: no vehicles, no platform, and the player back where they were.
        g.get_out();
        g.vehicles.clear();
        g.target_vehicle = nullptr;
        g.drops.clear();
        g.puffs.clear();
        g.pressed_detectors.clear();
        const glm::ivec3 t = track_;
        for (int dx = -3; dx <= 20; ++dx)
            for (int dz = -8; dz <= 12; ++dz)
                for (int dy = -2; dy <= 4; ++dy) set(t + glm::ivec3(dx, dy, dz), AIR);
        stand(before_track_);
        inv.slots[inv.selected].reset();
        g.weather = hand_weather_;
    }
}
