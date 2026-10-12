#include "menu.h"

#include <algorithm>
#include <cmath>
#include <ctime>

#include <GLFW/glfw3.h>

#include "app.h"
#include "font.h"
#include "game.h"
#include "save.h"
#include "textures.h"

// --- Drawing ---------------------------------------------------------------------------------

void Gui::rect(float x, float y, float w, float h, Color c) {
    p.rect(x * s, static_cast<float>(p.height()) - (y + h) * s, w * s, h * s, c);
}

void Gui::text(const std::string& str, float x, float y, Color c, float align) {
    p.text(str, std::floor(x * s), static_cast<float>(p.height()) - y * s, s, align, 1, false, c);
}

void Gui::dirt(float x, float y, float w, float h, float brightness, float scroll) {
    const float size = 32;  // menu units per texture repeat
    Color tint{brightness, brightness, brightness, 1};
    float start_y = y - std::fmod(scroll, size);
    for (float ty = start_y; ty < y + h; ty += size)
        for (float tx = x; tx < x + w; tx += size) {
            // Clip the tile to the area, adjusting its texture coordinates to match.
            float x0 = std::max(tx, x), x1 = std::min(tx + size, x + w);
            float y0 = std::max(ty, y), y1 = std::min(ty + size, y + h);
            if (x1 <= x0 || y1 <= y0) continue;
            float u0 = (x0 - tx) / size, u1 = (x1 - tx) / size;
            float v1 = 1 - (y0 - ty) / size, v0 = 1 - (y1 - ty) / size;  // v = 1 is the top of the tile
            p.tile(T_DIRT, x0 * s, static_cast<float>(p.height()) - y1 * s, (x1 - x0) * s, (y1 - y0) * s, tint, u0, v0, u1, v1);
        }
}

namespace {

// A Beta-style button face: black outline, light top-left edge, dark bottom-right edge.
void button_face(Gui& g, float x, float y, float w, float h, bool enabled, bool hover) {
    Color fill = !enabled ? Color{0.17f, 0.17f, 0.17f, 1} : hover ? Color{0.45f, 0.5f, 0.7f, 1} : Color{0.43f, 0.43f, 0.43f, 1};
    Color light = !enabled ? Color{0.26f, 0.26f, 0.26f, 1} : hover ? Color{0.66f, 0.71f, 0.92f, 1} : Color{0.66f, 0.66f, 0.66f, 1};
    Color dark = !enabled ? Color{0.1f, 0.1f, 0.1f, 1} : hover ? Color{0.3f, 0.34f, 0.52f, 1} : Color{0.3f, 0.3f, 0.3f, 1};
    g.rect(x, y, w, h, {0, 0, 0, 1});
    g.rect(x + 1, y + 1, w - 2, h - 2, dark);
    g.rect(x + 1, y + 1, w - 3, h - 3, light);
    g.rect(x + 2, y + 2, w - 4, h - 5, fill);
    // A faint lighter band on the upper half, like worn stone.
    g.rect(x + 2, y + 2, w - 4, (h - 5) / 2, {1, 1, 1, enabled ? 0.05f : 0.02f});
}

Color label_color(const Widget& w, bool hover) {
    if (!w.enabled) return TEXT_GRAY;
    if (w.warning) return {1, 0.33f, 0.33f, 1};
    if (w.highlight || hover) return TEXT_YELLOW;
    return TEXT_WHITE;
}

}  // namespace

void Gui::widget(const Widget& w, bool hover, long frame) {
    switch (w.kind) {
        case Widget::Kind::Button:
            button_face(*this, w.x, w.y, w.w, w.h, w.enabled, hover);
            text(w.label, w.x + w.w / 2.0f, w.y + (w.h - 7) / 2.0f, label_color(w, hover), 0.5f);
            break;
        case Widget::Kind::Slider: {
            button_face(*this, w.x, w.y, w.w, w.h, false, false);
            float kx = w.x + w.value * (w.w - 8);
            button_face(*this, kx, w.y, 8, w.h, true, hover);
            text(w.label, w.x + w.w / 2.0f, w.y + (w.h - 7) / 2.0f, label_color(w, hover), 0.5f);
            break;
        }
        case Widget::Kind::TextField: {
            rect(w.x - 1, w.y - 1, w.w + 2, w.h + 2, {0.63f, 0.63f, 0.63f, 1});
            rect(w.x, w.y, w.w, w.h, {0, 0, 0, 1});
            std::string shown = w.text;
            if (w.focused && (frame / 20) % 2 == 0) shown += "_";
            text(shown, w.x + 4, w.y + (w.h - 7) / 2.0f, TEXT_WHITE);
            break;
        }
    }
}

