// One world being played: player input, mining/building, survival, animals, monsters, drawing and saving.
#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "animals.h"
#include "hand.h"
#include "blockupdate.h"
#include "farming.h"
#include "fishing.h"
#include "hud.h"
#include "inventory_screen.h"
#include "mobs.h"
#include "monsters.h"
#include "navigation.h"
#include "options.h"
#include "physics.h"
#include "renderer.h"
#include "rng.h"
#include "save.h"
#include "sound.h"
#include "sky.h"
#include "weather.h"
#include "survival.h"
#include "vehicles.h"
#include "world.h"

struct GLFWwindow;

constexpr float EYE_HEIGHT = 1.62f;
constexpr float SNEAK_EYE_DROP = 0.08f;
constexpr float REACH = 5.0f;
constexpr float ATTACK_REACH = 3.5f;
constexpr double WALK_SPEED = 4.3;
constexpr double SPRINT_SPEED = 5.8;
constexpr double SNEAK_SPEED = 1.3;
constexpr double FLY_SPEED = 11.0;
constexpr double JUMP_SPEED = 8.4;
constexpr double THIRD_PERSON_DISTANCE = 4.0;
constexpr size_t MAX_ANIMALS = 30;  // land animals simulated at once (the world spawns no more past this)
constexpr size_t MAX_SQUID = 5;     // Beta's cap on water animals
constexpr double RIDE_HEIGHT = 0.55;  // the player's feet above a pig's while riding it
constexpr int MONSTER_SPAWN_CHUNKS = 8;  // monsters spawn in chunks this close to the player (Beta)

struct GameSetup {
    std::filesystem::path world_dir;  // the world's folder in saves/
    LevelData level;                  // from level.json (no player yet for a new world)
    bool scripted = false;            // the autotest drives this game: fixed random numbers, mouse left alone
};

class Game {
public:
    Game(GLFWwindow* window, GLuint atlas, Painter& painter, const Options& options, GameSetup setup);
    ~Game();  // waits for the last save to finish

    // --- state (public so the autotest can drive it) ---
    GLFWwindow* window;
    const Options& options;
    LevelData level;
    std::unique_ptr<WorldSaver> saver;
    World world;
    Rng rng;
    std::unique_ptr<WorldRenderer> renderer;
    std::unique_ptr<Hud> hud;

    Inventory inventory, creative_inventory;
    // The inventory, a crafting table or the creative item picker, while one is open.
    std::unique_ptr<InventoryScreen> gui;
    std::optional<glm::ivec3> furnace_open;  // the furnace whose screen is open
    std::vector<glm::ivec3> chest_open;      // the chest (one or both halves) whose screen is open
    // The sign whose text is being typed (the app shows the sign screen while it is set).
    std::optional<glm::ivec3> sign_editing;
    Vitals vitals;
    bool creative = false, flying = false, sneaking = false;

    glm::dvec3 spawn;
    Body player;
    double yaw = 0, pitch = 0;  // pitch > 0 looks down
    double fall_peak;
    bool was_on_ground = true;
    double walk_phase = 0, walk_amount = 0;  // for the third-person model
    Hand hand;  // the arm or held item in first person, and its swing

    // Weather (rain, snow, thunder) and the lightning bolts showing right now.
    Weather weather;
    std::vector<LightningBolt> bolts;
    int lightning_flash = 0;  // ticks left of the sky flashing white
    // Beds: the head of the bed the player last slept in (they respawn next to it), and the bed
    // they are lying in right now with how many ticks they have been asleep.
    std::optional<glm::ivec3> bed_spawn;
    std::optional<glm::ivec3> sleeping_in;
    int sleep_ticks = 0;
    int sleep_dir = 0;  // facing of that bed (foot to head)

