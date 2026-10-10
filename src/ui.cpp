#include "ui.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "font.h"
#include "textures.h"

namespace {

const char* UI_VERTEX = R"(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec4 color;
uniform vec2 screen;
out vec2 v_tex;
out vec4 v_color;
void main() {
    gl_Position = vec4(position / screen * 2.0 - 1.0, 0.0, 1.0);
    v_tex = tex;
    v_color = color;
}
)";

const char* UI_FRAGMENT = R"(#version 330 core
in vec2 v_tex;
in vec4 v_color;
uniform sampler2D image;
out vec4 out_color;
void main() {
    out_color = texture(image, v_tex) * v_color;
}
)";

// Where things are in the UI image: the font, the heart/food icons and a white pixel for plain rectangles.
constexpr int UI_W = 128, UI_H = 64;
constexpr int ICON_Y = 48, ICON_W = 9, ICON_H = 8;
constexpr int WHITE_X = 124, WHITE_Y = 0;
const char* ICON_NAMES[6] = {"heart_full", "heart_half", "heart_empty", "food_full", "food_half", "food_empty"};

Image make_ui_image() {
    Image img(UI_W, UI_H);
    img.paste(make_font_image(), 0, 0);
    auto icons = make_hud_icons();
    for (int i = 0; i < 6; ++i) img.paste(icons[ICON_NAMES[i]], i * (ICON_W + 1), ICON_Y);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) img.at(WHITE_X + x, WHITE_Y + y) = {255, 255, 255, 255};
    return img;
}

}  // namespace

Painter::Painter(GLuint block_atlas)
    : block_atlas_(block_atlas), ui_w_(UI_W), ui_h_(UI_H), program_(UI_VERTEX, UI_FRAGMENT) {
    ui_tex_ = upload_texture(make_ui_image());
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));
    for (GLuint i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
}

Painter::~Painter() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteTextures(1, &ui_tex_);
}

void Painter::begin(int width, int height) {
    screen_w_ = std::max(1, width);
    screen_h_ = std::max(1, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    program_.use();
    glUniform2f(program_.uniform("screen"), static_cast<float>(screen_w_), static_cast<float>(screen_h_));
    glUniform1i(program_.uniform("image"), 0);
    glActiveTexture(GL_TEXTURE0);
    current_tex_ = ui_tex_;
}

void Painter::end() {
    flush();
    clip_off();
    glBindVertexArray(0);
}

void Painter::flush() {
    if (quads_.empty()) return;
    std::vector<float> v;
    v.reserve(quads_.size() * 6 * 8);
    for (const Quad& q : quads_) {
        for (int i : {0, 1, 2, 0, 2, 3}) v.insert(v.end(), {q.x[i], q.y[i], q.u[i], q.v[i], q.c.r, q.c.g, q.c.b, q.c.a});
    }
    glBindTexture(GL_TEXTURE_2D, current_tex_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quads_.size() * 6));
    quads_.clear();
}

void Painter::use_texture(GLuint tex) {
    if (current_tex_ == tex) return;
    flush();
    current_tex_ = tex;
}

void Painter::clip(int x, int y, int w, int h) {
    flush();
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, std::max(0, w), std::max(0, h));
}

void Painter::clip_off() {
    flush();
    glDisable(GL_SCISSOR_TEST);
}

void Painter::rect(float x, float y, float w, float h, Color c) {
    use_texture(ui_tex_);
    float u = (WHITE_X + 1.0f) / ui_w_, v = 1.0f - (WHITE_Y + 1.0f) / ui_h_;
    quads_.push_back(Quad::box(x, y, x + w, y + h, u, v, u, v, c));
}

void Painter::tile(int tile, float x, float y, float w, float h, Color tint, float u0, float v0, float u1, float v1) {
    use_texture(block_atlas_);
    auto uv = tile_uv(tile);
    float du = uv[2] - uv[0], dv = uv[3] - uv[1];
    quads_.push_back(Quad::box(x, y, x + w, y + h, uv[0] + du * u0, uv[1] + dv * v0, uv[0] + du * u1, uv[1] + dv * v1, tint));
}