// --- Input -----------------------------------------------------------------------------------

Widget* Screen::find(int id) {
    for (Widget& w : widgets)
        if (w.id == id) return &w;
    return nullptr;
}

void Screen::ensure_layout(int gw, int gh) {
    if (gw == laid_out_w_ && gh == laid_out_h_) return;
    int focused = -1;
    for (const Widget& w : widgets)
        if (w.focused) focused = w.id;
    laid_out_w_ = gw;
    laid_out_h_ = gh;
    widgets.clear();
    layout(gw, gh);
    if (focused >= 0)
        if (Widget* w = find(focused)) w->focused = true;
}

void Screen::mouse_down(double gx, double gy) {
    for (Widget& w : widgets) {
        if (!w.contains(gx, gy) || !w.enabled) continue;
        int id = w.id;
        if (w.kind == Widget::Kind::TextField) {
            for (Widget& o : widgets) o.focused = false;
            w.focused = true;
        } else if (w.kind == Widget::Kind::Slider) {
            app.click_sound();
            dragging_ = id;
            mouse_move(gx, gy);
        } else {
            app.click_sound();
            on_button(id);  // may relayout or close this screen: touch nothing after this
        }
        return;
    }
    pressed_ = true;
    press_x_ = gx;
    press_y_ = gy;
    on_click(gx, gy);
}

void Screen::mouse_up() {
    dragging_ = -1;
    pressed_ = false;
}

void Screen::mouse_move(double gx, double gy) {
    if (pressed_ && dragging_ < 0) {
        on_drag(gx - press_x_, gy - press_y_);
        press_x_ = gx;
        press_y_ = gy;
    }
    if (dragging_ < 0) return;
    Widget* w = find(dragging_);
    if (!w) return;
    w->value = std::clamp(static_cast<float>((gx - (w->x + 4)) / (w->w - 8)), 0.0f, 1.0f);
    on_slider(w->id, w->value);
}

void Screen::key(int key) {
    if (on_raw_key(key)) return;
    Widget* field = nullptr;
    for (Widget& w : widgets)
        if (w.focused) field = &w;
    if (key == GLFW_KEY_ESCAPE) {
        on_escape();
    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        on_enter();
    } else if (key == GLFW_KEY_TAB) {
        std::vector<Widget*> fields;
        for (Widget& w : widgets)
            if (w.kind == Widget::Kind::TextField) fields.push_back(&w);
        if (fields.empty()) return;
        size_t next = 0;
        for (size_t i = 0; i < fields.size(); ++i)
            if (fields[i]->focused) next = (i + 1) % fields.size();
        for (Widget* f : fields) f->focused = false;
        fields[next]->focused = true;
    } else if (field && key == GLFW_KEY_BACKSPACE && !field->text.empty()) {
        field->text.pop_back();
        on_text(field->id, field->text);
    }
}

void Screen::character(unsigned int codepoint) {
    // The pixel font only has printable ASCII.
    if (codepoint < 32 || codepoint > 126) return;
    for (Widget& w : widgets)
        if (w.focused && w.kind == Widget::Kind::TextField && w.text.size() < w.max_length) {
            w.text += static_cast<char>(codepoint);
            on_text(w.id, w.text);
            return;
        }
}

void Screen::draw_all(Gui& gui, double mx, double my, long frame) {
    const int gw = gui.width(), gh = gui.height();
    if (over_game()) {
        gui.rect(0, 0, static_cast<float>(gw), static_cast<float>(gh) / 2, {0.06f, 0.06f, 0.06f, 0.75f});
        gui.rect(0, static_cast<float>(gh) / 2, static_cast<float>(gw), static_cast<float>(gh) / 2 + 1, {0.06f, 0.06f, 0.06f, 0.82f});
    } else {
        gui.dirt(0, 0, static_cast<float>(gw), static_cast<float>(gh), 0.25f);
    }
    mouse_x = mx;
    mouse_y = my;
    draw(gui);
    for (const Widget& w : widgets) gui.widget(w, w.enabled && (w.contains(mx, my) || dragging_ == w.id), frame);
}

// --- Screens ---------------------------------------------------------------------------------

Widget button(int id, int x, int y, int w, const std::string& label, bool enabled) {
    Widget b;
    b.id = id;
    b.x = x;
    b.y = y;
    b.w = w;
    b.label = label;
    b.enabled = enabled;
    return b;
}