    std::vector<std::unique_ptr<Animal>> animals;
    Animal* riding = nullptr;  // the saddled pig the player sits on
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<std::unique_ptr<ItemDrop>> drops;
    std::vector<std::unique_ptr<Arrow>> arrows;
    std::vector<std::unique_ptr<PrimedTnt>> tnts;
    std::vector<Puff> puffs;
    // Minecarts and boats (M12): the one the player sits in, the one in the crosshair, the storage
    // minecart whose chest is open, and the detector rails a minecart is pressing.
    std::vector<std::unique_ptr<Vehicle>> vehicles;
    Vehicle* in_vehicle = nullptr;
    Vehicle* target_vehicle = nullptr;
    Vehicle* chest_vehicle = nullptr;
    std::vector<glm::ivec3> pressed_detectors;
    // Finding the way (M15, game_items.cpp): the compass and clock needles, and the maps of this
    // world loaded so far, by number.
    Needle compass_needle, clock_needle;
    std::map<int, MapData> maps;
    // Fishing: the bobber the rod cast, while it is out, and how many fish were reeled in.
    std::unique_ptr<Bobber> bobber;
    int fish_caught = 0;
    double vehicle_ticks = 0;  // part of a game tick since vehicles last moved (they are drawn in between)
    // Sounds started this frame, for the app to play (then it clears them), and how many of each
    // kind were ever started (the autotest checks these).
    std::vector<SoundEvent> sounds;
    std::array<int, SOUND_COUNT> sounds_started{};
    void play_sound(Sound sound, glm::dvec3 pos, float pitch = 1.0f);
    // Mob spawners in the chunks shown so far, and ticks until each one's next try.
    struct Spawner {
        glm::ivec3 pos;
        int delay = 20;
    };
    std::vector<Spawner> spawners;
    // Flowing water and lava, falling sand and gravel, and fire (M6), and the sand and gravel
    // falling right now.
    BlockUpdater blocks{world};
    std::vector<std::unique_ptr<FallingBlock>> falling;
    int burn_ticks = 0;  // the player is on fire for this many more ticks
    int armor_remainder = 0;  // fraction of a damage point armor carries to the next hit (in 25ths)
    std::optional<glm::ivec3> mining_pos;
    double mining_progress = 0;
    double attack_cooldown = 0, break_cooldown = 0, use_cooldown = 0;
    std::optional<RayHit> target;
    Animal* target_animal = nullptr;
    Monster* target_monster = nullptr;
    bool paused = false;
    bool hud_hidden = false, debug_screen = false, third_person = false;  // F1, F3, F5
    double autosave_timer = 0;
    long frame = 0;
    double fps = 60;

    Inventory& active_inventory() { return creative ? creative_inventory : inventory; }
    // The crosshair is on an animal, a monster or a vehicle (not a block).
    bool targeting_mob() const { return target_animal || target_monster || target_vehicle; }
    int difficulty() const { return options.difficulty; }
    glm::vec3 eye() const;
    glm::vec3 forward() const;
    // Where the camera is: the eyes, or behind the player in third person.
    glm::vec3 camera();

