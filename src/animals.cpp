#include "animals.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "icons.h"
#include "items.h"
#include "textures.h"

namespace {

constexpr double PI = 3.14159265358979323846;
double radians(double deg) { return deg * PI / 180.0; }
double degrees(double rad) { return rad * 180.0 / PI; }

glm::mat4 translate(double x, double y, double z) {
    return glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}
glm::mat4 rot(double deg, glm::vec3 axis) { return glm::rotate(glm::mat4(1.0f), static_cast<float>(radians(deg)), axis); }
glm::mat4 rot_x(double deg) { return rot(deg, {1, 0, 0}); }
glm::mat4 rot_y(double deg) { return rot(deg, {0, 1, 0}); }
glm::mat4 rot_z(double deg) { return rot(deg, {0, 0, 1}); }

double pymod(double a, double b) { return a - b * std::floor(a / b); }
double angle_diff(double a, double b) { return pymod(b - a + 180, 360) - 180; }

const AnimalInfo INFO[ANIMAL_KIND_COUNT] = {
    {"Cow", 10, 0.45, 1.3, 1.3},
    {"Pig", 10, 0.45, 0.9, 1.2},
    {"Sheep", 8, 0.45, 1.3, 1.2},
    {"Chicken", 4, 0.2, 0.7, 1.0},
    {"Squid", 10, 0.45, 0.95, 0.0},
    {"Wolf", 8, 0.3, 0.85, 1.6},
};

}  // namespace

const AnimalInfo& animal_info(AnimalKind kind) { return INFO[static_cast<int>(kind)]; }

// --- Rules ------------------------------------------------------------------------------

int natural_sheep_color(Rng& rng) {
    // Beta's EntitySheep.getRandomFleeceColor: 5% black, 5% grey, 5% light grey, 3% brown,
    // 1 in 500 of the rest pink, everything else white.
    const int roll = rng.below(100);
    if (roll < 5) return 15;  // black
    if (roll < 10) return 7;  // grey
    if (roll < 15) return 8;  // light grey
    if (roll < 18) return 12; // brown
    return rng.below(500) == 0 ? 6 : SHEEP_COLOR_WHITE;
}

std::optional<int> dye_wool_color(ItemId item) {
    // The dyes are in Beta's dye order (ink sac 0 .. bone meal 15) and wool the other way round.
    if (item < INK_SAC || item > BONE_MEAL) return std::nullopt;
    return 15 - (item - INK_SAC);
}

ItemId wool_item(int color) { return static_cast<ItemId>(WHITE_WOOL + std::clamp(color, 0, 15)); }

int shear_count(Rng& rng) { return rng.randint(1, 3); }

int egg_delay_ticks(Rng& rng) { return 6000 + rng.below(6000); }

int chicks_from_egg(Rng& rng) {
    if (rng.below(8) != 0) return 0;
    return rng.below(32) == 0 ? 4 : 1;
}

bool tame_succeeds(Rng& rng) { return rng.below(3) == 0; }

int wolf_food_heal(ItemId item) {
    // Beta's wolves eat pork chops. This game has beef too, so they like that as well.
    if (item != RAW_PORKCHOP && item != COOKED_PORKCHOP && item != BEEF && item != STEAK) return 0;
    const FoodInfo* food = food_info(item);
    return food ? food->hunger : 0;
}

std::vector<ItemId> animal_drops(AnimalKind kind, const AnimalLootState& state, Rng& rng) {
    std::vector<ItemId> out;
    auto some = [&](ItemId item, int lo, int hi) {
        for (int n = rng.randint(lo, hi); n > 0; --n) out.push_back(item);
    };
    switch (kind) {
        case AnimalKind::Cow:
            some(LEATHER, 0, 2);
            some(state.burning ? static_cast<ItemId>(STEAK) : static_cast<ItemId>(BEEF), 1, 3);  // this game keeps beef
            break;
        case AnimalKind::Pig:
            some(state.burning ? COOKED_PORKCHOP : RAW_PORKCHOP, 0, 2);
            if (state.saddled) out.push_back(SADDLE);  // Beta lost the saddle; this game gives it back
            break;
        case AnimalKind::Sheep:
            if (!state.sheared) out.push_back(wool_item(state.wool));
            break;
        case AnimalKind::Chicken: some(FEATHER, 0, 2); break;
        case AnimalKind::Squid: some(INK_SAC, 1, 3); break;
        case AnimalKind::Wolf: break;
    }
    return out;
}

