// Axis-aligned box physics against the voxel grid (used by the player, cows and item drops).
#pragma once
#include <glm/vec3.hpp>

class World;

constexpr double GRAVITY = 28.0;
constexpr double TERMINAL_VELOCITY = 50.0;
// Water currents (M6): bodies that move themselves drift this fast with the flow; bodies that are
// only pushed (items, falling blocks) speed up this much per second (Beta: 0.014 per tick).
constexpr double CURRENT_SPEED = 1.4;
constexpr double CURRENT_PUSH = 5.6;

struct HitAxes {
    bool x = false, y = false, z = false;
};

class Body {
public:
    Body(World& world, double x, double y, double z, double half_width, double height);

    World* world;
    double x, y, z;  // feet centre
    double vx = 0, vy = 0, vz = 0;
    double half, height;
    bool on_ground = false;
    bool in_water = false;
    bool in_lava = false;
    glm::dvec3 current{0};  // which way the water it is in flows (normalised; set by check_water)
    // How high it walks up without jumping (Beta: half a block for the player, onto slabs and stairs).
    double step_height = 0;

    bool collides() const;
    // In water (sets in_water, in_lava and current). Returns in_water.
    bool check_water();
    bool in_liquid() const { return in_water || in_lava; }
    // Move with collision.
    HitAxes move(double dx, double dy, double dz);
    void apply_gravity(double dt, double gravity = GRAVITY);
    // Shorten a step so the body keeps standing on something (sneaking at an edge).
    void clamp_to_edges(double& dx, double& dz) const;
    double distance_to(double px, double py, double pz) const;

private:
    bool move_axis(int axis, double delta);
    HitAxes move_plain(double dx, double dy, double dz);
    bool supported(double dx, double dz) const;  // a solid block right under the feet, shifted by (dx, dz)
};