    void update(double dt);
    void draw(int width, int height);
    void update_target();
    void break_block(glm::ivec3 pos, BlockId block);
    void use_item();
    // Right click: use_item(), swinging the hand if that changed a block or opened something (Beta).
    void use_item_by_hand();
    // Left click on an animal or monster in reach.
    void try_attack();
    // Right click on an animal: milk, saddle, ride, shear, dye, tame, feed or sit. False if nothing happened.
    bool interact(Animal& a);
    // Throw the egg or snowball in hand (right click with one).
    void throw_item(ItemId item);
    // Get off the pig (sneak), standing on top of it or next to it.
    void dismount();
    // The player was hit by, or hit, this monster or animal: their tamed wolves go for it (Beta).
    void alert_wolves(const void* enemy);
    // An animal at a spot. Returns it.
    Animal* spawn_animal(AnimalKind kind, double x, double y, double z);
    // A group of 1 to 4 animals of one kind (Beta's packs) on grass rmin..rmax blocks from a point,
    // of the kind the biome there picks unless `kind` is given. Returns how many appeared.
    int spawn_animal_group(double x, double z, double rmin, double rmax, std::optional<AnimalKind> kind = std::nullopt);
    // A few squid in deep water rmin..rmax blocks from a point. Returns how many appeared.
    int spawn_squid(double x, double z, double rmin, double rmax);
    size_t land_animal_count() const;
    // Damage the player, less what their armor takes off. Returns true if it hurt.
    bool hurt_player(int amount, bool ignore_cooldown = false);
    // A monster (or its arrow or blast) hits the player: scaled by the difficulty, and a hit
    // knocks them away from (from_x, from_z).
    bool hurt_player_by_monster(int amount, double from_x, double from_z);
    // A monster at a spot, if it fits there. Returns nullptr if not.
    Monster* spawn_monster(MobKind kind, double x, double y, double z, int size = 1);
    // Could a monster appear with its feet at (x, y, z)? Natural spawns also need solid ground,
    // and to be 24 blocks from the player and the world spawn; spawners only need room and darkness.
    bool can_spawn_monster(MobKind kind, double x, double y, double z, int size, bool natural);
    // A blast of `power` (creeper 3, TNT 4): hurts and pushes everything near, then destroys blocks.
    void explode(glm::dvec3 center, double power, bool from_monster);
    // Light TNT at a block (taking the block away) with a fuse in ticks.
    void prime_tnt(glm::ivec3 pos, int fuse_ticks);
    // Monsters spawning at random in the dark (each tick), and mob spawners near the player.
    void spawn_monsters();
    void tick_spawners();
    void drop_selected();
    void open_gui(InventoryScreen::Kind kind);
    // Right-clicking a furnace opens its screen.
    void open_furnace(glm::ivec3 pos);
    // Every furnace within the render distance burns and smelts for one tick (Beta: far ones wait).
    void tick_furnaces();
    // A furnace block is gone: what was in it falls out.
    void drop_furnace_contents(glm::ivec3 pos);
    void close_gui();
    // Right-clicking a chest opens it (both halves of a double chest), unless a block sits on top.
    void open_chest(glm::ivec3 pos);
    // Right-clicking a block that does something by itself: doors and trapdoors open, a note block
    // goes up a note, a jukebox gives its disc back. False if the block doesn't do anything.
    bool use_block(glm::ivec3 pos);
    // A note block sounds its note (if there is air above it, like Beta).
    void play_note(glm::ivec3 pos);
    // The text of the sign being edited (nullptr if it is gone), and closing the sign screen.
    SignText* edited_sign();
    void finish_sign();
    // A block is about to be taken away: what it holds (chest items, a jukebox's disc) falls out and
    // any screen open on it closes. Then it is set to air; returns the chunks to mesh again.
    std::vector<ChunkPos> clear_cell(glm::ivec3 pos);
    // Blocks changed at these cells: ladders, doors, signs and paintings left with nothing to hold
    // them drop off (and what that leaves hanging, in turn).
    void drop_unsupported(std::vector<glm::ivec3> changed);
    // Throw a stack forwards from the player's eyes, like Q does.
    void throw_stack(const Stack& stack);
    void respawn();
    // Right-clicking a bed: lie down if it is night (or a thunderstorm), else say why not.
    void try_sleep(glm::ivec3 bed_pos);
    // Get out of bed; morning sets the bed as the spawn point.
    void wake_up(bool set_spawn);
    // One game tick (20 a second) of the world: weather, lightning and sleeping.
    void tick_world();
    // Beta's random block ticks in the chunks around the player: crops grow, farmland dries out.
    void random_ticks();
    // Apply what a farming rule did: mesh the changed chunks again and drop its items.
    void apply(const FarmEvents& events);
    // The sky for this frame, weather included.
    SkyState current_sky() const;
    // Block ticks, random ticks and what they set off; the player breathing and burning.
    void tick_blocks();
    void tick_player_fire_and_air();
    void apply_block_events(BlockEvents& events);
    // Rain is falling on this cell (it puts fire out there).
    bool rain_at(glm::ivec3 p);
    // The player's eyes are under the surface of water.
    bool head_under_water();
    // Set the player alight for at least `ticks` (Beta adds to a fire already burning).
    void set_player_on_fire(int ticks);

    // The map with this number, loaded from the world's folder (or started where the player is if
    // it has no file), and the map in hand (a blank one becomes a new map centred on the player).
    MapData& map_data(int id);
    MapData* held_map();
    // Each tick: the compass and clock icons move and the map in hand fills in.
    void tick_navigation();
    void save_maps();  // the maps changed since the last save
    // Right click with a fishing rod: cast the bobber, or reel it in (with a fish if one bites).
    void use_fishing_rod();
    void tick_bobber();
    // The bobber and its line, to draw.
    std::vector<ModelPart> fishing_parts();

