#include "hand.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "blocks.h"
#include "textures.h"

namespace {

constexpr float PI = 3.14159265f;

glm::mat4 translate(float x, float y, float z) { return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z)); }
glm::mat4 rot(float deg, glm::vec3 axis) { return glm::rotate(glm::mat4(1.0f), glm::radians(deg), axis); }
glm::mat4 rot_x(float deg) { return rot(deg, {1, 0, 0}); }
glm::mat4 rot_y(float deg) { return rot(deg, {0, 1, 0}); }
glm::mat4 rot_z(float deg) { return rot(deg, {0, 0, 1}); }
glm::mat4 scale(float s) { return glm::scale(glm::mat4(1.0f), glm::vec3(s)); }

void add_quad(MeshData& mesh, const std::array<glm::vec3, 4>& pts, const std::array<glm::vec2, 4>& uv, float shade) {
    uint32_t n = static_cast<uint32_t>(mesh.vertices.size());
    for (int c = 0; c < 4; ++c) mesh.vertices.push_back({pts[c].x, pts[c].y, pts[c].z, shade, uv[c].x, uv[c].y});
    mesh.indices.insert(mesh.indices.end(), {n, n + 1, n + 2, n, n + 2, n + 3});
}

// A box from lo to hi inside the unit cube, moved so the cube is centred on the origin. Each face
// shows the part of its tile it would cover on a whole block, like a slab in the world.
void add_block_box(MeshData& mesh, glm::vec3 lo, glm::vec3 hi, int top, int bottom, int front, int side) {
    for (const FaceDef& f : face_defs()) {
        const int tile = f.normal.y > 0 ? top : f.normal.y < 0 ? bottom : f.normal.z > 0 ? front : side;
        const auto rect = tile_uv(tile);
        // The tile's u runs along the face from corner 0 to 1, its v from corner 0 to 3.
        const glm::ivec3 du = f.corners[1] - f.corners[0], dv = f.corners[3] - f.corners[0];
        auto along = [](glm::ivec3 d, glm::vec3 p) {
            for (int a = 0; a < 3; ++a)
                if (d[a]) return d[a] > 0 ? p[a] : 1 - p[a];
            return 0.0f;
        };
        std::array<glm::vec3, 4> pts;
        std::array<glm::vec2, 4> uv;
        for (int c = 0; c < 4; ++c) {
            glm::vec3 p = lo + glm::vec3(f.corners[c]) * (hi - lo);
            pts[c] = p - 0.5f;
            uv[c] = {rect[0] + along(du, p) * (rect[2] - rect[0]), rect[1] + along(dv, p) * (rect[3] - rect[1])};
        }
        add_quad(mesh, pts, uv, f.light);
    }
}