namespace {

Widget slider(int id, int x, int y, int w, const std::string& label, float value) {
    Widget s = button(id, x, y, w, label);
    s.kind = Widget::Kind::Slider;
    s.value = value;
    return s;
}

Widget text_field(int id, int x, int y, int w, const std::string& text, size_t max_length) {
    Widget f = button(id, x, y, w, "");
    f.kind = Widget::Kind::TextField;
    f.text = text;
    f.max_length = max_length;
    return f;
}

std::string on_off(bool v) { return v ? "ON" : "OFF"; }

std::string percent(float v) { return std::to_string(static_cast<int>(std::lround(v * 100))) + "%"; }

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// --- Title -------------------------------------------------------------------------------------

class TitleScreen : public Screen {
public:
    using Screen::Screen;
    enum { SINGLEPLAYER, OPTIONS, QUIT };

    void layout(int gw, int gh) override {
        int y = gh / 4 + 48;
        widgets.push_back(button(SINGLEPLAYER, gw / 2 - 100, y, 200, "Singleplayer"));
        widgets.push_back(button(OPTIONS, gw / 2 - 100, y + 36, 98, "Options..."));
        widgets.push_back(button(QUIT, gw / 2 + 2, y + 36, 98, "Quit Game"));
    }
    void on_button(int id) override {
        App& a = app;
        if (id == SINGLEPLAYER) a.open(select_world_screen(a));
        if (id == OPTIONS) a.open(options_screen(a, [&a] { return title_screen(a); }));
        if (id == QUIT) a.quit = true;
    }
    void draw(Gui& g) override {
        const std::string logo = "PetrutzCraft";
        const float pixel = 4;  // menu units per font pixel
        float logo_w = static_cast<float>(text_width(logo)) * pixel;
        float x = (g.width() - logo_w) / 2, top = 30;
        g.p.block_text(logo, x * g.s, static_cast<float>(g.p.height()) - top * g.s, pixel * g.s, T_STONE);
        g.text("A Beta-style block game", g.width() / 2.0f, top + 7 * pixel + 8, TEXT_YELLOW, 0.5f);
        g.text("PetrutzCraft M2", 2, static_cast<float>(g.height()) - 10, TEXT_DARK_GRAY);
        g.text("Fan-made, not affiliated with Mojang", static_cast<float>(g.width()) - 2, static_cast<float>(g.height()) - 10,
               TEXT_DARK_GRAY, 1);
    }
};

// --- World list ----------------------------------------------------------------------------------

class SelectWorldScreen : public Screen {
public:
    explicit SelectWorldScreen(App& a) : Screen(a), worlds_(list_worlds(a.saves_dir)) {}
    enum { PLAY, CREATE, RENAME, ERASE, CANCEL };
    static constexpr int TOP = 32, ENTRY = 36;

