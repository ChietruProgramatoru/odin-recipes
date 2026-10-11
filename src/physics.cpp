#include "physics.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "blockupdate.h"
#include "world.h"

namespace {
constexpr double E = 1e-7;
}

Body::Body(World& w, double x_, double y_, double z_, double half_width, double height_)
    : world(&w), x(x_), y(y_), z(z_), half(half_width), height(height_) {}

bool Body::collides() const {
    return world->box_collides(x - half, y, z - half, x + half - E, y + height - E, z + half - E);
}

bool Body::check_water() {
    const double x0 = x - half, y0 = y + 0.1, z0 = z - half, x1 = x + half - E, y1 = y + height * 0.5, z1 = z + half - E;
    in_water = world->box_in_water(x0, y0, z0, x1, y1, z1);
    in_lava = world->box_in(LAVA, x0, y0, z0, x1, y1, z1);
    current = glm::dvec3(0);
    if (in_water) {
        // Beta's handleMaterialAcceleration: the flow of every water cell the body touches.
        for (int bx = static_cast<int>(std::floor(x0)); bx <= static_cast<int>(std::floor(x1)); ++bx)
            for (int by = static_cast<int>(std::floor(y0)); by <= static_cast<int>(std::floor(y1)); ++by)
                for (int bz = static_cast<int>(std::floor(z0)); bz <= static_cast<int>(std::floor(z1)); ++bz)
                    if (world->get_block(bx, by, bz) == WATER) current += flow_vector(*world, {bx, by, bz});
        if (glm::length(current) > 0) current = glm::normalize(current);
    }
    return in_water;
}

bool Body::move_axis(int axis, double delta) {
    (axis == 0 ? x : axis == 1 ? y : z) += delta;
    if (!collides()) return false;
    // Stop against the nearest side of what it ran into (slabs, stairs and fences are part blocks).
    const double x0 = x - half, y0 = y, z0 = z - half, x1 = x + half - E, y1 = y + height - E, z1 = z + half - E;
    if (axis == 0) {
        x = delta > 0 ? world->box_limit(0, true, x0, y0, z0, x1, y1, z1) - half
                      : world->box_limit(0, false, x0, y0, z0, x1, y1, z1) + half;
        vx = 0;
    } else if (axis == 1) {
        // Landing: stand on the highest top below (a bed or a slab is lower than a full block).
        y = delta > 0 ? world->box_limit(1, true, x0, y0, z0, x1, y1, z1) - height : world->box_top(x0, y0, z0, x1, y1, z1);
        vy = 0;
    } else {
        z = delta > 0 ? world->box_limit(2, true, x0, y0, z0, x1, y1, z1) - half
                      : world->box_limit(2, false, x0, y0, z0, x1, y1, z1) + half;
        vz = 0;
    }
    return true;
}

HitAxes Body::move_plain(double dx, double dy, double dz) {
    int steps = std::max(1, static_cast<int>(std::ceil(std::max({std::abs(dx), std::abs(dy), std::abs(dz)}) / 0.4)));
    HitAxes hit;
    bool grounded = false;
    for (int i = 0; i < steps; ++i) {
        if (dy) {
            bool h = move_axis(1, dy / steps);
            hit.y |= h;
            if (h && dy < 0) grounded = true;
        }
        if (dx) hit.x |= move_axis(0, dx / steps);
        if (dz) hit.z |= move_axis(2, dz / steps);
    }
    on_ground = grounded || (dy == 0 && on_ground);
    return hit;
}

HitAxes Body::move(double dx, double dy, double dz) {
    const double sx = x, sy = y, sz = z, svx = vx, svy = vy, svz = vz;
    const bool was_on_ground = on_ground;
    HitAxes hit = move_plain(dx, dy, dz);
    if (step_height <= 0 || !(hit.x || hit.z) || !(was_on_ground || on_ground) || dy > 0) return hit;
    // Beta's step: blocked sideways on the ground, try again from step_height up (onto a slab or a
    // stair) and keep whichever gets further.
    const double px = x, py = y, pz = z, pvx = vx, pvy = vy, pvz = vz;
    const bool pground = on_ground;
    x = sx, y = sy, z = sz, vx = svx, vy = svy, vz = svz;
    y += step_height;
    if (!collides()) {
        HitAxes stepped = move_plain(dx, 0, dz);
        const double down = step_height + std::max(0.0, -dy);
        y -= down;
        if (collides()) {
            y = world->box_top(x - half, y, z - half, x + half - E, y + height - E, z + half - E);
            if (std::hypot(x - sx, z - sz) > std::hypot(px - sx, pz - sz) + 1e-6) {
                on_ground = true;
                vy = 0;
                stepped.y = true;
                return stepped;
            }
        }
    }
    x = px, y = py, z = pz, vx = pvx, vy = pvy, vz = pvz;
    on_ground = pground;
    return hit;
}

bool Body::supported(double dx, double dz) const {
    return world->box_collides(x - half + dx, y - 0.5, z - half + dz, x + half - E + dx, y - 0.5, z + half - E + dz);
}

void Body::clamp_to_edges(double& dx, double& dz) const {
    if (!supported(0, 0)) return;
    const double step = 0.05;
    auto shrink = [&](double& v) { v = std::abs(v) < step ? 0 : v - std::copysign(step, v); };
    while (dx != 0 && !supported(dx, 0)) shrink(dx);
    while (dz != 0 && !supported(0, dz)) shrink(dz);
    while (dx != 0 && dz != 0 && !supported(dx, dz)) {
        shrink(dx);
        shrink(dz);
    }
}

void Body::apply_gravity(double dt, double gravity) {
    if (in_liquid()) {
        vy = std::max(vy - gravity * 0.25 * dt, in_lava ? -1.5 : -3.0);
        vy *= 1 - std::min(1.0, 2.0 * dt);
    } else {
        vy = std::max(vy - gravity * dt, -TERMINAL_VELOCITY);
    }
}

double Body::distance_to(double px, double py, double pz) const {
    return std::sqrt((x - px) * (x - px) + (y - py) * (y - py) + (z - pz) * (z - pz));
}
