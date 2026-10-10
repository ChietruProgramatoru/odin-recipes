#include "mobs.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "textures.h"

namespace {

constexpr double PI = 3.14159265358979323846;
double radians(double deg) { return deg * PI / 180.0; }
double degrees(double rad) { return rad * 180.0 / PI; }

glm::mat4 translate(double x, double y, double z) {
    return glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}
// rot_y turns local +z towards (sin, 0, cos); rot_x with a positive angle tips +z downwards.
glm::mat4 rot(double deg, glm::vec3 axis) {
    return glm::rotate(glm::mat4(1.0f), static_cast<float>(radians(deg)), axis);
}
glm::mat4 rot_x(double deg) { return rot(deg, {1, 0, 0}); }
glm::mat4 rot_y(double deg) { return rot(deg, {0, 1, 0}); }
glm::mat4 rot_z(double deg) { return rot(deg, {0, 0, 1}); }

// Python-style modulo (result has the sign of the divisor).
double pymod(double a, double b) { return a - b * std::floor(a / b); }
double angle_diff(double a, double b) { return pymod(b - a + 180, 360) - 180; }

// Add a textured box of `size` centred at `center` (in the frame of matrix m).
void add_box(MeshData& mesh, const glm::mat4& m, glm::vec3 size, glm::vec3 center, int tile, float brightness = 1.0f) {
    auto uv = tile_uv(tile);
    for (const FaceDef& f : face_defs()) {
        std::array<glm::vec3, 4> pts;
        for (int c = 0; c < 4; ++c) {
            glm::vec3 local = center + (glm::vec3(f.corners[c]) - 0.5f) * size;
            pts[c] = glm::vec3(m * glm::vec4(local, 1.0f));
        }
        mesh.quad_points(pts, uv, f.light * brightness);
    }
}

const double LEGS[4][2] = {{-0.22, 0.4}, {0.22, 0.4}, {-0.22, -0.4}, {0.22, -0.4}};

}  // namespace

// --- Cow --------------------------------------------------------------------------------

Cow::Cow(World& world, double x, double y, double z, Rng& rng)
    : body(world, x, y, z, COW_HALF_WIDTH, COW_HEIGHT) {
    yaw = rng.uniform(0, 360);
    target_yaw = yaw;
    state_timer = rng.uniform(1, 4);
}

void Cow::hurt(int amount, double from_x, double from_z) {
    if (!alive()) return;
    health -= amount;
    double dx = body.x - from_x, dz = body.z - from_z;
    double d = std::hypot(dx, dz);
    if (d == 0) d = 1.0;
    knock_x = dx / d * 7;
    knock_z = dz / d * 7;
    if (body.on_ground) body.vy = 5.5;
    flash = 0.3;
    state = State::Panic;
    state_timer = 5.0;
    target_yaw = degrees(std::atan2(dx, dz));
    if (!alive()) dying = 0.0001;
}

std::optional<float> Cow::ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const {
    const float h = 0.6f;
    glm::vec3 lo(body.x - h, body.y, body.z - h);
    glm::vec3 hi(body.x + h, body.y + 1.5, body.z + h);
    float t0 = 0.0f, t1 = max_dist;
    for (int a = 0; a < 3; ++a) {
        float o = origin[a], d = direction[a];
        if (std::abs(d) < 1e-9f) {
            if (o < lo[a] || o > hi[a]) return std::nullopt;
            continue;
        }
        float t_a = (lo[a] - o) / d, t_c = (hi[a] - o) / d;
        if (t_a > t_c) std::swap(t_a, t_c);
        t0 = std::max(t0, t_a);
        t1 = std::min(t1, t_c);
        if (t0 > t1) return std::nullopt;
    }
    return t0;
}