    void layout(int gw, int gh) override {
        gw_ = gw;
        gh_ = gh;
        bool any = selected_ >= 0;
        widgets.push_back(button(PLAY, gw / 2 - 154, gh - 52, 150, "Play Selected World", any));
        widgets.push_back(button(CREATE, gw / 2 + 4, gh - 52, 150, "Create New World"));
        widgets.push_back(button(RENAME, gw / 2 - 154, gh - 28, 70, "Rename", any));
        widgets.push_back(button(ERASE, gw / 2 - 74, gh - 28, 70, "Delete", any));
        widgets.push_back(button(CANCEL, gw / 2 + 4, gh - 28, 150, "Cancel"));
        clamp_scroll();
    }
    void on_button(int id) override {
        App& a = app;
        if (id == PLAY && selected_ >= 0) a.play_world(worlds_[selected_].folder);
        if (id == CREATE) a.open(create_world_screen(a, "New World", a.config.seed ? std::to_string(*a.config.seed) : ""));
        if (id == RENAME && selected_ >= 0) a.open(rename_screen(worlds_[selected_]));
        if (id == ERASE && selected_ >= 0) a.open(delete_screen(worlds_[selected_]));
        if (id == CANCEL) a.open(title_screen(a));
    }
    void on_escape() override { app.open(title_screen(app)); }
    void on_enter() override { on_button(PLAY); }
    void on_scroll(double dy) override {
        scroll_ -= static_cast<float>(dy) * ENTRY / 2;
        clamp_scroll();
    }
    void on_click(double gx, double gy) override {
        if (gy < TOP || gy >= gh_ - 64 || gx < gw_ / 2 - 110 || gx >= gw_ / 2 + 110) return;
        int i = static_cast<int>(std::floor((gy - TOP - 4 + scroll_) / ENTRY));
        if (i < 0 || i >= static_cast<int>(worlds_.size())) return;
        double now = glfwGetTime();
        bool double_click = i == selected_ && now - last_click_ < 0.3;
        last_click_ = now;
        select(i);
        if (double_click) app.play_world(worlds_[i].folder);
    }
    void draw(Gui& g) override {
        const float list_bottom = static_cast<float>(gh_ - 64);
        g.dirt(0, TOP, static_cast<float>(gw_), list_bottom - TOP, 0.125f, scroll_);
        // Shadows along the list edges.
        g.rect(0, TOP, static_cast<float>(gw_), 4, {0, 0, 0, 0.5f});
        g.rect(0, list_bottom - 4, static_cast<float>(gw_), 4, {0, 0, 0, 0.5f});
        g.text("Select World", gw_ / 2.0f, 16, TEXT_WHITE, 0.5f);

        g.p.clip(0, (g.height() - static_cast<int>(list_bottom)) * g.s, g.p.width(), static_cast<int>(list_bottom - TOP) * g.s);
        const float x = gw_ / 2.0f - 110;
        if (worlds_.empty()) g.text("No worlds yet. Create one below!", gw_ / 2.0f, TOP + 20, TEXT_DARK_GRAY, 0.5f);
        for (size_t i = 0; i < worlds_.size(); ++i) {
            float y = TOP + 4 + static_cast<float>(i) * ENTRY - scroll_;
            if (y + ENTRY < TOP || y > list_bottom) continue;
            const WorldInfo& w = worlds_[i];
            if (static_cast<int>(i) == selected_) {
                g.rect(x - 2, y - 2, 224, ENTRY, {0.5f, 0.5f, 0.5f, 1});
                g.rect(x - 1, y - 1, 222, ENTRY - 2, {0, 0, 0, 1});
            }
            g.text(w.name, x + 2, y + 1, TEXT_WHITE);
            g.text(w.folder + " (" + date(w.last_played) + ")", x + 2, y + 12, TEXT_DARK_GRAY);
            g.text(std::string(w.creative ? "Creative cheat on, " : "") + "seed " + std::to_string(static_cast<int64_t>(w.seed)),
                   x + 2, y + 23, TEXT_DARK_GRAY);
        }
        g.p.clip_off();
    }

private:
    void select(int i) {
        selected_ = i;
        for (int id : {PLAY, RENAME, ERASE})
            if (Widget* w = find(id)) w->enabled = true;
    }
    void clamp_scroll() {
        float max_scroll = std::max(0.0f, static_cast<float>(worlds_.size()) * ENTRY + 8 - (gh_ - 64 - TOP));
        scroll_ = std::clamp(scroll_, 0.0f, max_scroll);
    }
    static std::string date(int64_t ms) {
        std::time_t t = static_cast<std::time_t>(ms / 1000);
        std::tm* tm = std::localtime(&t);
        if (!tm) return "?";
        char buf[32];
        std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", tm);
        return buf;
    }
    std::unique_ptr<Screen> rename_screen(const WorldInfo& w);
    std::unique_ptr<Screen> delete_screen(const WorldInfo& w);

    std::vector<WorldInfo> worlds_;
    int selected_ = -1;
    float scroll_ = 0;
    double last_click_ = -1;
    int gw_ = 0, gh_ = 0;
};

// --- Create, rename and delete -------------------------------------------------------------------

class CreateWorldScreen : public Screen {
public:
    CreateWorldScreen(App& a, std::string name, std::string seed) : Screen(a), name_(std::move(name)), seed_(std::move(seed)) {}
    enum { NAME, SEED, CREATE, CANCEL };

    void layout(int gw, int gh) override {
        Widget name = text_field(NAME, gw / 2 - 100, 60, 200, name_, 32);
        name.focused = true;
        widgets.push_back(name);
        widgets.push_back(text_field(SEED, gw / 2 - 100, 116, 200, seed_, 32));
        widgets.push_back(button(CREATE, gw / 2 - 155, gh / 4 + 96 + 12, 150, "Create New World", !trim(name_).empty()));
        widgets.push_back(button(CANCEL, gw / 2 + 5, gh / 4 + 96 + 12, 150, "Cancel"));
    }
    void on_text(int id, const std::string& text) override {
        if (id == NAME) name_ = text;
        if (id == SEED) seed_ = text;
        if (Widget* w = find(CREATE)) w->enabled = !trim(name_).empty();
    }
    void on_button(int id) override {
        if (id == CREATE && !trim(name_).empty()) app.create_world(trim(name_), seed_);
        if (id == CANCEL) app.open(select_world_screen(app));
    }
    void on_enter() override { on_button(CREATE); }
    void on_escape() override { on_button(CANCEL); }
    void draw(Gui& g) override {
        float cx = g.width() / 2.0f, left = cx - 100;
        g.text("Create New World", cx, 20, TEXT_WHITE, 0.5f);
        g.text("World Name", left, 47, TEXT_GRAY);
        g.text("Will be saved in: " + unique_folder_name(app.saves_dir, trim(name_)), left, 85, TEXT_GRAY);
        g.text("Seed for the World Generator", left, 104, TEXT_GRAY);
        g.text("Leave blank for a random seed", left, 140, TEXT_GRAY);
    }

private:
    std::string name_, seed_;
};

class RenameWorldScreen : public Screen {
public:
    RenameWorldScreen(App& a, WorldInfo world) : Screen(a), world_(std::move(world)), name_(world_.name) {}
    enum { NAME, RENAME, CANCEL };

