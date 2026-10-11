#include "monsters.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "blocks.h"
#include "icons.h"
#include "items.h"
#include "nature.h"
#include "survival.h"
#include "textures.h"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double TICK = 0.05;  // seconds per game tick
double radians(double deg) { return deg * PI / 180.0; }
double degrees(double rad) { return rad * 180.0 / PI; }
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
double pymod(double a, double b) { return a - b * std::floor(a / b); }
double angle_diff(double a, double b) { return pymod(b - a + 180, 360) - 180; }

glm::mat4 translate(double x, double y, double z) {
    return glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}
glm::mat4 scale(double x, double y, double z) {
    return glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}
glm::mat4 rot(double deg, glm::vec3 axis) { return glm::rotate(glm::mat4(1.0f), static_cast<float>(radians(deg)), axis); }
glm::mat4 rot_x(double deg) { return rot(deg, {1, 0, 0}); }
glm::mat4 rot_y(double deg) { return rot(deg, {0, 1, 0}); }
glm::mat4 rot_z(double deg) { return rot(deg, {0, 0, 1}); }

// Seconds the death animation lasts (Beta: 20 ticks), and how far a rider sits above its mount's feet.
constexpr double DEATH_SECONDS = 1.0;
constexpr double RIDER_OFFSET = 0.35;

// Model ids of a biped (zombie or skeleton): head, body, arm, leg.
int biped_base(MobKind kind) { return kind == MobKind::Zombie ? MODEL_ZOMBIE_HEAD : MODEL_SKELETON_HEAD; }

}  // namespace

int light_at(World& world, int x, int y, int z, int sky_darkness) {
    return std::max(world.sky_light(x, y, z) - sky_darkness, world.block_light(x, y, z));
}

// --- Monster ----------------------------------------------------------------------------------

Monster::Monster(World& world, MobKind k, double x, double y, double z, Rng& rng, int sz)
    : kind(k), size(k == MobKind::Slime ? std::clamp(sz, 1, 4) : 1),
      body(world, x, y, z, mob_info(k, size).half_width, mob_info(k, size).height) {
    yaw = rng.uniform(0, 360);
    target_yaw = yaw;
    health = info().health;
    wander_timer_ = rng.uniform(0.5, 3);
    jump_delay_ = rng.below(20) + 10;
    fall_peak_ = y;
}

double Monster::eye_height() const { return body.height * 0.85; }

int Monster::hurt(int amount, double from_x, double from_z, bool knock) {
    if (!alive() || amount <= 0) return 0;
    bool fresh = false;
    const int dmg = cooldown.take(amount, fresh);
    if (dmg <= 0) return 0;
    health -= dmg;
    if (fresh) {
        flash = 0.5;
        if (knock && !mount) {
            double dx = body.x - from_x, dz = body.z - from_z;
            double d = std::hypot(dx, dz);
            if (d < 1e-6) {
                dx = 0;
                dz = d = 1;
            }
            knock_x = dx / d * 7;
            knock_z = dz / d * 7;
            if (body.on_ground) body.vy = 5.5;
        }
    }
    if (!alive()) {
        health = 0;
        dying = 1e-4;
        if (rider) rider->mount = nullptr;
        if (mount) mount->rider = nullptr;
        rider = mount = nullptr;
    }
    return dmg;
}

ArrowTarget Monster::target(int kind_tag, int index) const {
    const float h = static_cast<float>(body.half);
    return {glm::vec3(body.x - h, body.y, body.z - h), glm::vec3(body.x + h, body.y + body.height, body.z + h), kind_tag, index,
            this};
}

std::optional<float> Monster::ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const {
    const ArrowTarget t = target(0, 0);
    return ray_box(origin, direction, max_dist, t.lo, t.hi);
}

void Monster::face(double x, double z) { target_yaw = degrees(std::atan2(x - body.x, z - body.z)); }

bool Monster::sees(World& world, const MonsterSenses& s) const {
    const glm::vec3 eye(body.x, body.y + eye_height(), body.z);
    const glm::vec3 to(s.player.x, s.player.y + s.player_height * 0.9, s.player.z);
    const glm::vec3 d = to - eye;
    const float len = glm::length(d);
    if (len < 1e-4f) return true;
    return !world.raycast(eye, d / len, len, is_solid);
}

