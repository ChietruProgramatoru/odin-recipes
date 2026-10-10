// On-screen interface: crosshair, hotbar, hearts, hunger and overlays.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "gl.h"
#include "renderer.h"
#include "survival.h"

struct HudState {
    const Inventory* inventory;
    const Vitals* vitals;
    bool creative;
    std::string info;
    bool underwater, dead, paused;
};

class Hud {
public:
    explicit Hud(GLuint block_atlas);
    ~Hud();

    bool show_help = true;

    void flash_damage() { damage_alpha_ = 0.45f; }
    void show_message(const std::string& text, float seconds = 2.5f);
    void show_item_name(std::optional<ItemId> item);
    void set_mining(std::optional<float> progress) { mining_ = progress; }
    void update(float dt);
    void draw(int width, int height, const HudState& state);

private:
    struct Quad {
        float x0, y0, x1, y1;    // pixels, origin at the bottom left
        float u0, v0, u1, v1;
        float r, g, b, a;
    };
    void rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void icon(const std::string& name, float x, float y, float size);
    void item(ItemId item, float x, float y, float size);
    // anchor_x: 0 left, 0.5 centre, 1 right; anchor_y: 0 bottom, 0.5 centre, 1 top.
    void text(const std::string& s, float x, float y, int scale, float anchor_x = 0, float anchor_y = 0,
              bool centre_lines = false);
    void flush();

    GLuint block_atlas_, ui_tex_;
    int ui_w_, ui_h_;
    ShaderProgram program_;
    GLuint vao_ = 0, vbo_ = 0;
    std::vector<Quad> quads_;
    GLuint current_tex_ = 0;
    int screen_w_ = 1, screen_h_ = 1;

    std::string message_, item_name_;
    float message_timer_ = 0, item_name_timer_ = 0, damage_alpha_ = 0;
    std::optional<float> mining_;
};