    void layout(int gw, int gh) override {
        Widget name = text_field(NAME, gw / 2 - 100, 60, 200, name_, 32);
        name.focused = true;
        widgets.push_back(name);
        widgets.push_back(button(RENAME, gw / 2 - 100, gh / 4 + 96 + 12, 200, "Rename", !trim(name_).empty()));
        widgets.push_back(button(CANCEL, gw / 2 - 100, gh / 4 + 120 + 12, 200, "Cancel"));
    }
    void on_text(int, const std::string& text) override {
        name_ = text;
        if (Widget* w = find(RENAME)) w->enabled = !trim(name_).empty();
    }
    void on_button(int id) override {
        if (id == RENAME && !trim(name_).empty()) rename_world(app.saves_dir, world_.folder, trim(name_));
        if (id == RENAME || id == CANCEL) app.open(select_world_screen(app));
    }
    void on_enter() override { on_button(RENAME); }
    void on_escape() override { on_button(CANCEL); }
    void draw(Gui& g) override {
        g.text("Rename World", g.width() / 2.0f, 20, TEXT_WHITE, 0.5f);
        g.text("Enter Name", g.width() / 2.0f - 100, 47, TEXT_GRAY);
    }

private:
    WorldInfo world_;
    std::string name_;
};

class DeleteWorldScreen : public Screen {
public:
    DeleteWorldScreen(App& a, WorldInfo world) : Screen(a), world_(std::move(world)) {}
    enum { ERASE, CANCEL };

    void layout(int gw, int gh) override {
        widgets.push_back(button(ERASE, gw / 2 - 155, gh / 6 + 96, 150, "Delete"));
        widgets.push_back(button(CANCEL, gw / 2 + 5, gh / 6 + 96, 150, "Cancel"));
    }
    void on_button(int id) override {
        if (id == ERASE) delete_world(app.saves_dir, world_.folder);
        app.open(select_world_screen(app));
    }
    void on_escape() override { on_button(CANCEL); }
    void draw(Gui& g) override {
        g.text("Are you sure you want to delete this world?", g.width() / 2.0f, 70, TEXT_WHITE, 0.5f);
        g.text("'" + world_.name + "' will be gone for good!", g.width() / 2.0f, 90, TEXT_GRAY, 0.5f);
    }

private:
    WorldInfo world_;
};

std::unique_ptr<Screen> SelectWorldScreen::rename_screen(const WorldInfo& w) { return std::make_unique<RenameWorldScreen>(app, w); }
std::unique_ptr<Screen> SelectWorldScreen::delete_screen(const WorldInfo& w) { return std::make_unique<DeleteWorldScreen>(app, w); }

// --- Options -------------------------------------------------------------------------------------

// Base for the settings screens: two columns of 150-wide buttons, and Done at the bottom.
class SettingsScreen : public Screen {
public:
    SettingsScreen(App& a, ScreenFactory back) : Screen(a), back_(std::move(back)) {}
    static constexpr int DONE = 100;

    void on_escape() override { app.open(back_()); }
    bool over_game() const override { return app.game != nullptr; }

protected:
    static int column_x(int gw, int i) { return gw / 2 - 155 + (i % 2) * 160; }
    static int row_y(int gh, int i) { return gh / 6 + 24 * (i / 2); }
    void add_done(int gw, int gh) { widgets.push_back(button(DONE, gw / 2 - 100, gh / 6 + 168, 200, "Done")); }
    void relabel() {
        int gw = app.painter->width() / app.gui_scale(), gh = app.painter->height() / app.gui_scale();
        widgets.clear();
        layout(gw, gh);
    }
    ScreenFactory back_;
};

class OptionsScreen : public SettingsScreen {
public:
    using SettingsScreen::SettingsScreen;
    enum { MUSIC, SOUND, INVERT, SENSITIVITY, DIFFICULTY, VIDEO = 10, CONTROLS };