void Monster::per_tick(World& world, const MonsterSenses& s, Rng& rng, MonsterActions& out) {
    const MobInfo mi = info();
    const int cx = ifloor(body.x), cz = ifloor(body.z), cy = ifloor(body.y + body.height * 0.5);
    const int light = light_at(world, cx, cy, cz, s.sky_darkness);
    const bool sky_above = world.sky_light(cx, ifloor(body.y + eye_height()), cz) == MAX_LIGHT;

    // Fire: daylight sets zombies and skeletons alight; lava and fire blocks burn anything; water
    // and rain put it out.
    if (mi.burns_in_daylight && catches_fire_in_daylight(light, sky_above, s.sky_darkness, rng))
        burning = std::max(burning, DAYLIGHT_BURN_SECONDS);
    for (int x = ifloor(body.x - body.half); x <= ifloor(body.x + body.half - 1e-6); ++x)
        for (int y = ifloor(body.y); y <= ifloor(body.y + body.height - 1e-6); ++y)
            for (int z = ifloor(body.z - body.half); z <= ifloor(body.z + body.half - 1e-6); ++z) {
                const BlockId b = world.get_block(x, y, z);
                if (b == LAVA) {
                    hurt(4, body.x, body.z, false);
                    burning = std::max(burning, 30.0);
                } else if (b == FIRE) {
                    hurt(1, body.x, body.z, false);
                    burning = std::max(burning, 8.0);
                }
            }
    if (touches_cactus(world, body)) hurt(1, body.x, body.z, false);
    if (body.in_water || (s.raining && sky_above)) burning = 0;
    if (!alive()) return;

    // Who to go for.
    const double dx = s.player.x - body.x, dz = s.player.z - body.z, dy = s.player.y - body.y;
    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    std::optional<bool> seen;
    auto can_see = [&] {
        if (!seen) seen = sees(world, s);
        return *seen;
    };
    if (!s.player_targetable || dist > TARGET_RANGE * 1.5) {
        targeting = false;
    } else if (kind == MobKind::Spider) {
        // Spiders only hunt in the dark; in daylight they lose interest now and then.
        if (spider_hostile(light)) {
            if (dist < TARGET_RANGE) targeting = true;
        } else if (targeting && rng.below(100) == 0) {
            targeting = false;
        }
    } else if (kind == MobKind::Slime) {
        targeting = dist < TARGET_RANGE;
    } else if (!targeting && dist < TARGET_RANGE && can_see()) {
        targeting = true;
    }

    // Wandering about when not after the player.
    wander_timer_ -= TICK;
    if (wander_timer_ <= 0) {
        wandering_ = !wandering_ && rng.random() < 0.6;
        wander_timer_ = wandering_ ? rng.uniform(2, 5) : rng.uniform(2, 6);
        if (wandering_) target_yaw = rng.uniform(0, 360);
    }
    want_speed_ = targeting ? mi.speed : wandering_ ? mi.speed * 0.6 : 0.0;
    if (targeting) face(s.player.x, s.player.z);

    const bool level = s.player.y < body.y + body.height && s.player.y + s.player_height > body.y;
    switch (kind) {
        case MobKind::Zombie:
        case MobKind::Spider:
            if (targeting && attack_timer <= 0 && std::hypot(dx, dz) < MELEE_RANGE + body.half - 0.3 && level && can_see()) {
                out.melee = mi.attack;
                attack_timer = MELEE_RELOAD;
            }
            // Spiders leap at the player from a few blocks away.
            if (kind == MobKind::Spider && targeting && dist > 2 && dist < 6 && body.on_ground && rng.below(10) == 0) {
                const double h = std::max(1e-6, std::hypot(dx, dz));
                knock_x = dx / h * 6.4;
                knock_z = dz / h * 6.4;
                body.vy = 8.0;
            }
            break;
        case MobKind::Skeleton:
            if (targeting && dist < SKELETON_RANGE && can_see()) {
                want_speed_ = 0;  // stand and shoot
                if (attack_timer <= 0) {
                    attack_timer = SKELETON_RELOAD;
                    glm::dvec3 from(body.x, body.y + 1.4, body.z);
                    glm::dvec3 to(s.player.x, s.player.y + s.player_height * 0.7, s.player.z);
                    glm::dvec3 d = to - from;
                    const double horiz = std::hypot(d.x, d.z);
                    d.y += horiz * 0.2;  // Beta aims a little high to make up for the arrow dropping
                    d = glm::normalize(d);
                    d += glm::dvec3(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)) * (0.0075 * 12);
                    d = glm::normalize(d);
                    out.arrow_from = from + glm::dvec3(d.x, 0, d.z) * 0.4;
                    out.arrow_vel = d * SKELETON_ARROW_SPEED;
                }
            }
            break;
        case MobKind::Creeper: {
            const bool close = fuse <= 0 ? dist < CREEPER_IGNITE_RANGE : dist < CREEPER_DEFUSE_RANGE;
            if (targeting && close && can_see()) {
                if (fuse == 0) out.hiss = true;
                ++fuse;
                want_speed_ = 0;  // stands still while it hisses
                if (fuse >= CREEPER_FUSE_TICKS) out.explode = true;
            } else {
                fuse = std::max(0, fuse - 1);
            }
            break;
        }
        case MobKind::Slime:
            // Slimes hop: a jump every 10-29 ticks (three times as often when after the player),
            // sliding along only while in the air.
            if (body.on_ground) {
                speed = 0;
                if (--jump_delay_ <= 0) {
                    jump_delay_ = rng.below(20) + 10;
                    if (targeting) jump_delay_ /= 3;
                    if (!targeting) target_yaw = rng.uniform(0, 360);
                    body.vy = 6.0 + size * 0.5;
                    speed = mi.speed;
                    yaw = target_yaw;
                }
            }
            want_speed_ = body.on_ground ? 0 : speed;
            if (targeting && mi.attack > 0 && attack_timer <= 0 && can_see()) {
                const glm::dvec3 c(body.x, body.y + body.height / 2, body.z), p(s.player.x, s.player.y + 0.9, s.player.z);
                if (glm::length(c - p) < 0.6 * size + 0.6) {
                    out.melee = mi.attack;
                    attack_timer = 0.5;
                }
            }
            break;
    }
}

