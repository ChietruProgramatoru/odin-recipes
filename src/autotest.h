// Scripted smoke test: `PetrutzCraft --seed 7 --autotest OUT_DIR`.
// Drives the real program for a few hundred frames: the menus, creating a world, mining, building,
// cows, eating, fall damage, furnaces, tools, armor, the bow, fire and buckets, a tour of the biomes and caves, the F3 and F5 views,
// monsters, explosions and dying, the first-person hand, flowing water and lava, falling sand and fire, farming, animals,
// rails, minecarts and boats, redstone and pistons, the compass, clock, maps, fishing and snowballs, the Nether,
// achievements and statistics,
// the pause menu,
// then saving, quitting to the
// title and loading the world again. Saves screenshots to OUT_DIR and writes OUT_DIR/report.txt.
// The world and options.txt go to OUT_DIR too, so the real saves are never touched.
#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "blocks.h"
#include "furnace.h"
#include "save.h"
#include "weather.h"

class App;
class Game;
class Animal;
class Monster;

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
    void crafting(App& app, Game& g);  // the inventory, a crafting table and the creative item picker
    void furnaces(App& app, Game& g);  // placing a furnace, smelting, the lit furnace, glass, breaking it
    void tools(App& app, Game& g);     // pickaxe rules, armor, the bow, flint and steel, buckets
    void weather_and_beds(App& app, Game& g);  // rain, snow, a thunderstorm, sleeping in a bed
    void monsters(App& app, Game& g);  // each monster, fighting, creepers and TNT, spawners, difficulty, dying
    void hand(App& app, Game& g);      // the first-person hand, held items and the swing
    void fluids_and_fire(App& app, Game& g);  // flowing water and lava, falling sand, fire, drowning, burning
    void farming(App& app, Game& g);   // tilling, planting, bone meal, harvesting, cake and the wild plants
    void building(App& app, Game& g);  // M11: building blocks, chests, doors, signs, paintings, note blocks
    void nature(App& app, Game& g);    // saplings and trees, leaf decay, cactus, cobwebs, snow and ice melting
    void animals(App& app, Game& g);   // every animal, saddles and riding, shearing, dye, milk, taming wolves, eggs
    void transport(App& app, Game& g); // rails, riding a minecart, the chest and furnace minecarts, a boat
    void redstone(App& app, Game& g);  // M13: levers, dust, torches, repeaters, doors, pistons, plates, a dispenser
    void finding_and_fishing(App& app, Game& g);  // M15: compass, clock, a map, fishing, snowballs
    void nether(App& app, Game& g);    // M14: lighting a portal, the Nether, pigmen, a ghast's fireball, back again
    void achievements(App& app, Game& g);  // M17: earning all 16 achievements, the pop-ups, the map and statistics
    void sounds(Game& g);  // M16: the sounds every scene before made along the way
    void reload(App& app);      // quit to the title and load the world again

    std::string out_;
    std::vector<std::string> lines_;
    int failures_ = 0;
    Animal* cow_ = nullptr;
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
    size_t saved_animals_ = 0;
    // Light and day/night.
    glm::ivec3 torch_{0}, glowstone_{0};
    int64_t day_time_ = 0;
    double ground_y_ = 0;
    // Tools: the little range built in front of the player (x, eye level, z of the player's cell).
    glm::ivec3 range_{0};
    int arrows_before_ = 0;
    std::vector<std::pair<glm::ivec3, BlockId>> range_blocks_;
    // Weather and beds.
    glm::ivec3 bed_foot_{0}, bed_head_{0};
    bool bed_placed_ = false;
    bool saved_raining_ = false;
    // Monsters: the flat arena they are tried in (x, ground y, z of the player's start).
    glm::ivec3 arena_{0};
    Monster* mob_ = nullptr;
    int blocks_before_ = 0;
    double spider_top_ = 0;
    bool skeleton_shot_ = false;
    size_t drops_before_death_ = 0;
    // The hand: the hotbar slots it borrowed.
    std::array<std::optional<Stack>, 4> hand_slots_;
    int hand_selected_ = 0;
    std::vector<std::pair<glm::ivec3, BlockId>> hand_blocks_;
    Weather hand_weather_;
    int64_t hand_time_ = 0;
    // Furnaces: the one placed in front of the player, and one left smelting for the reload.
    glm::ivec3 furnace_{0};
    int64_t furnace_time_ = 0;
    // Fluids and fire: a stone platform high in the sky (x, top y, z of its middle).
    glm::ivec3 pad_{0};
    double current_start_x_ = 0;
    // Building: a stone floor high in the sky (x, top y, z of where the player starts), the things
    // checked again after loading, and what the scene borrowed.
    glm::ivec3 build_{0};
    glm::ivec3 sign_pos_{0}, chest_pos_{0}, note_pos_{0}, jukebox_pos_{0};
    glm::dvec3 build_return_{0};
    Weather build_weather_;
    int64_t build_time_ = 0;
    bool build_creative_ = false;
    // Redstone: a stone floor beyond the garden, and where the player was before.
    glm::ivec3 lab_{0};
    glm::dvec3 lab_return_{0};
    bool lab_creative_ = false, lab_raining_ = false;
    size_t lab_arrows_ = 0;
    // Nature: a grass platform next to the fluids one.
    glm::ivec3 garden_{0};
    glm::dvec3 garden_return_{0};
    bool garden_raining_ = false, garden_creative_ = false;
    // Animals: the cow, pig, white sheep, pink sheep, chicken, wolf and squid in the pen.
    std::array<Animal*, 7> pen_{};
    long chickens_ = 0;
    // Transport: the sky platform the track is built on (x, floor top y, z of the track's start),
    // where the player was before, and what the ride went past.
    glm::ivec3 track_{0};
    glm::dvec3 before_track_{0};
    bool rode_over_hump_ = false, pressed_detector_ = false;
    // Compass, clock, maps and fishing: the pond on a sky platform (x, floor top y, z of its near
    // edge), the map started there, and the pig pulled in with the rod.
    glm::ivec3 pond_{0};
    glm::dvec3 items_return_{0};
    bool items_raining_ = false, items_creative_ = false;
    int map_id_ = 0, fish_before_ = 0;
    Animal* pig_ = nullptr;
    double pig_start_ = 0;
    // The Nether: the portal lit in the overworld (its bottom inside cell), where the player was
    // before, the portal they came out of in the Nether, and the ghast and its fireball.
    glm::ivec3 portal_{0};
    glm::dvec3 before_nether_{0};
    std::optional<std::pair<glm::ivec3, int>> nether_portal_;  // origin and axis
    Monster* ghast_ = nullptr;
    std::vector<const Monster*> pigmen_;  // the scene's own; others spawning in the Nether are removed
    bool fireball_seen_ = false, fireball_hit_back_ = false;
    long fireball_frame_ = 0;
    glm::dvec3 ghast_pin_{0};
    std::optional<Inventory> nether_inventory_;  // put back after the trip
    // Achievements: where the player was, what they carried, the furnace whose screen is opened,
    // and the pig flown off a height.
    glm::dvec3 ach_return_{0};
    bool ach_creative_ = false;
    std::optional<Inventory> ach_inventory_;
    Furnace ach_furnace_;
    Animal* ach_pig_ = nullptr;
};
