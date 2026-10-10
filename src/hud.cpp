#include "hud.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "font.h"
#include "textures.h"

namespace {

const char* HELP =
    "WASD move   SPACE jump/swim   SHIFT sprint\n"
    "Left mouse: mine block / hit cow (hold to mine)\n"
    "Right mouse: place block / eat food\n"
    "1-9 or wheel: pick slot   F: craft 1 log -> 4 planks\n"
    "C: creative mode (double-tap SPACE to fly, SHIFT down)\n"
    "ESC: pause   H: hide this help";

const char* HUD_VERTEX = R"(#version 330 core
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

const char* HUD_FRAGMENT = R"(#version 330 core
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

// Pyglet font size (points) -> integer pixel scale for the 7-pixel-tall font.
int font_scale(int points) { return std::max(1, (points * 4 / 3 + 4) / 8); }

}  // namespace

Hud::Hud(GLuint block_atlas)
    : block_atlas_(block_atlas), ui_w_(UI_W), ui_h_(UI_H), program_(HUD_VERTEX, HUD_FRAGMENT) {
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

Hud::~Hud() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteTextures(1, &ui_tex_);
}

void Hud::show_message(const std::string& text, float seconds) {
    message_ = text;
    message_timer_ = seconds;
}

void Hud::show_item_name(std::optional<ItemId> item) {
    item_name_ = item ? item_name(*item) : "";
    item_name_timer_ = 2.0f;
}

void Hud::update(float dt) {
    item_name_timer_ -= dt;
    message_timer_ -= dt;
    damage_alpha_ = std::max(0.0f, damage_alpha_ - dt * 1.2f);
}

// --- Drawing helpers ------------------------------------------------------------------

void Hud::flush() {
    if (quads_.empty()) return;
    std::vector<float> v;
    v.reserve(quads_.size() * 6 * 8);
    for (const Quad& q : quads_) {
        const float corners[6][4] = {{q.x0, q.y0, q.u0, q.v0}, {q.x1, q.y0, q.u1, q.v0}, {q.x1, q.y1, q.u1, q.v1},
                                     {q.x0, q.y0, q.u0, q.v0}, {q.x1, q.y1, q.u1, q.v1}, {q.x0, q.y1, q.u0, q.v1}};
        for (const auto& c : corners) v.insert(v.end(), {c[0], c[1], c[2], c[3], q.r, q.g, q.b, q.a});
    }
    glBindTexture(GL_TEXTURE_2D, current_tex_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quads_.size() * 6));
    quads_.clear();
}

void Hud::rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    if (current_tex_ != ui_tex_) {
        flush();
        current_tex_ = ui_tex_;
    }
    float u = (WHITE_X + 1.0f) / ui_w_, v = 1.0f - (WHITE_Y + 1.0f) / ui_h_;
    quads_.push_back({x, y, x + w, y + h, u, v, u, v, r, g, b, a});
}

void Hud::icon(const std::string& name, float x, float y, float size) {
    if (current_tex_ != ui_tex_) {
        flush();
        current_tex_ = ui_tex_;
    }
    int i = static_cast<int>(std::find(std::begin(ICON_NAMES), std::end(ICON_NAMES), name) - std::begin(ICON_NAMES));
    float px = static_cast<float>(i * (ICON_W + 1));
    float u0 = px / ui_w_, u1 = (px + ICON_W) / ui_w_;
    float v1 = 1.0f - static_cast<float>(ICON_Y) / ui_h_, v0 = 1.0f - static_cast<float>(ICON_Y + ICON_H) / ui_h_;
    quads_.push_back({x, y, x + size, y + size * ICON_H / ICON_W, u0, v0, u1, v1, 1, 1, 1, 1});
}

void Hud::item(ItemId id, float x, float y, float size) {
    if (current_tex_ != block_atlas_) {
        flush();
        current_tex_ = block_atlas_;
    }
    auto uv = tile_uv(icon_tile(id));
    quads_.push_back({x, y, x + size, y + size, uv[0], uv[1], uv[2], uv[3], 1, 1, 1, 1});
}

void Hud::text(const std::string& s, float x, float y, int scale, float anchor_x, float anchor_y, bool centre_lines) {
    if (s.empty()) return;
    if (current_tex_ != ui_tex_) {
        flush();
        current_tex_ = ui_tex_;
    }
    const float w = static_cast<float>(text_width(s) * scale), h = static_cast<float>(text_height(s) * scale);
    const float left = std::floor(x - w * anchor_x), top = std::floor(y - h * anchor_y + h);
    std::vector<std::string> lines(1);
    for (char c : s) {
        if (c == '\n')
            lines.emplace_back();
        else
            lines.back() += c;
    }
    const float sc = static_cast<float>(scale);
    for (int pass = 0; pass < 2; ++pass) {  // drop shadow first, then the text
        float off = pass == 0 ? sc : 0;
        float shade = pass == 0 ? 0.25f : 1.0f;
        for (size_t li = 0; li < lines.size(); ++li) {
            float lx = left;
            if (centre_lines) lx = std::floor(left + (w - text_width(lines[li]) * sc) / 2);
            float ly = top - li * LINE_HEIGHT * sc - GLYPH_H * sc;
            for (char c : lines[li]) {
                int col, row;
                glyph_cell(c, col, row);
                float u0 = static_cast<float>(col * CELL_W) / ui_w_, u1 = static_cast<float>(col * CELL_W + GLYPH_W) / ui_w_;
                float v1 = 1.0f - static_cast<float>(row * CELL_H) / ui_h_;
                float v0 = 1.0f - static_cast<float>(row * CELL_H + GLYPH_H) / ui_h_;
                quads_.push_back({lx + off, ly - off, lx + off + GLYPH_W * sc, ly - off + GLYPH_H * sc, u0, v0, u1, v1,
                                  shade, shade, shade, pass == 0 ? 0.8f : 1.0f});
                lx += GLYPH_ADVANCE * sc;
            }
        }
    }
}