bool Monster::tick(double dt, World& world, const MonsterSenses& s, Rng& rng, MonsterActions& out) {
    age += dt;
    flash = std::max(0.0, flash - dt);
    cooldown.tick(dt);
    attack_timer -= dt;
    if (dying > 0) {
        dying += dt;
        return dying > DEATH_SECONDS;
    }
    tick_time_ += dt;
    while (tick_time_ >= TICK && alive() && !out.explode) {
        tick_time_ -= TICK;
        per_tick(world, s, rng, out);
    }
    if (!alive() || out.explode) return false;

    if (burning > 0) {
        burning -= dt;
        fire_timer_ -= dt;
        if (fire_timer_ <= 0) {
            fire_timer_ = 1.0;
            hurt(1, body.x, body.z, false);
            if (!alive()) return false;
        }
    } else {
        fire_timer_ = 0;
    }

    if (mount) {
        // Riding: sit on the mount, turning to look where it wants.
        body.x = mount->body.x;
        body.y = mount->body.y + RIDER_OFFSET;
        body.z = mount->body.z;
        body.vx = body.vy = body.vz = 0;
        fall_peak_ = body.y;
        yaw += std::clamp(angle_diff(yaw, target_yaw), -360 * dt, 360 * dt);
        return false;
    }

    const double diff = angle_diff(yaw, target_yaw);
    const double turn = 300;
    yaw += std::clamp(diff, -turn * dt, turn * dt);
    if (kind != MobKind::Slime) {
        double want = want_speed_;
        if (std::abs(diff) > 60) want *= 0.3;
        speed += (want - speed) * std::min(1.0, dt * 6);
    }

    Body& b = body;
    const double rad = radians(yaw);
    b.vx = std::sin(rad) * speed + knock_x;
    b.vz = std::cos(rad) * speed + knock_z;
    const double decay = std::max(0.0, 1 - dt * 6);
    knock_x *= decay;
    knock_z *= decay;
    b.check_water();
    b.vx += b.current.x * CURRENT_SPEED;
    b.vz += b.current.z * CURRENT_SPEED;
    b.apply_gravity(dt);
    if (b.in_liquid()) b.vy = std::max(b.vy, 2.0);
    const HitAxes hit = b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    bool climbing = false;
    if ((hit.x || hit.z) && want_speed_ > 0.3) {
        if (kind == MobKind::Spider) {
            b.vy = 3.0;  // spiders climb straight up walls
            climbing = true;
        } else if (b.on_ground && blocked_ < 3) {
            b.vy = 8.2;  // hop up one block
            ++blocked_;
        } else if (blocked_ >= 3 && !targeting) {
            target_yaw = yaw + rng.choice({-1, 1}) * rng.uniform(90, 180);
            blocked_ = 0;
        }
    } else if (b.on_ground) {
        blocked_ = 0;
    }

    // Monsters take fall damage like the player (not spiders on a wall or anything in water).
    if (b.in_water || climbing) {
        fall_peak_ = b.y;
    } else if (!b.on_ground) {
        fall_peak_ = std::max(fall_peak_, b.y);
    } else {
        if (int dmg = fall_damage(fall_peak_ - b.y)) hurt(dmg, b.x, b.z, false);
        fall_peak_ = b.y;
    }
    walk_phase += speed * dt * 5;
    return false;
}

