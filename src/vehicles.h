// Minecarts and boats (M12), as Beta's EntityMinecart and EntityBoat: their physics one game tick
// at a time (20 a second, in Beta's units of blocks per tick), what breaking one leaves behind,
// how they are drawn and how they are saved. Pure logic on the World, no graphics.
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "container.h"
#include "mobs.h"
#include "world.h"

enum class VehicleKind : uint8_t { Minecart, StorageMinecart, PoweredMinecart, Boat };
constexpr int VEHICLE_KIND_COUNT = 4;

// Models drawn for vehicles (see mobs.h for the others). Each points its front along +z.
enum VehicleModel : int { MODEL_MINECART = 300, MODEL_BOAT };

constexpr double CART_HALF_WIDTH = 0.49, CART_HEIGHT = 0.7;  // Beta: 0.98 x 0.7
constexpr double BOAT_HALF_WIDTH = 0.75, BOAT_HEIGHT = 0.6;  // Beta: 1.5 x 0.6
constexpr double VEHICLE_MAX_SPEED = 0.4;                     // blocks per tick (8 a second)
constexpr int VEHICLE_BREAK_DAMAGE = 40;  // hits add 10 per point of attack damage and wear off 1 a tick; over this it breaks
constexpr int COAL_FUEL_TICKS = 1200;     // a powered minecart runs this long on one coal (Beta)
constexpr double BOAT_CRASH_SPEED = 0.15; // a boat hitting something faster than this (blocks per tick) breaks up

// Where a sitting rider's feet would be, from the bottom of the vehicle (Beta's mounted offsets put
// their eyes 1.17 above a minecart's bottom and 1.12 above a boat's). Their hips are 0.675 higher.
constexpr double CART_SEAT = -0.45, BOAT_SEAT = -0.5;

// The vehicle that sits on a rail (or water) from an item, or nothing for other items.
std::optional<VehicleKind> vehicle_for_item(ItemId item);
ItemId vehicle_item(VehicleKind kind);

// The middle of the rail line through a point, and the height of the rail's surface there (up the
// slope on a slope), as Beta's EntityMinecart.func_514_g. Nothing if there is no rail there.
std::optional<glm::dvec3> point_on_rail(World& world, glm::dvec3 p);

// What the rider is pressing: forward/back and strafe (-1..1), and which way they look (degrees).
struct RideInput {
    double forward = 0, strafe = 0, yaw = 0;
};

class Vehicle {
public:
    Vehicle(World& world, VehicleKind kind, double x, double y, double z);

    VehicleKind kind;
    Body body;                // the bottom middle of its box
    glm::dvec3 motion{0};     // blocks per tick
    glm::dvec3 prev{0};       // where it was a tick ago (drawing goes smoothly between the two)
    double yaw = 0, prev_yaw = 0;  // which way its front points (degrees, like the player's)
    bool on_rail = false;
    // A powered minecart: ticks of fuel left and which way it pushes. A storage minecart: its chest.
    int fuel = 0;
    double push_x = 0, push_z = 0;
    ChestSlots chest{};
    // Hits wear it down (Beta's damageTaken), and it rocks for a moment after each one.
    int damage = 0, hurt_time = 0, rock_dir = 1;
    bool ridden = false;

    bool is_cart() const { return kind != VehicleKind::Boat; }
    bool rideable() const { return kind == VehicleKind::Minecart || kind == VehicleKind::Boat; }

    // One game tick. `rider` is what the rider presses (boats only steer; nothing for an empty
    // vehicle). Returns true if a boat crashed and broke up.
    bool tick(const RideInput* rider);
    // Hit for `amount` attack damage. Returns true if that broke it.
    bool hit(int amount);
    // What it leaves behind: a boat that crashed breaks into 3 planks and 2 sticks (so does one
    // broken by hand, as in Beta); a minecart drops itself, plus its chest and what was in it or its furnace.
    std::vector<Stack> drops() const;
    // Coal into a powered minecart, and the way it should push (away from the player). True if it took the coal.
    bool add_fuel(ItemId item, double from_x, double from_z);
    // Something at (x, z) walking into it pushes it away (Beta's applyEntityCollision).
    void push_from(double x, double z);

    // Positions between the last tick and this one (alpha 0..1).
    glm::dvec3 position(double alpha) const;
    glm::dvec3 seat(double alpha) const;  // the rider's feet
    std::optional<float> ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const;
    bool overlaps(const Body& other) const;
    std::vector<ModelPart> parts(double alpha) const;

    // Saved with the chunk it is in.
    EntityRecord record() const;
    static std::unique_ptr<Vehicle> from_record(World& world, const EntityRecord& e);

private:
    World& world_;
    void tick_cart();
    bool tick_boat(const RideInput* rider);
};

// Two minecarts that bump push each other apart.
void bump_carts(Vehicle& a, Vehicle& b);

// The minecart and boat models (called by build_models).
void add_vehicle_models(std::map<int, MeshData>& models);
