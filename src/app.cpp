#include "app.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <random>

#include <GLFW/glfw3.h>

#include "audio.h"
#include "autotest.h"
#include "game.h"
#include "menu.h"
#include "png.h"
#include "save.h"
#include "textures.h"

namespace fs = std::filesystem;

App::App(GLFWwindow* w, AppConfig cfg) : window(w), config(std::move(cfg)) {
    saves_dir = config.data_dir / "saves";
    std::error_code ec;
    fs::create_directories(saves_dir, ec);
    options = Options::load(config.data_dir / "options.txt");
    stats = load_stats(stats_file());
    stats.add(STAT_GAMES_STARTED);
    atlas = upload_texture(make_atlas(make_tiles()));
    painter = std::make_unique<Painter>(atlas);
    audio = std::make_unique<Audio>(config.autotest == nullptr);
    apply_volumes();
    screen_ = title_screen(*this);
}

App::~App() {
    screen_.reset();
    pending_.reset();
    if (audio) audio->stop_all();
    game.reset();
    painter.reset();
    glDeleteTextures(1, &atlas);
    save_stats(stats_file(), stats);
}

int App::gui_scale() const {
    // Like Beta's automatic GUI scale: as big as fits a 320 x 240 menu.
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    int s = 1;
    while (w / (s + 1) >= 320 && h / (s + 1) >= 240) ++s;
    return s;
}

double App::gui_x() const {
    int ww, wh, fw, fh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fw, &fh);
    return cursor_x_ * fw / std::max(1, ww) / gui_scale();
}

double App::gui_y() const {
    int ww, wh, fw, fh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fw, &fh);
    return cursor_y_ * fh / std::max(1, wh) / gui_scale();
}

// --- main loop -----------------------------------------------------------------------------

void App::run() {
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window) && !quit) {
        glfwPollEvents();
        apply_pending_screen();
        double now = glfwGetTime();
        double dt = config.autotest ? 1.0 / 60 : now - last;
        last = now;
        update(dt);
        if (quit) break;
        draw();
        if (want_screenshot_) take_screenshot();
        glfwSwapBuffers(window);
    }
    if (game) {
        draw_saving("Saving world...");
        game->save(true);
        if (audio) audio->stop_all();
        game.reset();
    }
}

void App::update(double dt) {
    ++frame;
    if (dt > 0) fps_ += (1.0 / std::max(dt, 1e-4) - fps_) * 0.05;
    if (game) {
        game->update(dt);
        for (const SoundEvent& e : game->sounds) {
            if (e.key && !e.stop) {
                audio->stop_music();  // a disc takes over from the music (Beta)
                music_.rest();
            }
            audio->play(e, glm::dvec3(game->eye()), game->yaw);
        }
        game->sounds.clear();
        audio->move_listener(glm::dvec3(game->eye()), game->yaw);
    }
    // Now and then a piece of music, in the menus and in the game alike.
    if (auto piece = music_.update(dt, audio->music_busy())) {
        SoundEvent e{*piece, {}};
        e.channel = Channel::Music;
        audio->play(e, {}, 0);
    }
    if (game && game->vitals.dead() && !screen_ && !has_pending_) open(death_screen(*this));
    if (game && game->sign_editing && !screen_ && !has_pending_) open(sign_screen(*this));
    if (screen_) screen_->update(dt);
    if (config.autotest) (*config.autotest)(*this);
    apply_pending_screen();
}

void App::draw() {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    if (game) {
        game->draw(w, h);
    } else {
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    if (!screen_) return;
    int s = gui_scale();
    screen_->ensure_layout(w / s, h / s);
    painter->begin(w, h);
    Gui gui(*painter, s);
    screen_->draw_all(gui, gui_x(), gui_y(), frame);
    painter->end();
}

void App::draw_saving(const char* text) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    painter->begin(w, h);
    Gui gui(*painter, gui_scale());
    gui.dirt(0, 0, static_cast<float>(gui.width()), static_cast<float>(gui.height()), 0.25f);
    gui.text(text, gui.width() / 2.0f, gui.height() / 2.0f - 4, TEXT_WHITE, 0.5f);
    painter->end();
    glfwSwapBuffers(window);
}

bool App::screenshot(const fs::path& path) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3), flipped(rgb.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    const size_t stride = static_cast<size_t>(w) * 3;
    for (int y = 0; y < h; ++y) std::copy_n(rgb.data() + (h - 1 - y) * stride, stride, flipped.data() + y * stride);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    return write_png(path.string(), w, h, flipped);
}