std::vector<ModelPart> Monster::parts() const {
    const double roll = dying > 0 ? std::min(1.0, std::sqrt(dying * 1.6)) * 90 : 0;
    glm::mat4 root = translate(body.x, body.y, body.z) * rot_y(yaw) * rot_z(roll);
    if (kind == MobKind::Creeper && fuse > 0) {
        // Beta's swelling: wider and a little taller as the fuse burns, with a slight wobble.
        double f = fuse_progress();
        const double wobble = 1 + std::sin(f * 100) * f * 0.01;
        f = f * f * f * f;
        root = root * scale((1 + f * 0.4) * wobble, (1 + f * 0.1) / wobble, (1 + f * 0.4) * wobble);
    }
    const double walk = mount ? 0.0 : std::min(1.0, speed / std::max(0.1, info().speed));
    std::vector<ModelPart> out = monster_model(kind, size, root, walk_phase, walk);
    glm::vec4 overlay(0);
    if (flash > 0 || dying > 0) {
        overlay = {1, 0, 0, 0.45f};
    } else if (kind == MobKind::Creeper && fuse > 0) {
        // Flashing white faster and faster as it is about to blow.
        const double f = fuse_progress();
        if (static_cast<int>(f * 10) % 2 == 1) overlay = {1, 1, 1, static_cast<float>(0.25 + f * 0.5)};
    }
    if (overlay.a == 0 && powered) overlay = {0.45f, 0.75f, 1.0f, 0.3f};
    for (ModelPart& p : out) p.overlay = overlay;
    if (burning > 0 && dying <= 0) {
        const double w = body.half * 2 + 0.2, h = body.height + 0.2;
        out.push_back({MODEL_FLAMES, translate(body.x, body.y - 0.05, body.z) * rot_y(yaw) * scale(w, h, w)});
    }
    return out;
}

