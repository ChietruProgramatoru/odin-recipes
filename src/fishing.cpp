#include "fishing.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "icons.h"
#include "world.h"

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double SIZE = 0.25;  // the bobber's box (Beta)
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
}  // namespace

Bobber::Bobber(World& world, glm::dvec3 pos, glm::dvec3 v) : body(world, pos.x, pos.y, pos.z, SIZE / 2, SIZE), vel(v) {}

double Bobber::water_fraction() const {
    // Five slices of the box, each in water or not, counting how high the water stands in its cell.
    World& w = *body.world;
    int wet = 0;
    for (int i = 0; i < 5; ++i) {
        const double y = body.y + SIZE * (i + 0.5) / 5;
        const int bx = ifloor(body.x), by = ifloor(y), bz = ifloor(body.z);
        if (w.get_block(bx, by, bz) != WATER) continue;
        const double top = w.get_block(bx, by + 1, bz) == WATER ? by + 1.0 : by + fluid_fill(w.get_data(bx, by, bz));
        if (y < top) ++wet;
    }
    return wet / 5.0;
}

Bobber::Tick Bobber::tick(Rng& rng, bool rain, const std::vector<ArrowTarget>& targets) {
    Tick out;
    ++age;
    if (hooked) return out;  // Game keeps it on the mob
    if (in_ground) {
        ++ground_ticks;
        return out;
    }
    // Flying into a mob hooks it.
    const glm::dvec3 from = pos() + glm::dvec3(0, SIZE / 2, 0);
    const double len = glm::length(vel);
    if (len > 1e-6) {
        std::optional<float> best;
        for (const ArrowTarget& t : targets) {
            auto d = ray_box(glm::vec3(from), glm::vec3(vel / len), static_cast<float>(len), t.lo - glm::vec3(0.15f),
                             t.hi + glm::vec3(0.15f));
            if (d && (!best || *d < *best)) {
                best = d;
                out.hit = t;
            }
        }
        if (out.hit) {
            hooked_kind = out.hit->kind;
            hooked = out.hit->who;
            vel = glm::dvec3(0);
            return out;
        }
    }
    const double before = water_fraction();
    const HitAxes hit = body.move(vel.x, vel.y, vel.z);
    if (hit.x) vel.x = 0;
    if (hit.y) vel.y = 0;
    if (hit.z) vel.z = 0;
    const double wet = water_fraction();
    if ((hit.x || hit.y || hit.z) && wet == 0 && before == 0) {
        in_ground = true;  // landed on dry ground or hit a wall in the air
        vel = glm::dvec3(0);
        return out;
    }
    // Beta: water lifts it (and gravity pulls it) by 0.04 a tick, and it slows down, more in water.
    double drag = 0.92;
    vel.y += 0.04 * (wet * 2 - 1);
    if (wet > 0) {
        drag *= 0.9;
        vel.y *= 0.8;
    }
    vel *= drag;
    if (wet > 0) {
        if (bite_ticks > 0) {
            --bite_ticks;
        } else if (rng.below(rain ? RAIN_BITE_CHANCE : BITE_CHANCE) == 0) {
            bite_ticks = rng.randint(10, 39);
            vel.y -= 0.2;  // the bobber dips
            out.bite = true;
        }
    } else {
        bite_ticks = 0;
    }
    return out;
}

std::vector<ModelPart> Bobber::parts() const {
    // A bit lower while a fish tugs at it.
    const float y = static_cast<float>(body.y) - (bite_ticks > 0 ? 0.08f : 0.0f);
    return {{MODEL_BOBBER, glm::translate(glm::mat4(1.0f), glm::vec3(body.x, y, body.z))}};
}

Reel reel_result(const Bobber& b) {
    if (b.hooked) return Reel::Mob;
    if (b.bite_ticks > 0) return Reel::Fish;
    if (b.in_ground) return Reel::Ground;
    return Reel::Nothing;
}

int reel_wear(Reel r) {
    switch (r) {
        case Reel::Mob: return 3;
        case Reel::Fish: return 1;
        case Reel::Ground: return 2;
        default: return 0;
    }
}

std::vector<ModelPart> fishing_line(glm::dvec3 from, glm::dvec3 to, int segments) {
    std::vector<ModelPart> out;
    const double sag = std::min(1.0, glm::length(to - from) * 0.06);
    auto point = [&](int i) {
        const double t = static_cast<double>(i) / segments;
        glm::dvec3 p = from + (to - from) * t;
        p.y -= sag * 4 * t * (1 - t);  // hangs lowest in the middle
        return p;
    };
    for (int i = 0; i < segments; ++i) {
        const glm::dvec3 a = point(i), b = point(i + 1), d = b - a;
        const double len = glm::length(d);
        if (len < 1e-6) continue;
        const float yaw = static_cast<float>(std::atan2(d.x, d.z));
        const float pitch = static_cast<float>(std::asin(std::clamp(-d.y / len, -1.0, 1.0)));
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(a));
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::rotate(m, pitch, glm::vec3(1, 0, 0));
        m = glm::scale(m, glm::vec3(1, 1, static_cast<float>(len)));
        out.push_back({MODEL_LINE, m});
    }
    return out;
}

void add_fishing_models(std::map<int, MeshData>& models) {
    const glm::mat4 ident(1.0f);
    // The bobber: red on top, white underneath, the size of its box.
    MeshData& bob = models[MODEL_BOBBER];
    add_box(bob, ident, {0.18f, 0.1f, 0.18f}, {0, 0.05f, 0}, T_WHITE);
    add_box(bob, ident, {0.18f, 0.1f, 0.18f}, {0, 0.15f, 0}, IT_WOOL + 14);
    add_box(bob, ident, {0.04f, 0.06f, 0.04f}, {0, 0.23f, 0}, T_BLACK);
    // A piece of line one block long along +z, from the origin.
    add_box(models[MODEL_LINE], ident, {0.012f, 0.012f, 1.0f}, {0, 0, 0.5f}, T_BLACK);
}