void Painter::icon(const std::string& name, float x, float y, float size) {
    use_texture(ui_tex_);
    int i = static_cast<int>(std::find(std::begin(ICON_NAMES), std::end(ICON_NAMES), name) - std::begin(ICON_NAMES));
    float px = static_cast<float>(i * (ICON_W + 1));
    float u0 = px / ui_w_, u1 = (px + ICON_W) / ui_w_;
    float v1 = 1.0f - static_cast<float>(ICON_Y) / ui_h_, v0 = 1.0f - static_cast<float>(ICON_Y + ICON_H) / ui_h_;
    quads_.push_back(Quad::box(x, y, x + size, y + size * ICON_H / ICON_W, u0, v0, u1, v1, {}));
}

void Painter::tile_quad(int tile, const float xy[4][2], const float uv[4][2], Color tint) {
    use_texture(block_atlas_);
    auto t = tile_uv(tile);
    Quad q;
    for (int i = 0; i < 4; ++i) {
        q.x[i] = xy[i][0];
        q.y[i] = xy[i][1];
        q.u[i] = t[0] + (t[2] - t[0]) * uv[i][0];
        q.v[i] = t[1] + (t[3] - t[1]) * uv[i][1];
    }
    q.c = tint;
    quads_.push_back(q);
}

void Painter::cube_icon(const ItemIcon& icon, float x, float y, float size) {
    // Isometric view of boxes inside a unit cube: x runs to the lower right, z to the lower left, y up.
    const float s = size * 0.94f, cx = x + size / 2, by = y + (size - s) / 2;
    auto project = [&](float px, float py, float pz, float* out) {
        out[0] = cx + (px - pz) * s / 2;
        out[1] = by + s * (0.5f * py + 0.5f - 0.25f * (px + pz));
    };
    auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        float xy[4][2];
        // Top face: u along x, v along z.
        project(x0, y1, z1, xy[0]);
        project(x1, y1, z1, xy[1]);
        project(x1, y1, z0, xy[2]);
        project(x0, y1, z0, xy[3]);
        const float top_uv[4][2] = {{x0, 1 - z1}, {x1, 1 - z1}, {x1, 1 - z0}, {x0, 1 - z0}};
        tile_quad(icon.top, xy, top_uv, {1, 1, 1, 1});
        // Left face (z = z1): u along x, v up.
        project(x0, y0, z1, xy[0]);
        project(x1, y0, z1, xy[1]);
        project(x1, y1, z1, xy[2]);
        project(x0, y1, z1, xy[3]);
        const float side_uv[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        tile_quad(icon.left, xy, side_uv, {0.8f, 0.8f, 0.8f, 1});
        // Right face (x = x1): u from front (z1) to back (z0), v up.
        project(x1, y0, z1, xy[0]);
        project(x1, y0, z0, xy[1]);
        project(x1, y1, z0, xy[2]);
        project(x1, y1, z1, xy[3]);
        const float right_uv[4][2] = {{1 - z1, y0}, {1 - z0, y0}, {1 - z0, y1}, {1 - z1, y1}};
        tile_quad(icon.right, xy, right_uv, {0.6f, 0.6f, 0.6f, 1});
    };
    switch (icon.shape) {
    case ItemIcon::Slab: box(0, 0, 0, 1, 0.5f, 1); break;
    case ItemIcon::Stairs:
        box(0, 0, 0, 1, 0.5f, 1);
        box(0, 0.5f, 0, 1, 1, 0.5f);
        break;
    default: box(0, 0, 0, 1, 1, 1); break;
    }
}

void Painter::item(ItemId id, float x, float y, float size) {
    const ItemInfo* info = item_info(id);
    if (info && info->icon.shape != ItemIcon::Flat)
        cube_icon(info->icon, x, y, size);
    else
        tile(icon_tile(id), x, y, size, size, {});
}