    void layout(int gw, int gh) override {
        const Options& o = app.options;
        // Beta's order: Music, Sound, Invert Mouse, Sensitivity, Difficulty.
        widgets.push_back(slider(MUSIC, column_x(gw, 0), row_y(gh, 0), 150, volume_label("Music", o.music_volume), o.music_volume));
        widgets.push_back(slider(SOUND, column_x(gw, 1), row_y(gh, 1), 150, volume_label("Sound", o.sound_volume), o.sound_volume));
        widgets.push_back(button(INVERT, column_x(gw, 2), row_y(gh, 2), 150, "Invert Mouse: " + on_off(o.invert_mouse)));
        widgets.push_back(slider(SENSITIVITY, column_x(gw, 3), row_y(gh, 3), 150,
                                 "Sensitivity: " + percent(o.sensitivity * 2), o.sensitivity));
        widgets.push_back(button(DIFFICULTY, column_x(gw, 4), row_y(gh, 4), 150, difficulty_label(o.difficulty)));
        widgets.push_back(button(VIDEO, gw / 2 - 100, gh / 6 + 96 + 12, 200, "Video Settings..."));
        widgets.push_back(button(CONTROLS, gw / 2 - 100, gh / 6 + 120 + 12, 200, "Controls..."));
        add_done(gw, gh);
    }
    void on_slider(int id, float v) override {
        Options& o = app.options;
        if (id == SENSITIVITY) o.sensitivity = v;
        if (id == MUSIC) o.music_volume = v;
        if (id == SOUND) o.sound_volume = v;
        if (id == MUSIC || id == SOUND) app.apply_volumes();
        update_labels();
    }
    void on_button(int id) override {
        Options& o = app.options;
        App& a = app;
        ScreenFactory back = back_;
        if (id == INVERT) {
            o.invert_mouse = !o.invert_mouse;
            a.options_changed();
            update_labels();
        }
        if (id == DIFFICULTY) {
            o.difficulty = (o.difficulty + 1) % DIFFICULTY_COUNT;
            a.options_changed();
            update_labels();
        }
        auto self = [&a, back] { return options_screen(a, back); };
        if (id == VIDEO) a.open(video_settings_screen(a, self));
        if (id == CONTROLS) a.open(controls_screen(a, self));
        if (id == DONE) a.open(back());
    }
    void draw(Gui& g) override { g.text("Options", g.width() / 2.0f, 20, TEXT_WHITE, 0.5f); }
    // Sliders don't write options.txt on every move; it is written when the screen closes.
    ~OptionsScreen() override { app.options_changed(); }

private:
    void update_labels() {
        const Options& o = app.options;
        find(SENSITIVITY)->label = "Sensitivity: " + percent(o.sensitivity * 2);
        find(MUSIC)->label = volume_label("Music", o.music_volume);
        find(SOUND)->label = volume_label("Sound", o.sound_volume);
        find(INVERT)->label = "Invert Mouse: " + on_off(o.invert_mouse);
        find(DIFFICULTY)->label = difficulty_label(o.difficulty);
    }
    static std::string difficulty_label(int d) { return std::string("Difficulty: ") + difficulty_name(d); }
    static std::string volume_label(const char* name, float v) { return std::string(name) + ": " + (v <= 0 ? "OFF" : percent(v)); }
};

class VideoSettingsScreen : public SettingsScreen {
public:
    using SettingsScreen::SettingsScreen;
    enum { GRAPHICS, DISTANCE, SMOOTH, CLOUDS, BOBBING };

    void layout(int gw, int gh) override {
        const Options& o = app.options;
        widgets.push_back(button(GRAPHICS, column_x(gw, 0), row_y(gh, 0), 150,
                                 std::string("Graphics: ") + (o.fancy_graphics ? "Fancy" : "Fast")));
        widgets.push_back(button(DISTANCE, column_x(gw, 1), row_y(gh, 1), 150,
                                 std::string("Render Distance: ") + view_distance_name(o.view_distance)));
        widgets.push_back(button(SMOOTH, column_x(gw, 2), row_y(gh, 2), 150, "Smooth Lighting: " + on_off(o.smooth_lighting)));
        widgets.push_back(button(CLOUDS, column_x(gw, 3), row_y(gh, 3), 150, "Clouds: " + on_off(o.clouds)));
        widgets.push_back(button(BOBBING, column_x(gw, 4), row_y(gh, 4), 150, "View Bobbing: " + on_off(o.view_bobbing)));
        add_done(gw, gh);
    }
    void on_button(int id) override {
        Options& o = app.options;
        if (id == DONE) {
            app.open(back_());
            return;
        }
        if (id == GRAPHICS) o.fancy_graphics = !o.fancy_graphics;
        if (id == DISTANCE) o.view_distance = (o.view_distance + 1) % VIEW_COUNT;
        if (id == SMOOTH) o.smooth_lighting = !o.smooth_lighting;
        if (id == CLOUDS) o.clouds = !o.clouds;
        if (id == BOBBING) o.view_bobbing = !o.view_bobbing;
        app.options_changed();
        relabel();
    }
    void draw(Gui& g) override { g.text("Video Settings", g.width() / 2.0f, 20, TEXT_WHITE, 0.5f); }
};

class ControlsScreen : public SettingsScreen {
public:
    using SettingsScreen::SettingsScreen;