bool Cow::tick(double dt, double player_x, double player_z, Rng& rng) {
    if (dying) {
        dying += dt;
        roll = std::min(90.0, dying * 200);
        return dying > 0.7;
    }

    state_timer -= dt;
    if (state_timer <= 0) {
        if (state == State::Panic || state == State::Walk) {
            state = State::Idle;
            state_timer = rng.uniform(2, 6);
            graze = rng.random() < 0.5;
        } else {
            state = State::Walk;
            state_timer = rng.uniform(2, 5);
            target_yaw = rng.uniform(0, 360);
        }
    }
    if (state == State::Panic && rng.random() < dt * 1.2) {
        double away = degrees(std::atan2(body.x - player_x, body.z - player_z));
        target_yaw = away + rng.uniform(-60, 60);
    }

    double turn = state == State::Panic ? 360 : 120;
    double diff = angle_diff(yaw, target_yaw);
    yaw += std::clamp(diff, -turn * dt, turn * dt);

    double want = state == State::Idle ? 0.0 : state == State::Walk ? 1.3 : 4.2;
    if (std::abs(diff) > 60) want *= 0.3;
    speed += (want - speed) * std::min(1.0, dt * 6);

    double rad = radians(yaw);
    Body& b = body;
    b.vx = std::sin(rad) * speed + knock_x;
    b.vz = std::cos(rad) * speed + knock_z;
    double decay = std::max(0.0, 1 - dt * 6);
    knock_x *= decay;
    knock_z *= decay;

    b.check_water();
    b.apply_gravity(dt);
    if (b.in_water) b.vy = std::max(b.vy, 2.0);
    HitAxes hit = b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    if ((hit.x || hit.z) && speed > 0.3) {
        if (b.on_ground && blocked < 3) {
            b.vy = 8.2;  // hop up one block
            ++blocked;
        } else if (blocked >= 3) {
            target_yaw = yaw + rng.choice({-1, 1}) * rng.uniform(90, 180);
            blocked = 0;
        }
    } else if (b.on_ground) {
        blocked = 0;
    }

    walk_phase += speed * dt * 5;
    double target_pitch = (state == State::Idle && graze) ? 40 : 0;
    head_pitch += (target_pitch - head_pitch) * std::min(1.0, dt * 4);
    flash = std::max(0.0, flash - dt);
    return false;
}

std::vector<ModelPart> Cow::parts() const {
    glm::mat4 root = translate(body.x, body.y, body.z) * rot_y(yaw) * rot_z(roll);
    int hurt = (flash > 0 || dying) ? 1 : 0;
    std::vector<ModelPart> out;
    out.push_back({MODEL_COW_BODY + hurt, root});
    double swing = std::sin(walk_phase) * std::min(1.0, speed) * 35;
    for (int i = 0; i < 4; ++i) {
        double s = (i == 0 || i == 3) ? swing : -swing;
        out.push_back({MODEL_COW_LEG + hurt, root * translate(LEGS[i][0], 0.75, LEGS[i][1]) * rot_x(s)});
    }
    out.push_back({MODEL_COW_HEAD + hurt, root * translate(0, 1.2, 0.58) * rot_x(head_pitch)});
    return out;
}

// --- Dropped items ------------------------------------------------------------------------

ItemDrop::ItemDrop(World& world, ItemId item_, double x, double y, double z, Rng& rng)
    : item(item_), body(world, x, y, z, 0.12, 0.25) {
    body.vx = rng.uniform(-1.5, 1.5);
    body.vz = rng.uniform(-1.5, 1.5);
    body.vy = rng.uniform(3, 5);
}

void ItemDrop::tick(double dt) {
    Body& b = body;
    age += dt;
    b.check_water();
    b.apply_gravity(dt);
    if (b.in_water) b.vy = std::max(b.vy, 1.0);
    b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    if (b.on_ground) {
        b.vx *= std::max(0.0, 1 - dt * 8);
        b.vz *= std::max(0.0, 1 - dt * 8);
    }
}

std::vector<ModelPart> ItemDrop::parts() const {
    double bob = 0.2 + std::sin(age * 3) * 0.06;
    return {{ITEM_MODEL_BASE + item, translate(body.x, body.y + bob, body.z) * rot_y(age * 90)}};
}

// --- Player ---------------------------------------------------------------------------------

std::vector<ModelPart> player_parts(const PlayerPose& pose) {
    glm::mat4 root = translate(pose.x, pose.y - (pose.sneaking ? 0.1 : 0), pose.z) * rot_y(pose.yaw);
    double swing = std::sin(pose.walk_phase) * pose.walk_amount * 40;
    double lean = pose.sneaking ? 25 : 0;
    glm::mat4 hips = root * translate(0, 0.675, 0);
    glm::mat4 torso = hips * rot_x(lean);
    std::vector<ModelPart> out;
    out.push_back({MODEL_PLAYER_LEG, root * translate(-0.125, 0.675, pose.sneaking ? -0.1 : 0) * rot_x(swing)});
    out.push_back({MODEL_PLAYER_LEG, root * translate(0.125, 0.675, pose.sneaking ? -0.1 : 0) * rot_x(-swing)});
    out.push_back({MODEL_PLAYER_BODY, torso});
    out.push_back({MODEL_PLAYER_ARM, torso * translate(-0.375, 0.615, 0) * rot_x(-swing)});
    out.push_back({MODEL_PLAYER_ARM, torso * translate(0.375, 0.615, 0) * rot_x(swing)});
    out.push_back({MODEL_PLAYER_HEAD, torso * translate(0, 0.675, 0) * rot_x(std::clamp(pose.pitch, -80.0, 80.0) - lean)});
    return out;
}