std::vector<ModelPart> monster_model(MobKind kind, int size, const glm::mat4& root, double walk_phase, double walk,
                                     double head_pitch) {
    std::vector<ModelPart> out;
    const double swing = std::sin(walk_phase) * walk * 40;
    switch (kind) {
        case MobKind::Zombie:
        case MobKind::Skeleton: {
            const int base = biped_base(kind);
            const double side = kind == MobKind::Zombie ? 0.375 : 0.3125;
            const glm::mat4 hips = root * translate(0, 0.675, 0);
            out.push_back({base + 3, root * translate(-0.125, 0.675, 0) * rot_x(swing)});
            out.push_back({base + 3, root * translate(0.125, 0.675, 0) * rot_x(-swing)});
            out.push_back({base + 1, hips});
            // Arms held out in front, swaying a little (Beta's zombie and skeleton pose).
            const double sway = std::sin(walk_phase * 0.6) * 4;
            const glm::mat4 left = hips * translate(-side, 0.625, 0) * rot_x(-90 + sway);
            const glm::mat4 right = hips * translate(side, 0.625, 0) * rot_x(-90 - sway);
            out.push_back({base + 2, left});
            out.push_back({base + 2, right});
            out.push_back({base, hips * translate(0, 0.675, 0) * rot_x(head_pitch)});
            if (kind == MobKind::Skeleton)  // its bow, in the right hand
                out.push_back({static_cast<int>(ITEM_MODEL_BASE) + static_cast<int>(BOW), right * translate(0, -0.6, 0.06) * rot_x(90) * rot_z(-45) * scale(1.3, 1.3, 1.3)});
            break;
        }
        case MobKind::Creeper: {
            const double legs[4][2] = {{-0.125, 0.25}, {0.125, 0.25}, {-0.125, -0.25}, {0.125, -0.25}};
            for (int i = 0; i < 4; ++i) {
                const double s = (i == 0 || i == 3) ? swing : -swing;
                out.push_back({MODEL_CREEPER_LEG, root * translate(legs[i][0], 0.375, legs[i][1]) * rot_x(s)});
            }
            out.push_back({MODEL_CREEPER_BODY, root * translate(0, 0.375, 0)});
            out.push_back({MODEL_CREEPER_HEAD, root * translate(0, 1.125, 0) * rot_x(head_pitch)});
            break;
        }
        case MobKind::Spider: {
            const glm::mat4 body = root * translate(0, 0.56, 0);
            out.push_back({MODEL_SPIDER_BODY, body});
            out.push_back({MODEL_SPIDER_HEAD, body * translate(0, 0, 0.18) * rot_x(head_pitch)});
            const double spread[4] = {40, 14, -14, -40};
            const double zs[4] = {0.15, 0.05, -0.05, -0.15};
            for (int side = -1; side <= 1; side += 2)
                for (int i = 0; i < 4; ++i) {
                    const double step = std::sin(walk_phase * 1.6 + i * PI / 2 + (side > 0 ? PI : 0)) * walk * 18;
                    const double lift = std::max(0.0, std::cos(walk_phase * 1.6 + i * PI / 2 + (side > 0 ? PI : 0))) * walk * 10;
                    const double turn = (side > 0 ? spread[i] : 180 - spread[i]) + step * side;
                    out.push_back({MODEL_SPIDER_LEG, body * translate(side * 0.18, 0, zs[i]) * rot_y(turn) * rot_z(-32 + lift)});
                }
            break;
        }
        case MobKind::Slime: {
            const double s = 0.5 * size;
            out.push_back({MODEL_SLIME, root * translate(0, s / 2, 0) * scale(s, s, s)});
            break;
        }
    }
    return out;
}

// --- Primed TNT ----------------------------------------------------------------------------------

PrimedTnt::PrimedTnt(World& world, double x, double y, double z, int fuse_ticks, Rng& rng)
    : body(world, x, y, z, 0.49, 0.98), fuse(fuse_ticks * TICK) {
    // Beta gives it a little hop in a random direction.
    const double a = rng.uniform(0, 2 * PI);
    body.vx = -std::sin(a) * 0.4;
    body.vz = -std::cos(a) * 0.4;
    body.vy = 4.0;
}

bool PrimedTnt::tick(double dt) {
    fuse -= dt;
    Body& b = body;
    b.check_water();
    b.vx += b.current.x * CURRENT_PUSH * dt;
    b.vz += b.current.z * CURRENT_PUSH * dt;
    b.vy -= 16.0 * dt;  // Beta: 0.04 per tick per tick
    const double drag = std::pow(0.98, dt * 20);
    b.vx *= drag;
    b.vy *= drag;
    b.vz *= drag;
    b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    if (b.on_ground) {
        const double friction = std::pow(0.7, dt * 20);
        b.vx *= friction;
        b.vz *= friction;
    }
    return fuse <= 0;
}