    // Minecarts and boats (game_vehicles.cpp). Vehicles move a game tick at a time.
    void update_vehicles(double dt);
    void tick_vehicles();
    // Right click with a minecart on a rail, or a boat on water (or anything): put it there.
    bool place_vehicle(ItemId item);
    // Right click on a vehicle: get in, open its chest or give it coal.
    void use_vehicle(Vehicle& v);
    // Left click on a vehicle. It breaks after a few hits (at once in creative).
    void hit_vehicle(Vehicle& v);
    // Getting in and out of a minecart or boat (dismount() is for the pig).
    void get_in(Vehicle& v);
    void get_out();
    // A rail is being placed at a cell (the block is already set): join it to the rails around it.
    void lay_rail(glm::ivec3 pos);
    // Lightning strikes at a spot: hurts what is near it and flashes the sky.
    void strike(glm::dvec3 pos);
    void set_paused(bool p);
    // Video settings or render distance changed.
    void apply_options();

    // Save the player, level and every changed chunk. In the background unless wait is true.
    bool save(bool wait);
    bool saving() const { return saver && saver->busy(); }
    PlayerData player_data() const;

    // GLFW input callbacks forward here (the app handles Escape and F2).
    void on_key(int key, int action, int mods);
    void on_mouse_button(int button, int action);
    void on_cursor(double x, double y);
    void on_scroll(double dy);

private:
    glm::dvec3 find_spawn();
    void restore_player(const PlayerData& p);
    void on_chunk_shown(ChunkPos pos);
    void spawn_entity(const EntityRecord& e);
    void stash_entity(const EntityRecord& e);
    void move(double dt);
    void mine(double dt);
    void spawn_drop(ItemId item, double x, double y, double z);
    // A broken vehicle's drops, and taking it out of the world.
    void remove_vehicle(size_t index, bool drop_items);
    void on_death();
    void update_animals(double dt);
    void update_monsters(double dt);
    void update_tnts(double dt);
    // A monster died: its drops, and a big slime's smaller slimes.
    void loot(Monster& m, std::vector<std::unique_ptr<Monster>>& born);
    void update_drops(double dt);
    void update_arrows(double dt);
    void update_falling(double dt);
    void shoot_arrow();
    bool light_fire(glm::ivec3 pos);
    // Wear the selected hotbar item by `uses` (survival only); it breaks when worn out.
    void wear_selected(int uses);
    // Replace one of the selected stack with `item` (filling or emptying a bucket).
    void swap_selected(ItemId item);
    bool use_bucket(ItemId held);
    // Right click with seeds, sugar cane, a flower or cake: plant or place it. False if it can't go there.
    bool plant(BlockId block);
    // Right click with a building item (building.h's placement, or a painting). False if nothing went down.
    bool place_building(ItemId item);
    // Put a music disc in the jukebox, or take one out.
    bool use_jukebox(glm::ivec3 pos, std::optional<ItemId> held);
    void set_mouse_captured(bool captured);
    void show_gui(std::unique_ptr<InventoryScreen> screen);
    bool key_down(int key) const;
    bool action_down(KeyAction action) const { return !gui && !sign_editing && key_down(options.keys[action]); }
    bool sprint_allowed() const { return !sneaking && !flying && (creative || vitals.can_sprint()); }
    bool mouse_down(int button) const { return mouse_buttons_.count(button) > 0; }
    std::string debug_text();
    std::string help_text() const;
    // The mouse in framebuffer pixels from the top left, for the inventory screens.
    void gui_mouse(double& x, double& y) const;
    void draw_player_preview(int x, int y, int w, int h, float look_x, float look_y);
    // The map in hand, held up in front of the view (instead of the hand).
    void draw_held_map(int w, int h);
    glm::dvec3 rod_tip() const;  // where the fishing line starts

    Painter& painter_;
    bool scripted_;
    std::set<int> mouse_buttons_;
    double last_space_ = -1.0;
    SprintControl sprint_;
    bool have_cursor_ = false;
    double cursor_x_ = 0, cursor_y_ = 0;
    double ticks_ = 0;  // fraction of a game tick not yet added to level.time
    Rng weather_rng_;   // its own random numbers, so weather doesn't change what animals do
    Rng animal_rng_;    // and for animals, so they don't change what the rest does
    Rng monster_rng_;   // and its own for monsters, so they don't change what the rest does
    Rng farm_rng_;      // and for random block ticks (crops growing, farmland drying)
    Rng fishing_rng_;   // and for fishing
    int shown_map_ = -1;          // the map whose picture is in the atlas now
    long shown_map_version_ = -1;
};