    void layout(int gw, int gh) override {
        const Options& o = app.options;
        for (int i = 0; i < KEY_ACTION_COUNT; ++i) {
            Widget b = button(i, column_x(gw, i), row_y(gh, i), 70, waiting_ == i ? "> " + key_name(o.keys[i]) + " <" : key_name(o.keys[i]));
            b.highlight = waiting_ == i;
            for (int j = 0; j < KEY_ACTION_COUNT; ++j) b.warning |= j != i && o.keys[j] == o.keys[i];
            widgets.push_back(b);
        }
        add_done(gw, gh);
    }
    bool on_raw_key(int key) override {
        if (waiting_ < 0) return false;
        if (key != GLFW_KEY_ESCAPE) {
            app.options.keys[waiting_] = key;
            app.options_changed();
        }
        waiting_ = -1;
        relabel();
        return true;
    }
    void on_button(int id) override {
        if (id == DONE) {
            app.open(back_());
            return;
        }
        waiting_ = id;
        relabel();
    }
    void draw(Gui& g) override {
        g.text("Controls", g.width() / 2.0f, 20, TEXT_WHITE, 0.5f);
        for (int i = 0; i < KEY_ACTION_COUNT; ++i)
            g.text(key_action_info(i).label, static_cast<float>(column_x(g.width(), i) + 76),
                   static_cast<float>(row_y(g.height(), i) + 6), TEXT_WHITE);
    }

private:
    int waiting_ = -1;
};

// --- Pause -------------------------------------------------------------------------------------------

class PauseScreen : public Screen {
public:
    using Screen::Screen;
    enum { BACK, OPTIONS, QUIT, ACHIEVEMENTS, STATISTICS };

    void layout(int gw, int gh) override {
        // Beta's game menu, with Achievements and Statistics side by side under Back to Game.
        widgets.push_back(button(BACK, gw / 2 - 100, gh / 4 + 8, 200, "Back to Game"));
        widgets.push_back(button(ACHIEVEMENTS, gw / 2 - 100, gh / 4 + 32, 98, "Achievements"));
        widgets.push_back(button(STATISTICS, gw / 2 + 2, gh / 4 + 32, 98, "Statistics"));
        widgets.push_back(button(OPTIONS, gw / 2 - 100, gh / 4 + 80, 200, "Options..."));
        widgets.push_back(button(QUIT, gw / 2 - 100, gh / 4 + 104, 200, "Save and Quit to Title"));
    }
    void on_button(int id) override {
        App& a = app;
        if (id == BACK) a.close_screen();
        if (id == ACHIEVEMENTS) a.open(achievements_screen(a, [&a] { return pause_screen(a); }));
        if (id == STATISTICS) a.open(statistics_screen(a, [&a] { return pause_screen(a); }));
        if (id == OPTIONS) a.open(options_screen(a, [&a] { return pause_screen(a); }));
        if (id == QUIT) a.save_and_quit_to_title();
    }
    void on_escape() override { app.close_screen(); }
    bool over_game() const override { return true; }
    void draw(Gui& g) override {
        g.text("Game menu", g.width() / 2.0f, 40, TEXT_WHITE, 0.5f);
        if (app.game && app.game->saving()) g.text("Saving world...", 4, static_cast<float>(g.height()) - 12, TEXT_GRAY);
    }
};

// --- Death ----------------------------------------------------------------------------------------------

class DeathScreen : public Screen {
public:
    using Screen::Screen;
    enum { RESPAWN, TITLE };

