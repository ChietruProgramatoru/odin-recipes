#include "mobs.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "animals.h"
#include "monsters.h"
#include "textures.h"
#include "vehicles.h"

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

}  // namespace

void add_box(MeshData& mesh, const glm::mat4& m, glm::vec3 size, glm::vec3 center, int tile, float brightness) {
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

std::optional<float> ray_box(glm::vec3 origin, glm::vec3 direction, float max_dist, glm::vec3 lo, glm::vec3 hi) {
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

// --- Dropped items ------------------------------------------------------------------------

ItemDrop::ItemDrop(World& world, Stack stack_, double x, double y, double z, Rng& rng)
    : stack(stack_), body(world, x, y, z, 0.12, 0.25) {
    body.vx = rng.uniform(-1.5, 1.5);
    body.vz = rng.uniform(-1.5, 1.5);
    body.vy = rng.uniform(3, 5);
}

void ItemDrop::tick(double dt) {
    Body& b = body;
    age += dt;
    b.check_water();
    b.vx += b.current.x * CURRENT_PUSH * dt;
    b.vz += b.current.z * CURRENT_PUSH * dt;
    b.apply_gravity(dt);
    if (b.in_water) b.vy = std::max(b.vy, 1.0);
    if (b.in_water && !b.on_ground) {  // water slows drifting items, so currents carry them at a steady speed
        b.vx *= std::max(0.0, 1 - dt * 4);
        b.vz *= std::max(0.0, 1 - dt * 4);
    }
    b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    if (b.on_ground) {
        b.vx *= std::max(0.0, 1 - dt * 8);
        b.vz *= std::max(0.0, 1 - dt * 8);
    }
}

std::vector<ModelPart> ItemDrop::parts() const {
    double bob = 0.2 + std::sin(age * 3) * 0.06;
    return {{ITEM_MODEL_BASE + stack.item, translate(body.x, body.y + bob, body.z) * rot_y(age * 90)}};
}

// --- Player ---------------------------------------------------------------------------------

std::array<std::optional<Material>, 4> worn_armor(const Inventory& inv) {
    std::array<std::optional<Material>, 4> out{};
    for (int p = 0; p < Inventory::ARMOR; ++p)
        if (const auto& piece = inv.armor[p]; piece && armor_slot(piece->item) == p)
            out[p] = static_cast<Material>((piece->item - LEATHER_HELMET) / 4);
    return out;
}

std::vector<ModelPart> player_parts(const PlayerPose& pose) {
    glm::mat4 root = translate(pose.x, pose.y - (pose.sneaking ? 0.1 : 0), pose.z) * rot_y(pose.yaw);
    double swing = std::sin(pose.walk_phase) * pose.walk_amount * 40;
    double lean = pose.sneaking ? 25 : 0;
    glm::mat4 hips = root * translate(0, 0.675, 0);
    glm::mat4 torso = hips * rot_x(lean);
    // Sitting (Beta's riding pose): legs straight out in front, a little apart, and arms forward.
    const double leg_pitch = pose.sitting ? -80 : 0, arm_pitch = pose.sitting ? -36 : 0, spread = pose.sitting ? 18 : 0;
    const glm::mat4 legs[2] = {root * translate(-0.125, 0.675, pose.sneaking ? -0.1 : 0) * rot_y(-spread) * rot_x(swing + leg_pitch),
                               root * translate(0.125, 0.675, pose.sneaking ? -0.1 : 0) * rot_y(spread) * rot_x(-swing + leg_pitch)};
    const glm::mat4 arms[2] = {torso * translate(-0.375, 0.615, 0) * rot_x(-swing + arm_pitch),
                               torso * translate(0.375, 0.615, 0) * rot_x(swing + arm_pitch)};
    const glm::mat4 head = torso * translate(0, 0.675, 0) * rot_x(std::clamp(pose.pitch, -80.0, 80.0) - lean);
    std::vector<ModelPart> out;
    for (const glm::mat4& leg : legs) out.push_back({MODEL_PLAYER_LEG, leg});
    out.push_back({MODEL_PLAYER_BODY, torso});
    for (const glm::mat4& arm : arms) out.push_back({MODEL_PLAYER_ARM, arm});
    out.push_back({MODEL_PLAYER_HEAD, head});

    // Armor goes over the same joints.
    auto material = [&](int piece) { return static_cast<int>(*pose.armor[piece]); };
    if (pose.armor[0]) out.push_back({MODEL_ARMOR_HELMET + material(0), head});
    if (pose.armor[1]) {
        out.push_back({MODEL_ARMOR_CHEST + material(1), torso});
        for (const glm::mat4& arm : arms) out.push_back({MODEL_ARMOR_SLEEVE + material(1), arm});
    }
    if (pose.armor[2]) {
        out.push_back({MODEL_ARMOR_WAIST + material(2), torso});
        for (const glm::mat4& leg : legs) out.push_back({MODEL_ARMOR_LEG + material(2), leg});
    }
    if (pose.armor[3])
        for (const glm::mat4& leg : legs) out.push_back({MODEL_ARMOR_BOOT + material(3), leg});
    return out;
}

// --- Arrows -----------------------------------------------------------------------------------

void Arrow::aim() {
    if (glm::length(vel) < 1e-6) return;
    yaw = degrees(std::atan2(vel.x, vel.z));
    pitch = degrees(std::atan2(-vel.y, std::hypot(vel.x, vel.z)));
}

std::optional<ArrowTarget> Arrow::tick(double dt, World& world, const std::vector<ArrowTarget>& targets) {
    age += dt;
    if (stuck) {
        if (is_solid(world.get_block(stuck_in.x, stuck_in.y, stuck_in.z))) {
            stuck_time += dt;
            return std::nullopt;
        }
        // The block it was stuck in is gone: drop out of the hole.
        stuck = false;
        stuck_time = 0;
        vel = {0, -1, 0};
    }
    const glm::dvec3 step = vel * dt;
    const double length = glm::length(step);
    if (length > 1e-9) {
        const glm::vec3 origin(pos), dir(step / length);
        auto block = world.raycast(origin, dir, static_cast<float>(length), is_solid);
        float nearest = block ? block->distance : static_cast<float>(length);
        std::optional<ArrowTarget> hit;
        for (const ArrowTarget& target : targets) {
            if (target.who && target.who == shooter) continue;
            auto t = ray_box(origin, dir, nearest, target.lo, target.hi);
            if (t && *t <= nearest) {
                nearest = *t;
                hit = target;
            }
        }
        if (hit) return hit;
        if (block) {
            // The tip goes a little way into the block, and there it stays.
            pos += glm::dvec3(dir) * (static_cast<double>(block->distance) + 0.15);
            stuck = true;
            stuck_in = block->block;
            vel = {0, 0, 0};
            return std::nullopt;
        }
        pos += step;
    }
    const glm::ivec3 cell(static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y)),
                          static_cast<int>(std::floor(pos.z)));
    const bool in_water = world.get_block(cell.x, cell.y, cell.z) == WATER;
    vel *= std::pow(in_water ? 0.8 : 0.99, dt * 20);  // Beta's drag per tick
    vel.y -= GRAVITY * dt;
    aim();
    return std::nullopt;
}