std::optional<AnimalKind> pick_land_animal(Biome biome, Rng& rng) {
    if (biome == Biome::DESERT) return std::nullopt;
    const bool wolves = biome == Biome::FOREST || biome == Biome::TAIGA;
    const int weights[] = {8, 10, 12, 10, 0, wolves ? 2 : 0};  // cow, pig, sheep, chicken, squid, wolf
    int total = 0;
    for (int w : weights) total += w;
    int roll = rng.below(total);
    for (int k = 0; k < ANIMAL_KIND_COUNT; ++k) {
        if (roll < weights[k]) return static_cast<AnimalKind>(k);
        roll -= weights[k];
    }
    return AnimalKind::Cow;
}

// --- Animal -------------------------------------------------------------------------------

Animal::Animal(World& world, AnimalKind kind_, double x, double y, double z, Rng& rng)
    : kind(kind_), body(world, x, y, z, animal_info(kind_).half, animal_info(kind_).height) {
    health = info().health;
    yaw = rng.uniform(0, 360);
    target_yaw = yaw;
    state_timer = rng.uniform(1, 4);
    if (kind == AnimalKind::Sheep) wool = natural_sheep_color(rng);
    if (kind == AnimalKind::Chicken) egg_timer = egg_delay_ticks(rng);
}

void Animal::hurt(int amount, double from_x, double from_z) {
    if (!alive()) return;
    health -= amount;
    double dx = body.x - from_x, dz = body.z - from_z;
    double d = std::hypot(dx, dz);
    if (d == 0) d = 1.0;
    knock_x = dx / d * 7;
    knock_z = dz / d * 7;
    if (body.on_ground) body.vy = 5.5;
    flash = 0.3;
    sitting = false;
    if (kind != AnimalKind::Wolf && kind != AnimalKind::Squid) {
        state = State::Panic;
        state_timer = 5.0;
        target_yaw = degrees(std::atan2(dx, dz));
    }
    if (!alive()) dying = 0.0001;
}

void Animal::tame() {
    tamed = true;
    angry = false;
    sitting = true;  // Beta: a wolf sits down as soon as it is tamed
    prey = nullptr;
    health = TAMED_WOLF_HEALTH;
}

ArrowTarget Animal::target(int kind_tag, int index) const {
    const float h = static_cast<float>(body.half + 0.15);
    const float top = static_cast<float>(body.height + 0.2);
    return {glm::vec3(body.x - h, body.y, body.z - h), glm::vec3(body.x + h, body.y + top, body.z + h), kind_tag, index, this};
}

std::optional<float> Animal::ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const {
    const ArrowTarget t = target(0, 0);
    return ray_box(origin, direction, max_dist, t.lo, t.hi);
}

