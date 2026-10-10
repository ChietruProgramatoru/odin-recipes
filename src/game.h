// The game window: player input, mining/building, survival, cows and drawing.
#pragma once
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <glm/vec3.hpp>

#include "hud.h"
#include "mobs.h"
#include "physics.h"
#include "renderer.h"
#include "rng.h"
#include "survival.h"
#include "world.h"

struct GLFWwindow;
class AutoTest;

constexpr float EYE_HEIGHT = 1.62f;
constexpr float REACH = 5.0f;
constexpr float ATTACK_REACH = 3.5f;
constexpr double WALK_SPEED = 4.3;
constexpr double SPRINT_SPEED = 5.8;
constexpr double FLY_SPEED = 11.0;
constexpr double JUMP_SPEED = 8.4;
constexpr double MOUSE_SENSITIVITY = 0.15;  // degrees per pixel
constexpr size_t MAX_COWS = 24;
const glm::vec4 SKY(0.62f, 0.8f, 1.0f, 1.0f);

class Game {
public:
    Game(GLFWwindow* window, uint64_t seed, int render_distance, AutoTest* autotest);
    ~Game();
    // Run until the window is closed (or the autotest finishes).
    void run();

    // --- state (public so the autotest can drive it) ---
    GLFWwindow* window;
    uint64_t seed;
    World world;
    Rng rng;
    GLuint atlas = 0;
    std::unique_ptr<WorldRenderer> renderer;
    std::unique_ptr<Hud> hud;

    Inventory inventory, creative_inventory;
    Vitals vitals;
    bool creative = false, flying = false;

    glm::dvec3 spawn;
    Body player;
    double yaw = 0, pitch = 0;  // pitch > 0 looks down
    double fall_peak;
    bool was_on_ground = true;

    std::vector<std::unique_ptr<Cow>> cows;
    std::vector<std::unique_ptr<ItemDrop>> drops;
    std::optional<glm::ivec3> mining_pos;
    double mining_progress = 0;
    double attack_cooldown = 0, break_cooldown = 0, use_cooldown = 0;
    std::optional<RayHit> target;
    Cow* target_cow = nullptr;
    bool paused = false;
    bool quit = false;
    long frame = 0;

    Inventory& active_inventory() { return creative ? creative_inventory : inventory; }
    glm::vec3 eye() const;
    glm::vec3 forward() const;

    void update(double dt);
    void draw();
    void update_target();
    void break_block(glm::ivec3 pos, BlockId block);
    void use_item();
    void respawn();
    void set_paused(bool p);

    // GLFW input callbacks forward here.
    void on_key(int key, int action, int mods);
    void on_mouse_button(int button, int action);
    void on_cursor(double x, double y);
    void on_scroll(double dy);
    void on_focus(bool focused);

private:
    glm::dvec3 find_spawn();
    bool spawn_cow_near(double x, double z, double rmin, double rmax);
    void maybe_spawn_cows(ChunkPos pos);
    void move(double dt);
    void try_attack();
    void mine(double dt);
    void spawn_drop(ItemId item, double x, double y, double z);
    void on_death();
    void update_cows(double dt);
    void update_drops(double dt);
    void set_mouse_captured(bool captured);
    bool key_down(int key) const;
    bool mouse_down(int button) const { return mouse_buttons_.count(button) > 0; }

    AutoTest* autotest_;
    std::set<int> mouse_buttons_;
    double last_space_ = -1.0;
    bool have_cursor_ = false;
    double cursor_x_ = 0, cursor_y_ = 0;
    double fps_ = 60;
};