void Painter::stack(const Stack& s, float x, float y, float size, int count_scale) {
    item(s.item, x, y, size);
    if (int most = max_damage(s.item); most > 0 && s.damage > 0) {
        // Wear bar: green when new, red when nearly broken.
        float left = std::clamp(1.0f - static_cast<float>(s.damage) / most, 0.0f, 1.0f);
        float bw = size * 0.85f, bh = std::max(2.0f, size / 8), bx = x + (size - bw) / 2, bar_y = y + size * 0.02f;
        rect(bx, bar_y, bw, bh, 0, 0, 0, 1);
        rect(bx, bar_y + bh / 2, bw * left, bh / 2, 1 - left, left, 0, 1);
    }
    if (count_scale > 0 && s.count != 1)
        text(std::to_string(s.count), x + size + count_scale, y - count_scale, count_scale, 1, 0);
}

float Painter::text_width(const std::string& s, int scale) { return static_cast<float>(::text_width(s) * scale); }

void Painter::text(const std::string& s, float x, float y, int scale, float anchor_x, float anchor_y, bool centre_lines,
                   Color color, bool shadow) {
    if (s.empty()) return;
    use_texture(ui_tex_);
    const float w = static_cast<float>(::text_width(s) * scale), h = static_cast<float>(text_height(s) * scale);
    const float left = std::floor(x - w * anchor_x), top = std::floor(y - h * anchor_y + h);
    std::vector<std::string> lines(1);
    for (char c : s) {
        if (c == '\n')
            lines.emplace_back();
        else
            lines.back() += c;
    }
    const float sc = static_cast<float>(scale);
    for (int pass = shadow ? 0 : 1; pass < 2; ++pass) {  // drop shadow first, then the text
        float off = pass == 0 ? sc : 0;
        Color c = pass == 0 ? Color{color.r * 0.25f, color.g * 0.25f, color.b * 0.25f, 0.8f * color.a} : color;
        for (size_t li = 0; li < lines.size(); ++li) {
            float lx = left;
            if (centre_lines) lx = std::floor(left + (w - ::text_width(lines[li]) * sc) / 2);
            float ly = top - li * LINE_HEIGHT * sc - GLYPH_H * sc;
            for (char ch : lines[li]) {
                int col, row;
                glyph_cell(ch, col, row);
                float u0 = static_cast<float>(col * CELL_W) / ui_w_, u1 = static_cast<float>(col * CELL_W + GLYPH_W) / ui_w_;
                float v1 = 1.0f - static_cast<float>(row * CELL_H) / ui_h_;
                float v0 = 1.0f - static_cast<float>(row * CELL_H + GLYPH_H) / ui_h_;
                quads_.push_back(Quad::box(lx + off, ly - off, lx + off + GLYPH_W * sc, ly - off + GLYPH_H * sc, u0, v0, u1, v1, c));
                lx += GLYPH_ADVANCE * sc;
            }
        }
    }
}

void Painter::block_text(const std::string& s, float x, float y, float pixel, int tile_index) {
    // (x, y) is the top left corner. A dark copy one pixel down-right gives the letters depth.
    for (int pass = 0; pass < 2; ++pass) {
        float off = pass == 0 ? pixel * 0.35f : 0;
        Color tint = pass == 0 ? Color{0.2f, 0.2f, 0.2f, 1} : Color{1, 1, 1, 1};
        float lx = x;
        for (char ch : s) {
            const uint8_t* rows = glyph_rows(ch);
            for (int r = 0; r < GLYPH_H; ++r)
                for (int c = 0; c < GLYPH_W; ++c) {
                    if (!(rows[r] & (0x10 >> c))) continue;
                    float px = lx + c * pixel + off, py = y - (r + 1) * pixel - off;
                    // Each font pixel shows a different part of the tile, so the letters look carved from stone.
                    float u0 = static_cast<float>((c * 3) % 13) / 16.0f, v0 = static_cast<float>((r * 5) % 13) / 16.0f;
                    tile(tile_index, px, py, pixel, pixel, tint, u0, v0, u0 + 3 / 16.0f, v0 + 3 / 16.0f);
                }
            lx += GLYPH_ADVANCE * pixel;
        }
    }
}
