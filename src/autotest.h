// Scripted smoke test: `PetrutzCraft --seed 7 --autotest OUT_DIR`.
// Drives the real program for a few hundred frames: the menus, creating a world, mining, building,
// cows, eating, fall damage, a tour of the biomes and caves, the F3 and F5 views, the pause menu,
// then saving, quitting to the
// title and loading the world again. Saves screenshots to OUT_DIR and writes OUT_DIR/report.txt.
// The world and options.txt go to OUT_DIR too, so the real saves are never touched.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "blocks.h"
#include "save.h"

class App;
class Game;
class Cow;

class AutoTest {
public:
    AutoTest(std::string out_dir, uint64_t seed);
    void operator()(App& app);  // called once per frame, after the game's update
    int failures() const { return failures_; }

private:
    void log(const std::string& msg);
    void check(const std::string& name, bool ok);
    void shot(App& app, const std::string& name);
    void face(Game& g, double x, double z, double pitch);
    void finish(App& app);
    void menus(App& app);       // before the world exists
    void play(App& app, Game& g);  // the first visit to the world
    void tour(App& app, Game& g);  // fly to a desert, a cold biome and into a cave
    void crafting(App& app, Game& g);
    void weather_and_beds(App& app, Game& g);  // rain, snow, a thunderstorm, sleeping in a bed  // the inventory, a crafting table and the creative item picker
    void reload(App& app);      // quit to the title and load the world again

    std::string out_;
    std::vector<std::string> lines_;
    int failures_ = 0;
    Cow* cow_ = nullptr;
    double cow_start_x_ = 0, cow_start_z_ = 0;
    std::optional<glm::ivec3> broken_;
    BlockId broken_block_ = 0;
    int hp_before_fall_ = 0;
    glm::dvec3 tour_start_{0};
    std::optional<glm::ivec3> cave_;
    uint64_t seed_;
    // Saving and loading.
    int phase_ = 0;  // 0 menus, 1 playing, 2 back at the title, 3 playing again
    long phase_start_ = 0;
    glm::ivec3 marker_{0};
    PlayerData saved_player_;
    size_t saved_cows_ = 0;
    // Light and day/night.
    glm::ivec3 torch_{0}, glowstone_{0};
    int64_t day_time_ = 0;
    double ground_y_ = 0;
    // Weather and beds.
    glm::ivec3 bed_foot_{0}, bed_head_{0};
    bool bed_placed_ = false;
    bool saved_raining_ = false;
};
