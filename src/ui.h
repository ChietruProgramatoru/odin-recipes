// 2D drawing for the HUD and the menus: coloured rectangles, atlas tiles, icons and pixel text,
// batched into one draw call per texture.
#pragma once
#include <string>
#include <vector>

#include "items.h"
#include "survival.h"
#include "gl.h"
#include "renderer.h"

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
};

class Painter {
public:
    explicit Painter(GLuint block_atlas);
    ~Painter();
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;

    // Coordinates are pixels with the origin at the bottom left of the window.
    void begin(int width, int height);
    void end();
    void flush();
    int width() const { return screen_w_; }
    int height() const { return screen_h_; }

    void rect(float x, float y, float w, float h, Color c);
    void rect(float x, float y, float w, float h, float r, float g, float b, float a) { rect(x, y, w, h, {r, g, b, a}); }
    // Part of an atlas tile: (u0, v0)-(u1, v1) in 0..1 of the tile, v = 0 at the tile's bottom.
    void tile(int tile, float x, float y, float w, float h, Color tint, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);
    void icon(const std::string& name, float x, float y, float size);
    // An item in a size x size square: blocks as little 3D cubes, other items as flat icons.
    void item(ItemId item, float x, float y, float size);
    // An item with its count (bottom right) and a wear bar for used tools. count_scale 0 hides the count.
    void stack(const Stack& stack, float x, float y, float size, int count_scale);
    // Any four-cornered piece of an atlas tile; corners go counter-clockwise, uv in 0..1 of the tile.
    void tile_quad(int tile, const float xy[4][2], const float uv[4][2], Color tint);
    // anchor_x: 0 left, 0.5 centre, 1 right; anchor_y: 0 bottom, 0.5 centre, 1 top. Drawn with a drop shadow unless `shadow` is false.
    void text(const std::string& s, float x, float y, int scale, float anchor_x = 0, float anchor_y = 0,
              bool centre_lines = false, Color color = {}, bool shadow = true);
    // Big text where every pixel of the font is an atlas tile (the title screen logo).
    void block_text(const std::string& s, float x, float y, float pixel, int tile);
    static float text_width(const std::string& s, int scale);

    // Only draw inside this rectangle (window pixels, bottom-left origin) until clip_off().
    void clip(int x, int y, int w, int h);
    void clip_off();

private:
    struct Quad {
        float x[4], y[4], u[4], v[4];  // counter-clockwise corners
        Color c;
        static Quad box(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c) {
            return {{x0, x1, x1, x0}, {y0, y0, y1, y1}, {u0, u1, u1, u0}, {v0, v0, v1, v1}, c};
        }
    };
    void cube_icon(const ItemIcon& icon, float x, float y, float size);
    void use_texture(GLuint tex);

    GLuint block_atlas_, ui_tex_;
    int ui_w_, ui_h_;
    ShaderProgram program_;
    GLuint vao_ = 0, vbo_ = 0;
    std::vector<Quad> quads_;
    GLuint current_tex_ = 0;
    int screen_w_ = 1, screen_h_ = 1;
};