std::vector<ModelPart> PrimedTnt::parts() const {
    const double ticks = fuse / TICK;
    double s = 1;
    if (ticks < 10) s = 1 + std::clamp(1 - ticks / 10, 0.0, 1.0) * 0.3;
    ModelPart part{MODEL_TNT, translate(body.x, body.y + 0.49, body.z) * scale(s, s, s)};
    if (static_cast<int>(ticks / 5) % 2 == 0) part.overlay = {1, 1, 1, static_cast<float>(std::clamp((1 - ticks / 100) * 0.8, 0.0, 0.8))};
    return {part};
}

// --- Smoke -----------------------------------------------------------------------------------------

bool Puff::tick(double dt) {
    age += dt;
    pos += vel * dt;
    vel *= std::max(0.0, 1 - dt * 3);
    vel.y += 0.6 * dt;
    return age < life;
}

std::vector<ModelPart> Puff::parts() const {
    const double t = age / life;
    const double s = size * (0.6 + t * 0.8) * (1 - t * t);
    return {{MODEL_PUFF, translate(pos.x, pos.y, pos.z) * scale(s, s, s)}};
}

void add_puffs(std::vector<Puff>& puffs, glm::dvec3 center, int count, double spread, double speed, double size, Rng& rng) {
    for (int i = 0; i < count; ++i) {
        glm::dvec3 d(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1));
        if (glm::length(d) < 1e-3) d = {0, 1, 0};
        d = glm::normalize(d);
        Puff p;
        p.pos = center + d * rng.uniform(0, spread);
        p.vel = d * rng.uniform(speed * 0.3, speed);
        p.life = rng.uniform(0.6, 1.4);
        p.size = size * rng.uniform(0.7, 1.3);
        puffs.push_back(p);
    }
}

// --- Models ----------------------------------------------------------------------------------------