// --- The HUD ----------------------------------------------------------------------------

void Hud::draw(int width, int height, const HudState& st) {
    screen_w_ = std::max(1, width);
    screen_h_ = std::max(1, height);
    const float w = static_cast<float>(screen_w_), h = static_cast<float>(screen_h_);
    const float slot = static_cast<float>(std::max(36, std::min(64, static_cast<int>(std::min(w / 14, h / 9)))));
    const float x0 = w / 2 - slot * 4.5f, y0 = 8;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    program_.use();
    glUniform2f(program_.uniform("screen"), w, h);
    glUniform1i(program_.uniform("image"), 0);
    glActiveTexture(GL_TEXTURE0);
    current_tex_ = ui_tex_;

    if (st.underwater) rect(0, 0, w, h, 25 / 255.f, 64 / 255.f, 180 / 255.f, 110 / 255.f);
    if (damage_alpha_ > 0) rect(0, 0, w, h, 200 / 255.f, 0, 0, damage_alpha_);

    const float ca = 220 / 255.f;
    rect(w / 2 - 10, h / 2 - 1, 20, 2, 1, 1, 1, ca);
    rect(w / 2 - 1, h / 2 - 10, 2, 20, 1, 1, 1, ca);

    // Hotbar
    const Inventory& inv = *st.inventory;
    rect(x0 + inv.selected * slot, y0, slot, slot, 1, 1, 1, 240 / 255.f);
    for (int i = 0; i < Inventory::SIZE; ++i)
        rect(x0 + i * slot + 3, y0 + 3, slot - 6, slot - 6, 25 / 255.f, 25 / 255.f, 25 / 255.f, 200 / 255.f);
    if (mining_) {
        rect(w / 2 - 50, h / 2 - 34, 100, 8, 0, 0, 0, 150 / 255.f);
        rect(w / 2 - 48, h / 2 - 32, 96 * std::min(1.0f, *mining_), 4, 1, 1, 1, 230 / 255.f);
    }
    for (int i = 0; i < Inventory::SIZE; ++i) {
        const auto& s = inv.slots[i];
        if (!s) continue;
        float sx = x0 + i * slot;
        item(s->item, sx + slot * 0.18f, y0 + slot * 0.18f, slot * 0.64f);
    }
    const int count_scale = font_scale(std::max(9, static_cast<int>(slot) / 5));
    for (int i = 0; i < Inventory::SIZE; ++i) {
        const auto& s = inv.slots[i];
        if (!s || st.creative || s->count == 1) continue;
        text(std::to_string(s->count), x0 + i * slot + slot - 5, y0 + 5, count_scale, 1, 0);
    }

    // Hearts and hunger
    const float size = slot * 0.42f;
    const float row_y = y0 + slot + 6;
    if (!st.creative)
        for (int i = 0; i < 10; ++i) {
            int hp = st.vitals->health - i * 2;
            icon(hp >= 2 ? "heart_full" : hp == 1 ? "heart_half" : "heart_empty", x0 + i * size * 1.05f, row_y, size);
            int fd = st.vitals->hunger - i * 2;
            icon(fd >= 2 ? "food_full" : fd == 1 ? "food_half" : "food_empty", x0 + slot * 9 - (i + 1) * size * 1.05f, row_y, size);
        }

    if (item_name_timer_ > 0) text(item_name_, w / 2, row_y + size + 16, font_scale(14), 0.5f, 0);
    if (message_timer_ > 0) text(message_, w / 2, h * 0.68f, font_scale(18), 0.5f, 0);
    text(st.info, 10, h - 10, font_scale(12), 0, 1);
    if (show_help) text(HELP, 10, h - 36, font_scale(11), 0, 1);

    if (st.dead) {
        rect(0, 0, w, h, 115 / 255.f, 0, 0, 150 / 255.f);
        text("You died!\n\nPress R to respawn", w / 2, h / 2, font_scale(30), 0.5f, 0.5f, true);
    } else if (st.paused) {
        text("Paused\nClick to resume", w / 2, h / 2, font_scale(26), 0.5f, 0.5f, true);
    }
    flush();
    glBindVertexArray(0);
}