void Animal::wander(double dt, Rng& rng, double player_x, double player_z) {
    state_timer -= dt;
    if (state_timer <= 0) {
        if (state == State::Panic || state == State::Walk) {
            state = State::Idle;
            state_timer = rng.uniform(2, 6);
            graze = kind != AnimalKind::Wolf && rng.random() < 0.5;
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
}

void Animal::chase(double dt, glm::dvec3 to, double run) {
    (void)dt;
    target_yaw = degrees(std::atan2(to.x - body.x, to.z - body.z));
    state = State::Walk;
    state_timer = std::max(state_timer, 0.5);
    graze = false;
    speed = std::max(speed, run * 0.5);
    want_ = run;
}

void Animal::walk(double dt, Rng& rng) {
    // Panicking and chasing animals turn fast (a wolf about to bite fastest).
    double turn = want_ > info().walk_speed + 0.1 ? (kind == AnimalKind::Wolf ? 720 : 360) : state == State::Panic ? 360 : 120;
    double diff = angle_diff(yaw, target_yaw);
    yaw += std::clamp(diff, -turn * dt, turn * dt);

    double want = want_;
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
    b.vx += b.current.x * CURRENT_SPEED;  // carried along by water
    b.vz += b.current.z * CURRENT_SPEED;
    b.apply_gravity(dt);
    if (b.in_liquid()) b.vy = std::max(b.vy, 2.0);
    // Chickens flap their wings and fall slowly (Beta).
    if (kind == AnimalKind::Chicken && !b.on_ground && !b.in_liquid()) {
        b.vy = std::max(b.vy, -3.0);
        flap += dt * 30;
    }
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
}

void Animal::swim_about(double dt, Rng& rng) {
    Body& b = body;
    b.check_water();
    stroke += dt * (b.in_water ? 6 : 14);
    if (b.in_water) {
        // Beta's squid pick a new direction now and then and drift there, slowing between strokes.
        swim_timer -= dt;
        if (swim_timer <= 0 || flash > 0.25) {
            swim_timer = rng.uniform(1.5, 4);
            const double a = rng.uniform(0, 2 * PI);
            const double s = rng.uniform(0.8, 2.2);
            swim = {std::cos(a) * s, rng.uniform(-0.8, 0.8), std::sin(a) * s};
        }
        const double k = std::min(1.0, dt * 2);
        b.vx += (swim.x - b.vx) * k + knock_x * dt;
        b.vy += (swim.y - b.vy) * k;
        b.vz += (swim.z - b.vz) * k;
    } else {
        // Out of the water a squid just flops about.
        b.apply_gravity(dt);
        b.vx *= std::max(0.0, 1 - dt * 6);
        b.vz *= std::max(0.0, 1 - dt * 6);
    }
    knock_x *= std::max(0.0, 1 - dt * 6);
    knock_z *= std::max(0.0, 1 - dt * 6);
    HitAxes hit = b.move(b.vx * dt, b.vy * dt, b.vz * dt);
    if (hit.x) swim.x = -swim.x;
    if (hit.y) swim.y = -swim.y;
    if (hit.z) swim.z = -swim.z;
    const double h = std::hypot(b.vx, b.vz);
    if (h > 0.2) target_yaw = degrees(std::atan2(b.vx, b.vz));
    yaw += std::clamp(angle_diff(yaw, target_yaw), -90 * dt, 90 * dt);
    const double want_pitch = b.in_water ? degrees(std::atan2(h, 1.0)) * 0.8 : 80;
    pitch += (want_pitch - pitch) * std::min(1.0, dt * 2);
}

bool Animal::tick(double dt, const AnimalSenses& senses, Rng& rng, AnimalActions& out) {
    if (dying) {
        dying += dt;
        roll = std::min(90.0, dying * 200);
        return dying > 0.7;
    }
    flash = std::max(0.0, flash - dt);
    bite_timer = std::max(0.0, bite_timer - dt);

    // Fire: water puts it out; otherwise it hurts 1 a second.
    if (body.in_water) burning = 0;
    if (burning > 0) {
        burning -= dt;
        burn_hurt_ -= dt;
        if (burn_hurt_ <= 0) {
            burn_hurt_ = 1.0;
            health -= 1;  // burning doesn't push or scare
            flash = 0.3;
            if (!alive()) {
                dying = 0.0001;
                return false;
            }
        }
    }

    if (kind == AnimalKind::Squid) {
        swim_about(dt, rng);
        return false;
    }

    want_ = 0;
    auto horiz = [&](glm::dvec3 p) { return std::hypot(p.x - body.x, p.z - body.z); };
    auto in_reach = [&](glm::dvec3 p, double other_half) {
        return horiz(p) < body.half + other_half + 0.7 && std::abs(p.y - body.y) < 1.5;
    };
    if (kind == AnimalKind::Wolf) {
        const int bite = tamed ? TAMED_WOLF_BITE : WILD_WOLF_BITE;
        const double to_player = horiz(senses.player);
        if (sitting) {
            state = State::Idle;
            graze = false;
        } else if (senses.prey) {
            chase(dt, *senses.prey, 5.0);
            if (in_reach(*senses.prey, senses.prey_half) && bite_timer <= 0) {
                out.bite_prey = bite;
                bite_timer = 1.0;
            }
        } else if (angry && !tamed && senses.player_targetable && !senses.peaceful && to_player < WOLF_ANGER_RANGE) {
            chase(dt, senses.player, 5.0);
            if (in_reach(senses.player, 0.3) && bite_timer <= 0) {
                out.bite_player = bite;
                bite_timer = 1.0;
            }
        } else if (tamed && to_player > WOLF_TELEPORT_DISTANCE) {
            out.teleport = true;
        } else if (tamed && to_player > WOLF_FOLLOW_DISTANCE) {
            chase(dt, senses.player, 4.6);
        } else {
            wander(dt, rng, senses.player.x, senses.player.z);
        }
    } else {
        wander(dt, rng, senses.player.x, senses.player.z);
    }
    if (want_ == 0) want_ = state == State::Idle ? 0.0 : state == State::Walk ? info().walk_speed : 4.2;
    if (kind == AnimalKind::Wolf && sitting) want_ = 0;
    walk(dt, rng);

    if (kind == AnimalKind::Chicken) {
        egg_timer -= dt * 20;
        if (egg_timer <= 0) {
            out.lay_egg = true;
            egg_timer = egg_delay_ticks(rng);
        }
        if (body.on_ground) flap = 0;
    }

    double target_pitch = (state == State::Idle && graze) ? 40 : 0;
    head_pitch += (target_pitch - head_pitch) * std::min(1.0, dt * 4);
    return false;
}

// --- Drawing ------------------------------------------------------------------------------

std::vector<ModelPart> Animal::parts() const {
    std::vector<ModelPart> out;
    glm::mat4 root = translate(body.x, body.y, body.z) * rot_y(yaw) * rot_z(roll);
    const double swing = std::sin(walk_phase) * std::min(1.0, speed) * 35;
    auto legs4 = [&](int model, double hip_y, double sx, double fz, double bz) {
        const double xs[4] = {-sx, sx, -sx, sx}, zs[4] = {fz, fz, bz, bz};
        for (int i = 0; i < 4; ++i) {
            const double s = (i == 0 || i == 3) ? swing : -swing;
            out.push_back({model, root * translate(xs[i], hip_y, zs[i]) * rot_x(s)});
        }
    };
    switch (kind) {
        case AnimalKind::Cow:
            out.push_back({MODEL_COW_BODY, root});
            legs4(MODEL_COW_LEG, 0.75, 0.22, 0.4, -0.4);
            out.push_back({MODEL_COW_HEAD, root * translate(0, 1.2, 0.58) * rot_x(head_pitch)});
            break;
        case AnimalKind::Pig:
            out.push_back({MODEL_PIG_BODY, root});
            if (saddled) out.push_back({MODEL_SADDLE, root});
            legs4(MODEL_PIG_LEG, 0.375, 0.19, 0.32, -0.32);
            out.push_back({MODEL_PIG_HEAD, root * translate(0, 0.75, 0.45) * rot_x(head_pitch)});
            break;
        case AnimalKind::Sheep: {
            out.push_back({MODEL_SHEEP_BODY, root});
            if (!sheared) out.push_back({MODEL_WOOL_BODY + wool, root});
            const double xs[4] = {-0.19, 0.19, -0.19, 0.19}, zs[4] = {0.35, 0.35, -0.35, -0.35};
            for (int i = 0; i < 4; ++i) {
                const double s = (i == 0 || i == 3) ? swing : -swing;
                const glm::mat4 leg = root * translate(xs[i], 0.75, zs[i]) * rot_x(s);
                out.push_back({MODEL_SHEEP_LEG, leg});
                if (!sheared) out.push_back({MODEL_WOOL_LEG + wool, leg});
            }
            const glm::mat4 head = root * translate(0, 1.12, 0.5) * rot_x(head_pitch);
            out.push_back({MODEL_SHEEP_HEAD, head});
            if (!sheared) out.push_back({MODEL_WOOL_HEAD + wool, head});
            break;
        }
        case AnimalKind::Chicken: {
            out.push_back({MODEL_CHICKEN_BODY, root});
            const double wing = flap > 0 ? std::sin(flap) * 50 + 50 : 0;
            out.push_back({MODEL_CHICKEN_WING, root * translate(-0.2, 0.62, 0) * rot_z(-wing)});
            out.push_back({MODEL_CHICKEN_WING, root * translate(0.2, 0.62, 0) * rot_z(wing)});
            out.push_back({MODEL_CHICKEN_LEG, root * translate(-0.07, 0.32, 0.0) * rot_x(swing)});
            out.push_back({MODEL_CHICKEN_LEG, root * translate(0.07, 0.32, 0.0) * rot_x(-swing)});
            out.push_back({MODEL_CHICKEN_HEAD, root * translate(0, 0.62, 0.2) * rot_x(head_pitch * 0.5)});
            break;
        }
        case AnimalKind::Squid: {
            // Squid stand on their tentacles; tipped forwards as they swim.
            const glm::mat4 body_m = translate(body.x, body.y + 0.5, body.z) * rot_y(yaw) * rot_x(pitch) * rot_z(roll);
            out.push_back({MODEL_SQUID_BODY, body_m});
            const double open = 10 + (std::sin(stroke) + 1) * 15;
            for (int i = 0; i < 8; ++i) {
                const double a = i * 45.0;
                const glm::mat4 t = body_m * rot_y(a) * translate(0, -0.3, 0.28) * rot_x(open);
                out.push_back({MODEL_SQUID_TENTACLE, t});
            }
            break;
        }
        case AnimalKind::Wolf: {
            const double sit = sitting ? 1 : 0;
            const glm::mat4 body_m = root * translate(0, 0.5 - sit * 0.15, 0) * rot_x(-sit * 30);
            out.push_back({MODEL_WOLF_BODY, body_m});
            out.push_back({MODEL_WOLF_MANE, body_m});
            // Front legs stand; back legs fold under it when it sits.
            const double fx[2] = {-0.1, 0.1};
            for (int i = 0; i < 2; ++i) {
                const double s = i == 0 ? swing : -swing;
                out.push_back({MODEL_WOLF_LEG, root * translate(fx[i], 0.5, 0.22) * rot_x(sitting ? 0.0 : s)});
                out.push_back({MODEL_WOLF_LEG, sitting ? root * translate(fx[i], 0.2, -0.12) * rot_x(-80)
                                                       : root * translate(fx[i], 0.5, -0.25) * rot_x(-s)});
            }
            // Tamed wolves hold their tails higher the healthier they are (Beta's health bar).
            const double tail = tamed ? 20 + 70 * std::clamp(health / static_cast<double>(TAMED_WOLF_HEALTH), 0.0, 1.0)
                                      : angry ? 70 : 25;
            out.push_back({MODEL_WOLF_TAIL, body_m * translate(0, 0.08, -0.32) * rot_x(-tail + 90)});
            out.push_back({angry ? MODEL_WOLF_HEAD_ANGRY : MODEL_WOLF_HEAD,
                           root * translate(0, 0.65 - sit * 0.05, 0.38) * rot_x(head_pitch)});
            break;
        }
    }
    const glm::vec4 overlay = (flash > 0 || dying) ? glm::vec4(1, 0, 0, 0.45f) : glm::vec4(0);
    for (ModelPart& p : out) p.overlay = overlay;
    if (burning > 0 && !dying) {
        const double w = body.half * 2 + 0.2, h = body.height + 0.2;
        out.push_back({MODEL_FLAMES, translate(body.x, body.y - 0.05, body.z) * rot_y(yaw) *
                                         glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(w), static_cast<float>(h),
                                                                               static_cast<float>(w)))});
    }
    return out;
}