// --- Models ---------------------------------------------------------------------------------

std::map<int, MeshData> build_models() {
    std::map<int, MeshData> models;
    const glm::mat4 ident(1.0f);
    for (int hurt = 0; hurt < 2; ++hurt) {
        int hide = hurt ? T_SNOUT : T_COW_HIDE;
        MeshData& body = models[MODEL_COW_BODY + hurt];
        add_box(body, ident, {0.75f, 0.62f, 1.15f}, {0, 1.06f, 0}, hide);   // body
        add_box(body, ident, {0.08f, 0.5f, 0.08f}, {0, 0.95f, -0.6f}, hide);  // tail
        MeshData& leg = models[MODEL_COW_LEG + hurt];
        add_box(leg, ident, {0.25f, 0.63f, 0.25f}, {0, -0.315f, 0}, hide);
        add_box(leg, ident, {0.26f, 0.12f, 0.26f}, {0, -0.69f, 0}, hurt ? T_SNOUT : T_HOOF);
        MeshData& head = models[MODEL_COW_HEAD + hurt];
        add_box(head, ident, {0.5f, 0.5f, 0.38f}, {0, 0, 0.19f}, hurt ? T_SNOUT : T_COW_HEAD);
        add_box(head, ident, {0.32f, 0.2f, 0.08f}, {0, -0.13f, 0.41f}, T_SNOUT);
        for (float side : {-1.0f, 1.0f}) {
            add_box(head, ident, {0.07f, 0.07f, 0.02f}, {side * 0.15f, 0.08f, 0.385f}, T_BLACK);  // eyes
            add_box(head, ident, {0.08f, 0.14f, 0.08f}, {side * 0.29f, 0.25f, 0.12f}, T_HORN);    // horns
        }
    }
    // Player parts, each around its joint: head at the neck, body at the hips,
    // arms at the shoulders and legs at the hips. The face looks along +z.
    MeshData& head = models[MODEL_PLAYER_HEAD];
    add_box(head, ident, {0.45f, 0.45f, 0.45f}, {0, 0.225f, 0}, T_SKIN);
    add_box(head, ident, {0.47f, 0.1f, 0.47f}, {0, 0.42f, 0}, T_HAIR);
    add_box(head, ident, {0.47f, 0.3f, 0.07f}, {0, 0.27f, -0.205f}, T_HAIR);
    for (float side : {-1.0f, 1.0f}) {
        add_box(head, ident, {0.09f, 0.05f, 0.02f}, {side * 0.1f, 0.24f, 0.228f}, T_EYE);
        add_box(head, ident, {0.045f, 0.05f, 0.02f}, {side * 0.078f, 0.24f, 0.232f}, T_BLACK);
    }
    add_box(head, ident, {0.14f, 0.03f, 0.02f}, {0, 0.1f, 0.228f}, T_HAIR);  // mouth
    MeshData& torso = models[MODEL_PLAYER_BODY];
    add_box(torso, ident, {0.5f, 0.675f, 0.25f}, {0, 0.3375f, 0}, T_SHIRT);
    MeshData& arm = models[MODEL_PLAYER_ARM];
    add_box(arm, ident, {0.25f, 0.31f, 0.25f}, {0, -0.095f, 0}, T_SHIRT);
    add_box(arm, ident, {0.24f, 0.365f, 0.24f}, {0, -0.4325f, 0}, T_SKIN);
    MeshData& leg = models[MODEL_PLAYER_LEG];
    add_box(leg, ident, {0.25f, 0.55f, 0.25f}, {0, -0.275f, 0}, T_PANTS);
    add_box(leg, ident, {0.26f, 0.125f, 0.27f}, {0, -0.6125f, 0.005f}, T_SHOE);

    for (ItemId item : all_items()) {
        glm::vec3 size = is_block(item) && !is_torch(item) ? glm::vec3(0.25f) : glm::vec3(0.4f, 0.4f, 0.03f);
        add_box(models[ITEM_MODEL_BASE + item], ident, size, {0, 0, 0}, icon_tile(item));
    }
    return models;
}

int drop_beef_count(Rng& rng) { return rng.randint(1, 3); }