void App::take_screenshot() {
    want_screenshot_ = false;
    std::time_t t = std::time(nullptr);
    char stamp[64] = "screenshot";
    if (std::tm* tm = std::localtime(&t)) std::strftime(stamp, sizeof stamp, "%Y-%m-%d_%H.%M.%S", tm);
    fs::path dir = config.data_dir / "screenshots";
    std::string name = std::string(stamp) + ".png";
    std::error_code ec;
    for (int n = 2; fs::exists(dir / name, ec); ++n) name = std::string(stamp) + "_" + std::to_string(n) + ".png";
    bool ok = screenshot(dir / name);
    if (game) game->hud->show_message(ok ? "Saved screenshot as " + name : "Couldn't save the screenshot", 3);
}

// --- menus and worlds ---------------------------------------------------------------------------

void App::open(std::unique_ptr<Screen> s) {
    pending_ = std::move(s);
    has_pending_ = true;
}

void App::close_screen() { open(nullptr); }

void App::apply_pending_screen() {
    if (!has_pending_) return;
    has_pending_ = false;
    screen_ = std::move(pending_);  // the old screen is destroyed here, outside its own event handlers
    if (game) {
        game->set_paused(screen_ != nullptr && screen_->pauses_game());
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (!screen_) screen_ = title_screen(*this);
    }
}

bool App::play_world(const std::string& folder) {
    fs::path dir = saves_dir / folder;
    auto level = read_level(dir);
    if (!level) return false;
    draw_saving("Loading world...");
    if (audio) audio->stop_all();
    game.reset();
    GameSetup setup{dir, std::move(*level), config.autotest != nullptr, &stats, stats_file()};
    stats.add(STAT_WORLDS_LOADED);
    game = std::make_unique<Game>(window, atlas, *painter, options, std::move(setup));
    game->save(false);  // so "last played" is right even if the game crashes later
    close_screen();
    return true;
}

bool App::create_world(const std::string& name, const std::string& seed_text) {
    uint64_t seed;
    if (auto s = seed_from_text(seed_text)) {
        seed = *s;
    } else {
        std::random_device rd;
        seed = (static_cast<uint64_t>(rd()) << 32) ^ rd() ^ static_cast<uint64_t>(std::time(nullptr));
    }
    auto folder = ::create_world(saves_dir, name, seed);
    if (folder) stats.add(STAT_WORLDS_CREATED);
    return folder && play_world(*folder);
}

void App::pause() {
    if (!game || screen_) return;
    game->save(false);
    open(pause_screen(*this));
    apply_pending_screen();
}

void App::save_and_quit_to_title() {
    if (game) {
        draw_saving("Saving world...");
        game->save(true);
        if (audio) audio->stop_all();
        game.reset();
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    open(title_screen(*this));
}

void App::options_changed() {
    options.save(config.data_dir / "options.txt");
    apply_volumes();
    if (game) game->apply_options();
}

void App::apply_volumes() { audio->set_volumes(options.sound_volume, options.music_volume); }

void App::click_sound() {
    SoundEvent e{Sound::UiClick, {}};
    e.channel = Channel::Ui;
    audio->play(e, {}, 0);
}

// --- input ---------------------------------------------------------------------------------------

void App::on_key(int key, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_F2) {
        want_screenshot_ = true;
        return;
    }
    if (screen_) {
        if (action != GLFW_RELEASE) screen_->key(key);
        apply_pending_screen();
        return;
    }
    if (!game) return;
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        if (game->gui)
            game->close_gui();  // Escape closes the inventory first, like E
        else
            pause();
        return;
    }
    game->on_key(key, action, mods);
}

void App::on_char(unsigned int codepoint) {
    if (screen_) screen_->character(codepoint);
    apply_pending_screen();
}

void App::on_mouse_button(int button, int action) {
    if (screen_) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS)
                screen_->mouse_down(gui_x(), gui_y());
            else if (action == GLFW_RELEASE)
                screen_->mouse_up();
        }
        apply_pending_screen();
        return;
    }
    if (game) game->on_mouse_button(button, action);
}

void App::on_cursor(double x, double y) {
    cursor_x_ = x;
    cursor_y_ = y;
    if (screen_)
        screen_->mouse_move(gui_x(), gui_y());
    else if (game)
        game->on_cursor(x, y);
}

void App::on_scroll(double dy) {
    if (screen_)
        screen_->on_scroll(dy);
    else if (game)
        game->on_scroll(dy);
}

void App::on_focus(bool focused) {
    if (!focused && !config.autotest) pause();
}
