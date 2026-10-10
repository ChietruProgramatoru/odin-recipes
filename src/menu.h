// Beta-style menus: gray buttons on a dark dirt background, all drawn by the code.
// Positions are in menu units (window pixels divided by the GUI scale), top-left origin,
// with the same layouts Beta used (buttons 200 x 20).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui.h"

class App;

struct Widget {
    enum class Kind { Button, Slider, TextField };
    Kind kind = Kind::Button;
    int id = 0;
    int x = 0, y = 0, w = 200, h = 20;
    std::string label;        // button and slider text
    bool enabled = true;
    float value = 0;          // slider position, 0..1
    std::string text;         // text field contents
    size_t max_length = 32;
    bool focused = false;
    bool highlight = false;   // drawn in yellow (a key button waiting for a key)
    bool warning = false;     // drawn in red (a key bound twice)

    bool contains(double gx, double gy) const { return gx >= x && gx < x + w && gy >= y && gy < y + h; }
};

// Draws in menu units on top of a Painter.
class Gui {
public:
    Gui(Painter& painter, int scale) : p(painter), s(scale) {}
    Painter& p;
    int s;
    int width() const { return p.width() / s; }
    int height() const { return p.height() / s; }

    void rect(float x, float y, float w, float h, Color c);
    // align: 0 left, 0.5 centred, 1 right. y is the top of the text.
    void text(const std::string& str, float x, float y, Color c = {}, float align = 0);
    // The dark dirt background, tiled 32 units per texture like Beta.
    void dirt(float x, float y, float w, float h, float brightness, float scroll = 0);
    void widget(const Widget& w, bool hover, long frame);
};

const Color TEXT_WHITE{0.88f, 0.88f, 0.88f, 1};
const Color TEXT_GRAY{0.63f, 0.63f, 0.63f, 1};
const Color TEXT_DARK_GRAY{0.5f, 0.5f, 0.5f, 1};
const Color TEXT_YELLOW{1, 1, 0.63f, 1};

class Screen {
public:
    explicit Screen(App& app) : app(app) {}
    virtual ~Screen() = default;

    App& app;
    std::vector<Widget> widgets;

    // Build the widgets for a gw x gh menu. Called when the screen opens and when the window resizes.
    virtual void layout(int gw, int gh) = 0;
    virtual void on_button(int id) {}
    virtual void on_slider(int id, float value) {}
    virtual void on_text(int id, const std::string& text) {}
    virtual void on_escape() {}
    virtual void on_enter() {}
    // First look at every key press; return true to swallow it.
    virtual bool on_raw_key(int key) { return false; }
    // A click that hit no widget.
    virtual void on_click(double gx, double gy) {}
    virtual void on_scroll(double dy) {}
    virtual void update(double dt) {}
    // Titles, labels and anything else that isn't a widget.
    virtual void draw(Gui& gui) {}
    // Screens opened from the game show the game behind them instead of dirt.
    virtual bool over_game() const { return false; }
    // The world stands still while the screen is open (not behind the death screen, like Beta).
    virtual bool pauses_game() const { return true; }

    // Input, already converted to menu units.
    void mouse_down(double gx, double gy);
    void mouse_up();
    void mouse_move(double gx, double gy);
    void key(int key);
    void character(unsigned int codepoint);
    // Lay out (keeping typed text and focus) when the size changed.
    void ensure_layout(int gw, int gh);
    void draw_all(Gui& gui, double mouse_x, double mouse_y, long frame);
    Widget* find(int id);

private:
    int dragging_ = -1;
    int laid_out_w_ = -1, laid_out_h_ = -1;
};

using ScreenFactory = std::function<std::unique_ptr<Screen>()>;

std::unique_ptr<Screen> title_screen(App& app);
std::unique_ptr<Screen> select_world_screen(App& app);
std::unique_ptr<Screen> create_world_screen(App& app, std::string name = "New World", std::string seed = "");
std::unique_ptr<Screen> options_screen(App& app, ScreenFactory back);
std::unique_ptr<Screen> video_settings_screen(App& app, ScreenFactory back);
std::unique_ptr<Screen> controls_screen(App& app, ScreenFactory back);
std::unique_ptr<Screen> pause_screen(App& app);
// "Game over!" with Respawn and Title menu, shown while the player is dead.
std::unique_ptr<Screen> death_screen(App& app);
