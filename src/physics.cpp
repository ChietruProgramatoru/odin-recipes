#include "physics.h"

#include <algorithm>
#include <cmath>

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
    in_water = world->box_in_water(x - half, y + 0.1, z - half, x + half - E, y + height * 0.5, z + half - E);
    return in_water;
}

bool Body::move_axis(int axis, double delta) {
    (axis == 0 ? x : axis == 1 ? y : z) += delta;
    if (!collides()) return false;
    if (axis == 0) {
        x = delta > 0 ? std::floor(x + half - E) - half : std::floor(x - half) + 1 + half;
        vx = 0;
    } else if (axis == 1) {
        y = delta > 0 ? std::floor(y + height - E) - height : std::floor(y) + 1;
        vy = 0;
    } else {
        z = delta > 0 ? std::floor(z + half - E) - half : std::floor(z - half) + 1 + half;
        vz = 0;
    }
    return true;
}

HitAxes Body::move(double dx, double dy, double dz) {
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

void Body::apply_gravity(double dt, double gravity) {
    if (in_water) {
        vy = std::max(vy - gravity * 0.25 * dt, -3.0);
        vy *= 1 - std::min(1.0, 2.0 * dt);
    } else {
        vy = std::max(vy - gravity * dt, -TERMINAL_VELOCITY);
    }
}

double Body::distance_to(double px, double py, double pz) const {
    return std::sqrt((x - px) * (x - px) + (y - py) * (y - py) + (z - pz) * (z - pz));
}
