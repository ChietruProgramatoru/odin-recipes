// Axis-aligned box physics against the voxel grid (used by the player, cows and item drops).
#pragma once

class World;

constexpr double GRAVITY = 28.0;
constexpr double TERMINAL_VELOCITY = 50.0;

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

    bool collides() const;
    bool check_water();
    // Move with collision.
    HitAxes move(double dx, double dy, double dz);
    void apply_gravity(double dt, double gravity = GRAVITY);
    double distance_to(double px, double py, double pz) const;

private:
    bool move_axis(int axis, double delta);
};