std::vector<ModelPart> Arrow::parts() const {
    if (item != ARROW)
        return {{ITEM_MODEL_BASE + item, translate(pos.x, pos.y, pos.z) * rot_y(yaw) * glm::scale(glm::mat4(1.0f), glm::vec3(0.6f))}};
    return {{MODEL_ARROW, translate(pos.x, pos.y, pos.z) * rot_y(yaw) * rot_x(pitch)}};
}

// --- Models ---------------------------------------------------------------------------------

std::map<int, MeshData> build_models() {
    std::map<int, MeshData> models;
    const glm::mat4 ident(1.0f);
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

    // Armor, a little bigger than the parts it covers. The helmet leaves the face open.
    for (int m = 0; m < 5; ++m) {
        const int tile = T_ARMOR_LEATHER + m;
        MeshData& helmet = models[MODEL_ARMOR_HELMET + m];
        add_box(helmet, ident, {0.53f, 0.12f, 0.53f}, {0, 0.43f, 0}, tile);
        add_box(helmet, ident, {0.53f, 0.3f, 0.06f}, {0, 0.24f, -0.245f}, tile);
        for (float side : {-1.0f, 1.0f}) add_box(helmet, ident, {0.06f, 0.3f, 0.47f}, {side * 0.26f, 0.24f, -0.03f}, tile);
        add_box(models[MODEL_ARMOR_CHEST + m], ident, {0.56f, 0.5f, 0.31f}, {0, 0.42f, 0}, tile);
        add_box(models[MODEL_ARMOR_SLEEVE + m], ident, {0.31f, 0.34f, 0.31f}, {0, -0.08f, 0}, tile);
        add_box(models[MODEL_ARMOR_WAIST + m], ident, {0.54f, 0.2f, 0.29f}, {0, 0.1f, 0}, tile);
        add_box(models[MODEL_ARMOR_LEG + m], ident, {0.29f, 0.45f, 0.29f}, {0, -0.225f, 0}, tile);
        add_box(models[MODEL_ARMOR_BOOT + m], ident, {0.3f, 0.22f, 0.31f}, {0, -0.575f, 0.005f}, tile);
    }

    // The arrow points along +z with its tip at the origin.
    MeshData& arrow = models[MODEL_ARROW];
    add_box(arrow, ident, {0.04f, 0.04f, 0.55f}, {0, 0, -0.3f}, T_PLANKS);
    add_box(arrow, ident, {0.08f, 0.08f, 0.1f}, {0, 0, -0.04f}, T_SHOE);
    add_box(arrow, ident, {0.16f, 0.015f, 0.12f}, {0, 0, -0.5f}, T_WHITE);
    add_box(arrow, ident, {0.015f, 0.16f, 0.12f}, {0, 0, -0.5f}, T_WHITE);

    add_monster_models(models);
    add_animal_models(models);
    add_vehicle_models(models);

    for (ItemId item : all_items()) {
        glm::vec3 size = looks_like_block(item) ? glm::vec3(0.25f) : glm::vec3(0.4f, 0.4f, 0.03f);
        add_box(models[ITEM_MODEL_BASE + item], ident, size, {0, 0, 0}, icon_tile(item));
    }
    return models;
}