void add_monster_models(std::map<int, MeshData>& models) {
    const glm::mat4 ident(1.0f);
    auto eyes = [&](MeshData& m, float y, float z, float spread, glm::vec3 size, int tile) {
        for (float side : {-1.0f, 1.0f}) add_box(m, ident, size, {side * spread, y, z}, tile);
    };

    // Zombie: the player's shape in green and torn blue, arms out in front.
    {
        MeshData& head = models[MODEL_ZOMBIE_HEAD];
        add_box(head, ident, {0.45f, 0.45f, 0.45f}, {0, 0.225f, 0}, T_ZOMBIE_SKIN);
        eyes(head, 0.24f, 0.228f, 0.1f, {0.1f, 0.06f, 0.02f}, T_BLACK);
        add_box(head, ident, {0.2f, 0.04f, 0.02f}, {0, 0.1f, 0.228f}, T_BLACK);  // mouth
        add_box(head, ident, {0.08f, 0.06f, 0.02f}, {0.03f, 0.17f, 0.228f}, T_ZOMBIE_PANTS);  // nose
        add_box(models[MODEL_ZOMBIE_BODY], ident, {0.5f, 0.675f, 0.25f}, {0, 0.3375f, 0}, T_ZOMBIE_SHIRT);
        MeshData& arm = models[MODEL_ZOMBIE_ARM];
        add_box(arm, ident, {0.26f, 0.3f, 0.26f}, {0, -0.1f, 0}, T_ZOMBIE_SHIRT);
        add_box(arm, ident, {0.24f, 0.4f, 0.24f}, {0, -0.45f, 0}, T_ZOMBIE_SKIN);
        MeshData& leg = models[MODEL_ZOMBIE_LEG];
        add_box(leg, ident, {0.25f, 0.55f, 0.25f}, {0, -0.275f, 0}, T_ZOMBIE_PANTS);
        add_box(leg, ident, {0.24f, 0.125f, 0.24f}, {0, -0.6125f, 0}, T_ZOMBIE_SKIN);
    }
    // Skeleton: bone-white, with thin limbs and a ribcage.
    {
        MeshData& head = models[MODEL_SKELETON_HEAD];
        add_box(head, ident, {0.45f, 0.45f, 0.45f}, {0, 0.225f, 0}, T_BONE);
        eyes(head, 0.24f, 0.228f, 0.1f, {0.12f, 0.08f, 0.02f}, T_BLACK);
        add_box(head, ident, {0.06f, 0.06f, 0.02f}, {0, 0.16f, 0.228f}, T_BLACK);
        add_box(head, ident, {0.26f, 0.03f, 0.02f}, {0, 0.08f, 0.228f}, T_BLACK);
        MeshData& body = models[MODEL_SKELETON_BODY];
        add_box(body, ident, {0.12f, 0.675f, 0.12f}, {0, 0.3375f, -0.04f}, T_BONE);  // spine
        for (int i = 0; i < 3; ++i) add_box(body, ident, {0.46f, 0.07f, 0.22f}, {0, 0.6f - i * 0.12f, 0}, T_BONE);
        add_box(body, ident, {0.4f, 0.08f, 0.16f}, {0, 0.04f, 0}, T_BONE);  // hips
        add_box(models[MODEL_SKELETON_ARM], ident, {0.125f, 0.675f, 0.125f}, {0, -0.29f, 0}, T_BONE);
        add_box(models[MODEL_SKELETON_LEG], ident, {0.125f, 0.675f, 0.125f}, {0, -0.3375f, 0}, T_BONE);
    }
    // Creeper: mottled green, four stubby legs and the frowning face.
    {
        MeshData& head = models[MODEL_CREEPER_HEAD];
        add_box(head, ident, {0.5f, 0.5f, 0.5f}, {0, 0.25f, 0}, T_CREEPER);
        eyes(head, 0.32f, 0.252f, 0.125f, {0.125f, 0.125f, 0.01f}, T_BLACK);
        add_box(head, ident, {0.125f, 0.19f, 0.01f}, {0, 0.17f, 0.252f}, T_BLACK);
        eyes(head, 0.095f, 0.252f, 0.0625f + 0.03125f, {0.0625f, 0.125f, 0.01f}, T_BLACK);
        add_box(models[MODEL_CREEPER_BODY], ident, {0.5f, 0.75f, 0.25f}, {0, 0.375f, 0}, T_CREEPER);
        add_box(models[MODEL_CREEPER_LEG], ident, {0.25f, 0.375f, 0.25f}, {0, -0.1875f, 0}, T_CREEPER);
    }
    // Spider: a round dark back, a small middle, a head with red eyes and eight long legs.
    {
        MeshData& head = models[MODEL_SPIDER_HEAD];
        add_box(head, ident, {0.5f, 0.5f, 0.5f}, {0, 0, 0.25f}, T_SPIDER);
        eyes(head, 0.08f, 0.502f, 0.12f, {0.1f, 0.1f, 0.01f}, T_SPIDER_EYE);
        eyes(head, 0.08f, 0.502f, 0.03f, {0.05f, 0.05f, 0.01f}, T_SPIDER_EYE);
        MeshData& body = models[MODEL_SPIDER_BODY];
        add_box(body, ident, {0.375f, 0.375f, 0.375f}, {0, 0, 0}, T_SPIDER);
        add_box(body, ident, {0.75f, 0.56f, 0.75f}, {0, 0.06f, -0.56f}, T_SPIDER);
        add_box(models[MODEL_SPIDER_LEG], ident, {1.0f, 0.125f, 0.125f}, {0.5f, 0, 0}, T_SPIDER);
    }
    // Slime: a green cube with a face, around the origin (scaled to its size).
    {
        MeshData& slime = models[MODEL_SLIME];
        add_box(slime, ident, {1, 1, 1}, {0, 0, 0}, T_SLIME);
        eyes(slime, 0.15f, 0.505f, 0.2f, {0.18f, 0.18f, 0.01f}, T_BLACK);
        add_box(slime, ident, {0.1f, 0.1f, 0.01f}, {0.1f, -0.18f, 0.505f}, T_BLACK);
    }
    add_box(models[MODEL_TNT], ident, {0.98f, 0.98f, 0.98f}, {0, 0, 0}, IT_TNT_SIDE);
    add_box(models[MODEL_PUFF], ident, {1, 1, 1}, {0, 0, 0}, T_SMOKE);
    // Flames around a burning mob: a box of fire tiles from its feet up (scaled to its size).
    add_box(models[MODEL_FLAMES], ident, {1, 1, 1}, {0, 0.5f, 0}, T_FIRE);
}