// Beta's held sprite: the icon on the square (0, 0) .. (1, 1), one pixel thick (z from -1/16 to 0),
// with the picture mirrored on the front face as Beta has it. Every edge of a solid pixel that
// borders a see-through one gets a little side face in that pixel's colour.
void add_sprite(MeshData& mesh, int tile, const Image& image) {
    const auto rect = tile_uv(tile);
    const float U0 = rect[0], V0 = rect[1], U1 = rect[2], V1 = rect[3];
    const float D = -1.0f / 16;
    const std::array<glm::vec2, 4> face_uv = {glm::vec2(U1, V0), glm::vec2(U0, V0), glm::vec2(U0, V1), glm::vec2(U1, V1)};
    add_quad(mesh, {glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(0, 1, 0)}, face_uv, 1.0f);
    add_quad(mesh, {glm::vec3(0, 0, D), glm::vec3(1, 0, D), glm::vec3(1, 1, D), glm::vec3(0, 1, D)}, face_uv, 0.8f);

    const int W = image.width, H = image.height;
    auto solid = [&](int c, int r) { return c >= 0 && c < W && r >= 0 && r < H && image.at(c, r).a >= 128; };
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            if (!solid(c, r)) continue;
            // Image column c is mirrored to x from (W-1-c)/W, row r (top first) to y from (H-1-r)/H.
            const float x0 = static_cast<float>(W - 1 - c) / W, x1 = x0 + 1.0f / W;
            const float y0 = static_cast<float>(H - 1 - r) / H, y1 = y0 + 1.0f / H;
            const glm::vec2 t(U0 + (c + 0.5f) / W * (U1 - U0), V1 - (r + 0.5f) / H * (V1 - V0));
            const std::array<glm::vec2, 4> uv = {t, t, t, t};
            if (!solid(c - 1, r)) add_quad(mesh, {glm::vec3(x1, y0, 0), glm::vec3(x1, y0, D), glm::vec3(x1, y1, D), glm::vec3(x1, y1, 0)}, uv, 0.7f);
            if (!solid(c + 1, r)) add_quad(mesh, {glm::vec3(x0, y0, D), glm::vec3(x0, y0, 0), glm::vec3(x0, y1, 0), glm::vec3(x0, y1, D)}, uv, 0.7f);
            if (!solid(c, r - 1)) add_quad(mesh, {glm::vec3(x0, y1, 0), glm::vec3(x1, y1, 0), glm::vec3(x1, y1, D), glm::vec3(x0, y1, D)}, uv, 0.9f);
            if (!solid(c, r + 1)) add_quad(mesh, {glm::vec3(x0, y0, D), glm::vec3(x1, y0, D), glm::vec3(x1, y0, 0), glm::vec3(x0, y0, 0)}, uv, 0.6f);
        }
}

}  // namespace

void Hand::swing() {
    if (swing_time_ < 0 || swing_time_ >= SWING_SECONDS / 2) swing_time_ = 0;
}

void Hand::update(double dt, std::optional<ItemId> held, int slot, HandMotion motion) {
    // In steps of at most a tick, so a long frame still dips and rises like short ones.
    while (dt > 0) {
        const double step = std::min(dt, 0.05);
        tick(step, held, slot, motion);
        dt -= step;
    }
}

void Hand::tick(double dt, std::optional<ItemId> held, int slot, HandMotion motion) {
    if (swing_time_ >= 0) {
        swing_time_ += dt;
        if (swing_time_ >= SWING_SECONDS) swing_time_ = -1;
    }

    const bool same = slot == shown_slot_ && held == shown_;
    const double target = same ? 1.0 : 0.0, most = EQUIP_RATE * dt;
    equipped_ += std::clamp(target - equipped_, -most, most);
    if (equipped_ < 0.1) {
        shown_ = held;
        shown_slot_ = slot;
    }

    // Beta, per tick: the bob eases 40% of the way towards the speed (at most 0.1 a tick) while on
    // the ground, and the view tips 80% of the way towards how fast the player rises or falls in the air.
    const double ticks = dt * 20;
    walked_ += motion.horizontal * dt * 0.6;
    const double speed = motion.on_ground ? std::min(motion.horizontal / 20, 0.1) : 0.0;
    bob_amount_ += (speed - bob_amount_) * (1 - std::pow(0.6, ticks));
    const double tip = motion.on_ground ? 0.0 : std::atan(-motion.vertical / 20 * 0.2) * 15;
    bob_pitch_ += (tip - bob_pitch_) * (1 - std::pow(0.2, ticks));
}

float Hand::swing_progress() const {
    return swing_time_ < 0 ? 0.0f : static_cast<float>(std::min(1.0, swing_time_ / SWING_SECONDS));
}

glm::mat4 Hand::bob() const {
    if (!bobbing) return glm::mat4(1.0f);
    // Beta's setupViewBobbing.
    const float w = static_cast<float>(-walked_) * PI, a = static_cast<float>(bob_amount_);
    return translate(std::sin(w) * a * 0.5f, -std::abs(std::cos(w) * a), 0) * rot_z(std::sin(w) * a * 3) *
           rot_x(std::abs(std::cos(w - 0.2f) * a) * 5) * rot_x(static_cast<float>(bob_pitch_));
}

