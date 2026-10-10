// One world being played: player input, mining/building, survival, cows, drawing and saving.
#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "hud.h"
#include "inventory_screen.h"
#include "mobs.h"
#include "options.h"
#include "physics.h"
#include "renderer.h"
#include "rng.h"
#include "save.h"
#include "sky.h"
#include "weather.h"
#include "survival.h"
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
constexpr size_t MAX_COWS = 24;

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
    Vitals vitals;
    bool creative = false, flying = false, sneaking = false;

    glm::dvec3 spawn;
    Body player;
    double yaw = 0, pitch = 0;  // pitch > 0 looks down
    double fall_peak;
    bool was_on_ground = true;
    double walk_phase = 0, walk_amount = 0;  // for the third-person model

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

    std::vector<std::unique_ptr<Cow>> cows;
    std::vector<std::unique_ptr<ItemDrop>> drops;
    std::optional<glm::ivec3> mining_pos;
    double mining_progress = 0;
    double attack_cooldown = 0, break_cooldown = 0, use_cooldown = 0;
    std::optional<RayHit> target;
    Cow* target_cow = nullptr;
    bool paused = false;
    bool hud_hidden = false, debug_screen = false, third_person = false;  // F1, F3, F5
    double autosave_timer = 0;
    long frame = 0;
    double fps = 60;

    Inventory& active_inventory() { return creative ? creative_inventory : inventory; }
    glm::vec3 eye() const;
    glm::vec3 forward() const;
    // Where the camera is: the eyes, or behind the player in third person.
    glm::vec3 camera();

    void update(double dt);
    void draw(int width, int height);
    void update_target();
    void break_block(glm::ivec3 pos, BlockId block);
    void use_item();
    void drop_selected();
    void open_gui(InventoryScreen::Kind kind);
    void close_gui();
    // Throw a stack forwards from the player's eyes, like Q does.
    void throw_stack(const Stack& stack);
    void respawn();
    // Right-clicking a bed: lie down if it is night (or a thunderstorm), else say why not.
    void try_sleep(glm::ivec3 bed_pos);
    // Get out of bed; morning sets the bed as the spawn point.
    void wake_up(bool set_spawn);
    // One game tick (20 a second) of the world: weather, lightning and sleeping.
    void tick_world();
    // The sky for this frame, weather included.
    SkyState current_sky() const;
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
    bool spawn_cow_near(double x, double z, double rmin, double rmax);
    void on_chunk_shown(ChunkPos pos);
    void spawn_entity(const EntityRecord& e);
    void stash_entity(const EntityRecord& e);
    void move(double dt);
    void try_attack();
    void mine(double dt);
    void spawn_drop(ItemId item, double x, double y, double z);
    void on_death();
    void update_cows(double dt);
    void update_drops(double dt);
    void set_mouse_captured(bool captured);
    bool key_down(int key) const;
    bool action_down(KeyAction action) const { return !gui && key_down(options.keys[action]); }
    bool sprint_allowed() const { return !sneaking && !flying && (creative || vitals.can_sprint()); }
    bool mouse_down(int button) const { return mouse_buttons_.count(button) > 0; }
    std::string debug_text();
    std::string help_text() const;
    // The mouse in framebuffer pixels from the top left, for the inventory screens.
    void gui_mouse(double& x, double& y) const;
    void draw_player_preview(int x, int y, int w, int h, float look_x, float look_y);

    Painter& painter_;
    bool scripted_;
    std::set<int> mouse_buttons_;
    double last_space_ = -1.0;
    SprintControl sprint_;
    bool have_cursor_ = false;
    double cursor_x_ = 0, cursor_y_ = 0;
    double ticks_ = 0;  // fraction of a game tick not yet added to level.time
    Rng weather_rng_;   // its own random numbers, so weather doesn't change what cows do
    void hurt_player(int amount);
};