    void layout(int gw, int gh) override {
        widgets.push_back(button(RESPAWN, gw / 2 - 100, gh / 4 + 72, 200, "Respawn"));
        widgets.push_back(button(TITLE, gw / 2 - 100, gh / 4 + 96, 200, "Title menu"));
    }
    void on_button(int id) override {
        App& a = app;
        if (id == RESPAWN && a.game) {
            a.game->respawn();
            a.close_screen();
        }
        if (id == TITLE) a.save_and_quit_to_title();
    }
    void update(double) override {
        if (app.game && !app.game->vitals.dead()) app.close_screen();  // respawned some other way
    }
    bool over_game() const override { return true; }
    bool pauses_game() const override { return false; }
    void draw(Gui& g) override {
        // Beta's dark red wash, darker at the bottom, and "Game over!" twice the usual size.
        const float w = static_cast<float>(g.width()), h = static_cast<float>(g.height());
        const int bands = 16;
        for (int i = 0; i < bands; ++i) {
            const float t = i / (bands - 1.0f);
            g.rect(0, h * i / bands, w, h / bands + 1, {0.31f + 0.19f * t, 0.0f + 0.19f * t, 0.19f * t, 0.38f + 0.25f * t});
        }
        g.p.text("Game over!", std::floor(w / 2 * g.s), static_cast<float>(g.p.height()) - 60.0f * g.s, g.s * 2, 0.5f, 1, true,
                 TEXT_WHITE);
    }
};

// --- Sign ---------------------------------------------------------------------------------------------

class SignScreen : public Screen {
public:
    using Screen::Screen;
    enum { DONE = 100 };

    void layout(int gw, int gh) override {
        const SignText* t = app.game ? app.game->edited_sign() : nullptr;
        for (int i = 0; i < SIGN_LINES; ++i) {
            Widget f = text_field(i, gw / 2 - 50, gh / 4 + 24 + i * 14, 100, t ? (*t)[i] : "", SIGN_LINE_LENGTH);
            f.h = 12;
            f.focused = first_ && i == 0;
            widgets.push_back(f);
        }
        first_ = false;
        widgets.push_back(button(DONE, gw / 2 - 100, gh / 4 + 120, 200, "Done"));
    }
    void on_text(int id, const std::string& text) override {
        if (SignText* t = app.game ? app.game->edited_sign() : nullptr) (*t)[id] = text;
    }
    // Up and down (and Enter) go from line to line; Enter on the last line is done.
    bool on_raw_key(int key) override {
        const bool down = key == GLFW_KEY_DOWN || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
        if (!down && key != GLFW_KEY_UP) return false;
        int line = 0;
        for (const Widget& w : widgets)
            if (w.focused) line = w.id;
        if (line == SIGN_LINES - 1 && key != GLFW_KEY_DOWN && key != GLFW_KEY_UP) {
            finish();
            return true;
        }
        line = std::clamp(line + (down ? 1 : -1), 0, SIGN_LINES - 1);
        for (Widget& w : widgets) w.focused = w.kind == Widget::Kind::TextField && w.id == line;
        return true;
    }
    void on_button(int id) override {
        if (id == DONE) finish();
    }
    void on_escape() override { finish(); }
    void update(double) override {
        if (!app.game || !app.game->sign_editing) app.close_screen();  // the sign went
    }
    bool over_game() const override { return true; }
    void draw(Gui& g) override {
        g.text("Edit sign message:", g.width() / 2.0f, 40, TEXT_WHITE, 0.5f);
        // The board behind the lines, in the planks' colour.
        g.rect(g.width() / 2.0f - 56, g.height() / 4.0f + 18, 112, SIGN_LINES * 14 + 10, {0.62f, 0.48f, 0.29f, 1});
    }

private:
    void finish() {
        if (app.game) app.game->finish_sign();
        app.close_screen();
    }
    bool first_ = true;
};

}  // namespace

std::unique_ptr<Screen> sign_screen(App& app) { return std::make_unique<SignScreen>(app); }

std::unique_ptr<Screen> death_screen(App& app) { return std::make_unique<DeathScreen>(app); }

std::unique_ptr<Screen> title_screen(App& app) { return std::make_unique<TitleScreen>(app); }
std::unique_ptr<Screen> select_world_screen(App& app) { return std::make_unique<SelectWorldScreen>(app); }
std::unique_ptr<Screen> create_world_screen(App& app, std::string name, std::string seed) {
    return std::make_unique<CreateWorldScreen>(app, std::move(name), std::move(seed));
}
std::unique_ptr<Screen> options_screen(App& app, ScreenFactory back) { return std::make_unique<OptionsScreen>(app, std::move(back)); }
std::unique_ptr<Screen> video_settings_screen(App& app, ScreenFactory back) {
    return std::make_unique<VideoSettingsScreen>(app, std::move(back));
}
std::unique_ptr<Screen> controls_screen(App& app, ScreenFactory back) { return std::make_unique<ControlsScreen>(app, std::move(back)); }
std::unique_ptr<Screen> pause_screen(App& app) { return std::make_unique<PauseScreen>(app); }