std::vector<ModelPart> Hand::parts() const {
    // Beta 1.7.3's ItemRenderer, step for step.
    const std::optional<ItemId> held = shown_;
    const float p = swing_progress();
    const float arc = std::sin(p * PI), root = std::sin(std::sqrt(p) * PI), loop = std::sin(std::sqrt(p) * PI * 2);
    const float ease = std::sin(p * p * PI);
    const float equipped = this->equipped();
    if (held && valid_item(*held)) {
        glm::mat4 m = bob() * translate(-root * 0.4f, loop * 0.2f, -arc * 0.2f) *
                      translate(0.56f, -0.52f - (1 - equipped) * 0.6f, -0.72f) * rot_y(45) * rot_y(-ease * 20) *
                      rot_z(-root * 20) * rot_x(-root * 80) * scale(0.4f);
        if (*held == FISHING_ROD) m *= rot_y(180);  // Beta holds the rod the other way round
        if (!looks_like_block(*held))
            m *= translate(0, -0.3f, 0) * scale(1.5f) * rot_y(50) * rot_z(335) * translate(-0.9375f, -0.0625f, 0);
        return {{HELD_MODEL_BASE + *held, m}};
    }
    glm::mat4 m = bob() * translate(-root * 0.3f, loop * 0.4f, -arc * 0.4f) *
                  translate(0.64f, -0.6f - (1 - equipped) * 0.6f, -0.72f) * rot_y(45) * rot_y(root * 70) *
                  rot_z(-ease * 20) * translate(-1, 3.6f, 3.5f) * rot_z(120) * rot_x(200) * rot_y(-135) *
                  translate(5.6f, 0, 0);
    // Beta's right arm hangs from (-5, 2, 0) pixels, tipped out by 0.1 radians, with its box one pixel
    // further out. Beta's models have y down and face -z; ours have y up and face +z.
    m *= translate(-5.0f / 16, 2.0f / 16, 0) * rot_z(glm::degrees(0.1f)) * translate(-1.0f / 16, 0, 0) * rot_x(180);
    return {{MODEL_PLAYER_ARM, m}};
}

std::map<int, MeshData> build_held_models(const std::map<int, Image>& tiles) {
    std::map<int, MeshData> models;
    for (ItemId item : all_items()) {
        const ItemInfo* info = item_info(item);
        MeshData& mesh = models[HELD_MODEL_BASE + item];
        const ItemIcon& icon = info->icon;
        if (icon.shape == ItemIcon::Flat) {
            auto it = tiles.find(icon.top);
            if (it != tiles.end()) add_sprite(mesh, icon.top, it->second);
            continue;
        }
        // The icon has no bottom; a real block's own bottom is used when it differs from its top
        // (grass shows dirt), else the icon's top.
        int bottom = icon.top;
        if (const BlockInfo* b = item < BLOCK_COUNT ? block_info(item) : nullptr; b && b->tiles[2] != b->tiles[0]) bottom = b->tiles[2];
        switch (icon.shape) {
        case ItemIcon::Slab: add_block_box(mesh, {0, 0, 0}, {1, 0.5f, 1}, icon.top, bottom, icon.left, icon.right); break;
        case ItemIcon::Stairs:
            add_block_box(mesh, {0, 0, 0}, {1, 0.5f, 1}, icon.top, bottom, icon.left, icon.right);
            add_block_box(mesh, {0, 0.5f, 0}, {1, 1, 0.5f}, icon.top, bottom, icon.left, icon.right);
            break;
        default: add_block_box(mesh, {0, 0, 0}, {1, 1, 1}, icon.top, bottom, icon.left, icon.right); break;
        }
    }
    return models;
}