void add_animal_models(std::map<int, MeshData>& models) {
    const glm::mat4 ident(1.0f);
    // Cow: body at its feet frame, legs from the hip, head from the neck.
    MeshData& cow = models[MODEL_COW_BODY];
    add_box(cow, ident, {0.75f, 0.62f, 1.15f}, {0, 1.06f, 0}, T_COW_HIDE);
    add_box(cow, ident, {0.08f, 0.5f, 0.08f}, {0, 0.95f, -0.6f}, T_COW_HIDE);  // tail
    MeshData& cow_leg = models[MODEL_COW_LEG];
    add_box(cow_leg, ident, {0.25f, 0.63f, 0.25f}, {0, -0.315f, 0}, T_COW_HIDE);
    add_box(cow_leg, ident, {0.26f, 0.12f, 0.26f}, {0, -0.69f, 0}, T_HOOF);
    MeshData& cow_head = models[MODEL_COW_HEAD];
    add_box(cow_head, ident, {0.5f, 0.5f, 0.38f}, {0, 0, 0.19f}, T_COW_HEAD);
    add_box(cow_head, ident, {0.32f, 0.2f, 0.08f}, {0, -0.13f, 0.41f}, T_SNOUT);
    for (float side : {-1.0f, 1.0f}) {
        add_box(cow_head, ident, {0.07f, 0.07f, 0.02f}, {side * 0.15f, 0.08f, 0.385f}, T_BLACK);  // eyes
        add_box(cow_head, ident, {0.08f, 0.14f, 0.08f}, {side * 0.29f, 0.25f, 0.12f}, T_HORN);    // horns
    }

    // Pig: a pink barrel on short legs, with a flat snout.
    add_box(models[MODEL_PIG_BODY], ident, {0.62f, 0.5f, 1.0f}, {0, 0.6f, 0}, T_PIG);
    add_box(models[MODEL_PIG_LEG], ident, {0.24f, 0.375f, 0.24f}, {0, -0.1875f, 0}, T_PIG);
    MeshData& pig_head = models[MODEL_PIG_HEAD];
    add_box(pig_head, ident, {0.5f, 0.5f, 0.45f}, {0, 0, 0.2f}, T_PIG);
    add_box(pig_head, ident, {0.25f, 0.18f, 0.08f}, {0, -0.1f, 0.46f}, T_PIG_SNOUT);
    for (float side : {-1.0f, 1.0f}) {
        add_box(pig_head, ident, {0.07f, 0.07f, 0.02f}, {side * 0.15f, 0.08f, 0.43f}, T_BLACK);
        add_box(pig_head, ident, {0.04f, 0.06f, 0.02f}, {side * 0.06f, -0.1f, 0.505f}, T_BLACK);  // nostrils
    }
    MeshData& saddle = models[MODEL_SADDLE];
    add_box(saddle, ident, {0.66f, 0.08f, 0.45f}, {0, 0.87f, 0.02f}, T_SADDLE);
    add_box(saddle, ident, {0.66f, 0.2f, 0.06f}, {0, 0.75f, -0.2f}, T_SADDLE);
    for (float side : {-1.0f, 1.0f}) add_box(saddle, ident, {0.04f, 0.4f, 0.08f}, {side * 0.33f, 0.62f, 0.05f}, T_BLACK);

    // Sheep: the skin shows once sheared; wool is a fluffier box in the sheep's colour.
    add_box(models[MODEL_SHEEP_BODY], ident, {0.5f, 0.45f, 0.95f}, {0, 0.98f, 0}, T_SHEEP_SKIN);
    add_box(models[MODEL_SHEEP_LEG], ident, {0.22f, 0.75f, 0.22f}, {0, -0.375f, 0}, T_SHEEP_SKIN);
    MeshData& sheep_head = models[MODEL_SHEEP_HEAD];
    add_box(sheep_head, ident, {0.38f, 0.38f, 0.45f}, {0, 0, 0.18f}, T_SHEEP_FACE);
    for (float side : {-1.0f, 1.0f}) add_box(sheep_head, ident, {0.06f, 0.06f, 0.02f}, {side * 0.11f, 0.06f, 0.41f}, T_BLACK);
    for (int c = 0; c < 16; ++c) {
        const int tile = IT_WOOL + c;
        add_box(models[MODEL_WOOL_BODY + c], ident, {0.76f, 0.66f, 1.1f}, {0, 1.0f, 0}, tile);
        add_box(models[MODEL_WOOL_LEG + c], ident, {0.28f, 0.32f, 0.28f}, {0, -0.16f, 0}, tile);
        add_box(models[MODEL_WOOL_HEAD + c], ident, {0.44f, 0.2f, 0.36f}, {0, 0.14f, 0.12f}, tile);
    }

    // Chicken: a white body on thin legs, red wattle and yellow beak.
    add_box(models[MODEL_CHICKEN_BODY], ident, {0.38f, 0.38f, 0.5f}, {0, 0.5f, 0}, T_WHITE);
    MeshData& chicken_head = models[MODEL_CHICKEN_HEAD];
    add_box(chicken_head, ident, {0.25f, 0.38f, 0.19f}, {0, 0.19f, 0.05f}, T_WHITE);
    add_box(chicken_head, ident, {0.25f, 0.12f, 0.12f}, {0, 0.25f, 0.2f}, T_BEAK);
    add_box(chicken_head, ident, {0.12f, 0.12f, 0.06f}, {0, 0.13f, 0.17f}, T_WATTLE);
    for (float side : {-1.0f, 1.0f})
        add_box(chicken_head, ident, {0.05f, 0.05f, 0.02f}, {side * 0.08f, 0.32f, 0.15f}, T_BLACK);
    add_box(models[MODEL_CHICKEN_WING], ident, {0.06f, 0.25f, 0.38f}, {0, -0.12f, 0}, T_WHITE);
    MeshData& chicken_leg = models[MODEL_CHICKEN_LEG];
    add_box(chicken_leg, ident, {0.05f, 0.32f, 0.05f}, {0, -0.16f, 0}, T_BEAK);
    add_box(chicken_leg, ident, {0.15f, 0.03f, 0.15f}, {0, -0.31f, 0.04f}, T_BEAK);

    // Squid: a tall body with eight tentacles hanging from it.
    MeshData& squid = models[MODEL_SQUID_BODY];
    add_box(squid, ident, {0.75f, 1.0f, 0.75f}, {0, 0.2f, 0}, T_SQUID);
    for (float side : {-1.0f, 1.0f}) {
        add_box(squid, ident, {0.1f, 0.12f, 0.02f}, {side * 0.18f, -0.1f, 0.38f}, T_WHITE);
        add_box(squid, ident, {0.05f, 0.08f, 0.02f}, {side * 0.18f, -0.1f, 0.39f}, T_BLACK);
    }
    add_box(models[MODEL_SQUID_TENTACLE], ident, {0.12f, 0.9f, 0.12f}, {0, -0.45f, 0}, T_SQUID);

    // Wolf: grey, with a shaggy mane, pointy ears and a long snout.
    add_box(models[MODEL_WOLF_BODY], ident, {0.36f, 0.36f, 0.6f}, {0, 0.05f, -0.1f}, T_WOLF);
    add_box(models[MODEL_WOLF_MANE], ident, {0.5f, 0.44f, 0.36f}, {0, 0.07f, 0.17f}, T_WOLF);
    add_box(models[MODEL_WOLF_LEG], ident, {0.13f, 0.5f, 0.13f}, {0, -0.25f, 0}, T_WOLF);
    add_box(models[MODEL_WOLF_TAIL], ident, {0.12f, 0.12f, 0.5f}, {0, 0, -0.25f}, T_WOLF_DARK);
    for (int angry = 0; angry < 2; ++angry) {
        MeshData& head = models[angry ? MODEL_WOLF_HEAD_ANGRY : MODEL_WOLF_HEAD];
        add_box(head, ident, {0.38f, 0.38f, 0.26f}, {0, 0, 0.13f}, T_WOLF);
        add_box(head, ident, {0.19f, 0.16f, 0.25f}, {0, -0.09f, 0.38f}, T_WOLF_DARK);
        add_box(head, ident, {0.08f, 0.06f, 0.03f}, {0, -0.04f, 0.5f}, T_BLACK);  // nose
        for (float side : {-1.0f, 1.0f}) {
            add_box(head, ident, {0.12f, 0.12f, 0.06f}, {side * 0.12f, 0.24f, 0.08f}, T_WOLF_DARK);  // ears
            add_box(head, ident, {0.07f, 0.05f, 0.02f}, {side * 0.09f, 0.06f, 0.265f}, angry ? T_SPIDER_EYE : T_BLACK);
        }
    }
}
