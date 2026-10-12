// The program around the game: window events, the menus, options and switching worlds.
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "achievements.h"
#include "gl.h"
#include "options.h"
#include "ui.h"

struct GLFWwindow;
class AutoTest;
class Audio;
class Game;
class Screen;

struct AppConfig {
    std::filesystem::path data_dir;    // options.txt, saves/ and screenshots/ go here
    std::optional<uint64_t> seed;      // fills in the seed box of the Create New World screen
    AutoTest* autotest = nullptr;
};

class App {
public:
    App(GLFWwindow* window, AppConfig config);
    ~App();

    // Run until the window is closed or Quit Game is chosen.
    void run();

    // --- state (public so the menus and the autotest can use it) ---
    GLFWwindow* window;
    AppConfig config;
    std::filesystem::path saves_dir;
    Options options;
    Stats stats;  // achievements and statistics, from stats.json (the player's, across worlds)
    std::filesystem::path stats_file() const { return config.data_dir / "stats.json"; }
    GLuint atlas = 0;
    std::unique_ptr<Painter> painter;
    std::unique_ptr<Game> game;
    std::unique_ptr<Audio> audio;  // sound effects (silent without an audio device, and in the autotest)
    bool quit = false;
    long frame = 0;

    // Menus. open() takes effect after the current event, so a screen may replace itself.
    void open(std::unique_ptr<Screen> screen);
    void close_screen();  // back to the game (or nothing)
    Screen* screen() const { return screen_.get(); }
    int gui_scale() const;

    // Worlds.
    bool play_world(const std::string& folder);
    bool create_world(const std::string& name, const std::string& seed_text);
    void pause();                  // open the pause menu and autosave
    void save_and_quit_to_title();
    void options_changed();        // write options.txt and apply the new settings

    // Draw one frame: the game (if any), then the current menu.
    void draw();
    // Save the frame just drawn as a PNG. Returns false if it couldn't be written.
    bool screenshot(const std::filesystem::path& path);

    // GLFW input callbacks forward here.
    void on_key(int key, int action, int mods);
    void on_char(unsigned int codepoint);
    void on_mouse_button(int button, int action);
    void on_cursor(double x, double y);
    void on_scroll(double dy);
    void on_focus(bool focused);

private:
    void apply_pending_screen();
    void update(double dt);
    void draw_saving(const char* text);  // a full frame with a message, while a save finishes
    void take_screenshot();               // F2
    // Mouse position in menu units (top-left origin).
    double gui_x() const;
    double gui_y() const;

    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Screen> pending_;
    bool has_pending_ = false;
    bool want_screenshot_ = false;
    double cursor_x_ = 0, cursor_y_ = 0;
    double fps_ = 60;
};
