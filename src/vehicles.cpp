#include "vehicles.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "rails.h"

namespace {
constexpr double PI = 3.14159265358979323846;
double degrees(double r) { return r * 180.0 / PI; }
double radians(double d) { return d * PI / 180.0; }
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
double wrap_degrees(double d) {
    d = std::fmod(d, 360.0);
    if (d >= 180) d -= 360;
    if (d < -180) d += 360;
    return d;
}
glm::mat4 rotate(double deg, glm::vec3 axis) {
    return glm::rotate(glm::mat4(1.0f), static_cast<float>(radians(deg)), axis);
}
}  // namespace

std::optional<VehicleKind> vehicle_for_item(ItemId item) {
    switch (item) {
        case MINECART: return VehicleKind::Minecart;
        case STORAGE_MINECART: return VehicleKind::StorageMinecart;
        case POWERED_MINECART: return VehicleKind::PoweredMinecart;
        case BOAT: return VehicleKind::Boat;
        default: return std::nullopt;
    }
}

ItemId vehicle_item(VehicleKind kind) {
    switch (kind) {
        case VehicleKind::Minecart: return MINECART;
        case VehicleKind::StorageMinecart: return STORAGE_MINECART;
        case VehicleKind::PoweredMinecart: return POWERED_MINECART;
        case VehicleKind::Boat: break;
    }
    return BOAT;
}

std::optional<glm::dvec3> point_on_rail(World& world, glm::dvec3 p) {
    const int i = ifloor(p.x), k = ifloor(p.z);
    int j = ifloor(p.y);
    if (is_rail(world.get_block(i, j - 1, k))) --j;
    const BlockId b = world.get_block(i, j, k);
    if (!is_rail(b)) return std::nullopt;
    const auto& e = rail_ends(rail_shape(b, world.get_data(i, j, k)));
    // The two ends' middles, and the point between them nearest p.
    const double x0 = i + 0.5 + e[0].x * 0.5, y0 = j + 0.5 + e[0].y * 0.5, z0 = k + 0.5 + e[0].z * 0.5;
    const double x1 = i + 0.5 + e[1].x * 0.5, y1 = j + 0.5 + e[1].y * 0.5, z1 = k + 0.5 + e[1].z * 0.5;
    const double dx = x1 - x0, dy = (y1 - y0) * 2, dz = z1 - z0;
    double t;
    if (dx == 0) {
        p.x = i + 0.5;
        t = p.z - k;
    } else if (dz == 0) {
        p.z = k + 0.5;
        t = p.x - i;
    } else {
        t = ((p.x - x0) * dx + (p.z - z0) * dz) * 2;
    }
    double y = y0 + dy * t;
    if (dy < 0) y += 1;
    if (dy > 0) y += 0.5;
    return glm::dvec3(x0 + dx * t, y - 0.5, z0 + dz * t);
}

Vehicle::Vehicle(World& world, VehicleKind k, double x, double y, double z)
    : kind(k),
      body(world, x, y, z, k == VehicleKind::Boat ? BOAT_HALF_WIDTH : CART_HALF_WIDTH,
           k == VehicleKind::Boat ? BOAT_HEIGHT : CART_HEIGHT),
      prev(x, y, z),
      world_(world) {}

bool Vehicle::tick(const RideInput* rider) {
    prev = {body.x, body.y, body.z};
    prev_yaw = yaw;
    if (hurt_time > 0) --hurt_time;
    if (damage > 0) --damage;
    if (body.y < -64) return true;  // fell out of the world
    if (kind == VehicleKind::Boat) return tick_boat(rider);
    tick_cart();
    return false;
}

// Beta's EntityMinecart.onUpdate.
void Vehicle::tick_cart() {
    glm::dvec3& m = motion;
    if (kind == VehicleKind::PoweredMinecart) {
        if (fuel > 0) --fuel;
        if (fuel <= 0) push_x = push_z = 0;
    }
    m.y -= 0.04;
    const int i = ifloor(body.x), k = ifloor(body.z);
    int j = ifloor(body.y);
    if (is_rail(world_.get_block(i, j - 1, k))) --j;
    const BlockId b = world_.get_block(i, j, k);
    on_rail = is_rail(b);
    if (on_rail) {
        const auto before = point_on_rail(world_, {body.x, body.y, body.z});
        const int data = world_.get_data(i, j, k);
        const int shape = rail_shape(b, data);
        bool boost = false, brake = false;
        if (b == POWERED_RAIL) {
            boost = rail_powered(b, data);
            brake = !boost;
        }
        // On a slope the cart rides a block up while it moves, and rolls downhill.
        body.y = rail_sloped(shape) ? j + 1 : j;
        if (shape == RAIL_ASCENDING_EAST) m.x -= 0.0078125;
        if (shape == RAIL_ASCENDING_WEST) m.x += 0.0078125;
        if (shape == RAIL_ASCENDING_NORTH) m.z += 0.0078125;
        if (shape == RAIL_ASCENDING_SOUTH) m.z -= 0.0078125;
        // All the speed goes along the rail, whichever way the cart was already going.
        const auto& e = rail_ends(shape);
        double dx = e[1].x - e[0].x, dz = e[1].z - e[0].z;
        const double len = std::hypot(dx, dz);
        if (m.x * dx + m.z * dz < 0) dx = -dx, dz = -dz;
        double speed = std::hypot(m.x, m.z);
        m.x = speed * dx / len;
        m.z = speed * dz / len;
        if (brake) {  // a powered rail with no power stops carts
            if (std::hypot(m.x, m.z) < 0.03) {
                m = glm::dvec3(0);
            } else {
                m.x *= 0.5;
                m.y = 0;
                m.z *= 0.5;
            }
        }
        // Onto the line between the ends.
        const double x0 = i + 0.5 + e[0].x * 0.5, z0 = k + 0.5 + e[0].z * 0.5;
        const double lx = (i + 0.5 + e[1].x * 0.5) - x0, lz = (k + 0.5 + e[1].z * 0.5) - z0;
        double t;
        if (lx == 0) {
            body.x = i + 0.5;
            t = body.z - k;
        } else if (lz == 0) {
            body.z = k + 0.5;
            t = body.x - i;
        } else {
            t = ((body.x - x0) * lx + (body.z - z0) * lz) * 2;
        }
        body.x = x0 + lx * t;
        body.z = z0 + lz * t;
        double mx = m.x, mz = m.z;
        if (ridden) mx *= 0.75, mz *= 0.75;
        mx = std::clamp(mx, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
        mz = std::clamp(mz, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
        const HitAxes hit = body.move(mx, 0, mz);
        if (hit.x) m.x = 0;
        if (hit.z) m.z = 0;
        // Off the low end of a slope: down a block.
        for (const glm::ivec3& end : e)
            if (end.y != 0 && ifloor(body.x) - i == end.x && ifloor(body.z) - k == end.z) {
                body.y += end.y;
                break;
            }
        if (ridden) {
            m.x *= 0.997;
            m.y = 0;
            m.z *= 0.997;
        } else {
            if (kind == VehicleKind::PoweredMinecart) {
                const double d = std::hypot(push_x, push_z);
                if (d > 0.01) {
                    push_x /= d;
                    push_z /= d;
                    m.x = m.x * 0.8 + push_x * 0.04;
                    m.z = m.z * 0.8 + push_z * 0.04;
                } else {
                    m.x *= 0.9;
                    m.z *= 0.9;
                }
                m.y = 0;
            }
            m.x *= 0.96;
            m.y = 0;
            m.z *= 0.96;
        }
        // Going downhill speeds it up, uphill slows it down.
        const auto after = point_on_rail(world_, {body.x, body.y, body.z});
        if (after && before) {
            const double gain = (before->y - after->y) * 0.05;
            speed = std::hypot(m.x, m.z);
            if (speed > 0) {
                m.x = m.x / speed * (speed + gain);
                m.z = m.z / speed * (speed + gain);
            }
            body.y = after->y;
        }
        // Into the next cell: the speed turns that way (round a curve).
        const int ni = ifloor(body.x), nk = ifloor(body.z);
        if (ni != i || nk != k) {
            speed = std::hypot(m.x, m.z);
            m.x = speed * (ni - i);
            m.z = speed * (nk - k);
        }
        if (kind == VehicleKind::PoweredMinecart) {
            const double d = std::hypot(push_x, push_z);
            if (d > 0.01 && m.x * m.x + m.z * m.z > 0.001) {
                // It keeps pushing the way it is going, unless that is backwards.
                if (push_x * m.x + push_z * m.z < 0) {
                    push_x = push_z = 0;
                } else {
                    push_x = m.x;
                    push_z = m.z;
                }
            }
        }
        if (boost) {
            speed = std::hypot(m.x, m.z);
            if (speed > 0.01) {
                m.x += m.x / speed * 0.06;
                m.z += m.z / speed * 0.06;
            } else {
                // A cart standing on a powered rail against a wall is pushed away from it.
                auto solid = [&](int x, int z) { return is_solid(world_.get_block(x, j, z)); };
                if (shape == RAIL_EAST_WEST) {
                    if (solid(i - 1, k)) m.x = 0.02;
                    else if (solid(i + 1, k)) m.x = -0.02;
                } else if (shape == RAIL_NORTH_SOUTH) {
                    if (solid(i, k - 1)) m.z = 0.02;
                    else if (solid(i, k + 1)) m.z = -0.02;
                }
            }
        }
    } else {
        m.x = std::clamp(m.x, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
        m.z = std::clamp(m.z, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
        if (body.on_ground) m *= 0.5;
        const HitAxes hit = body.move(m.x, m.y, m.z);
        if (hit.x) m.x = 0;
        if (hit.y) m.y = 0;
        if (hit.z) m.z = 0;
        if (!body.on_ground) m *= 0.95;
    }
    // Face the way it moves (either end will do: carts look the same both ways round).
    const double mdx = body.x - prev.x, mdz = body.z - prev.z;
    if (mdx * mdx + mdz * mdz > 0.001 * 0.001) {
        double want = degrees(std::atan2(mdx, mdz));
        if (std::abs(wrap_degrees(want - yaw)) > 90) want += 180;
        yaw = yaw + wrap_degrees(want - yaw);
    }
}

// Beta's EntityBoat.onUpdate.
bool Vehicle::tick_boat(const RideInput* rider) {
    glm::dvec3& m = motion;
    // How much of the boat is in water, in fifths.
    double in_water = 0;
    for (int s = 0; s < 5; ++s) {
        const double y0 = body.y + body.height * s / 5 - 0.125, y1 = body.y + body.height * (s + 1) / 5 - 0.125;
        if (world_.box_in(WATER, body.x - body.half, y0, body.z - body.half, body.x + body.half - 1e-7, y1 - 1e-7,
                          body.z + body.half - 1e-7))
            in_water += 0.2;
    }
    if (in_water < 1) {
        m.y += 0.04 * (in_water * 2 - 1);
    } else {
        if (m.y < 0) m.y /= 2;
        m.y += 0.007;
    }
    if (rider && (rider->forward != 0 || rider->strafe != 0)) {
        // A fifth of what the rider's walking would do, the way they look (Beta).
        const double yr = radians(rider->yaw);
        double wx = std::sin(yr) * rider->forward - std::cos(yr) * rider->strafe;
        double wz = std::cos(yr) * rider->forward + std::sin(yr) * rider->strafe;
        const double l = std::hypot(wx, wz);
        m.x += wx / l * 0.1 * 0.2;
        m.z += wz / l * 0.1 * 0.2;
    }
    m.x = std::clamp(m.x, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
    m.z = std::clamp(m.z, -VEHICLE_MAX_SPEED, VEHICLE_MAX_SPEED);
    if (body.on_ground) m *= 0.5;
    const double speed = std::hypot(m.x, m.z);
    const HitAxes hit = body.move(m.x, m.y, m.z);
    if (hit.y) m.y = 0;
    if ((hit.x || hit.z) && speed > BOAT_CRASH_SPEED) return true;  // crashed: it breaks up
    if (hit.x) m.x = 0;
    if (hit.z) m.z = 0;
    m.x *= 0.99;
    m.y *= 0.95;
    m.z *= 0.99;
    // Turn towards where it is going, at most 20 degrees a tick.
    const double mdx = body.x - prev.x, mdz = body.z - prev.z;
    if (mdx * mdx + mdz * mdz > 0.001) yaw += std::clamp(wrap_degrees(degrees(std::atan2(mdx, mdz)) - yaw), -20.0, 20.0);
    return false;
}

bool Vehicle::hit(int amount) {
    rock_dir = -rock_dir;
    hurt_time = 10;
    damage += amount * 10;
    return damage > VEHICLE_BREAK_DAMAGE;
}

std::vector<Stack> Vehicle::drops() const {
    std::vector<Stack> out;
    switch (kind) {
        case VehicleKind::Boat:
            out.push_back({PLANKS, 3});
            out.push_back({STICK, 2});
            break;
        case VehicleKind::StorageMinecart:
            out.push_back({MINECART, 1});
            out.push_back({CHEST, 1});
            for (const auto& s : chest)
                if (s) out.push_back(*s);
            break;
        case VehicleKind::PoweredMinecart:
            out.push_back({MINECART, 1});
            out.push_back({FURNACE, 1});
            break;
        case VehicleKind::Minecart: out.push_back({MINECART, 1}); break;
    }
    return out;
}

bool Vehicle::add_fuel(ItemId item, double from_x, double from_z) {
    if (kind != VehicleKind::PoweredMinecart) return false;
    push_x = body.x - from_x;
    push_z = body.z - from_z;
    if (item != COAL && item != CHARCOAL) return false;
    fuel += COAL_FUEL_TICKS;
    return true;
}

void Vehicle::push_from(double x, double z) {
    double dx = x - body.x, dz = z - body.z;
    const double d = std::hypot(dx, dz);
    if (d < 0.01) return;
    const double k = std::min(1.0, 1.0 / d) * 0.05 / d;
    motion.x -= dx * k;
    motion.z -= dz * k;
}

void bump_carts(Vehicle& a, Vehicle& b) {
    // Beta's EntityMinecart.applyEntityCollision between two carts, when they are lined up.
    double dx = b.body.x - a.body.x, dz = b.body.z - a.body.z;
    const double d = std::hypot(dx, dz);
    if (d < 0.01) return;
    const double heading = radians(a.yaw);
    if (std::abs((dx * std::sin(heading) + dz * std::cos(heading)) / d) < 0.8) return;
    const double k = std::min(1.0, 1.0 / d) * 0.05 / d;
    const double px = dx * k, pz = dz * k;  // a little shove from a towards b
    const bool ap = a.kind == VehicleKind::PoweredMinecart, bp = b.kind == VehicleKind::PoweredMinecart;
    if (bp && !ap) {
        a.motion.x = a.motion.x * 0.2 + b.motion.x - px;
        a.motion.z = a.motion.z * 0.2 + b.motion.z - pz;
        b.motion.x *= 0.95;
        b.motion.z *= 0.95;
    } else if (ap && !bp) {
        b.motion.x = b.motion.x * 0.2 + a.motion.x + px;
        b.motion.z = b.motion.z * 0.2 + a.motion.z + pz;
        a.motion.x *= 0.95;
        a.motion.z *= 0.95;
    } else {
        const double mx = (a.motion.x + b.motion.x) / 2, mz = (a.motion.z + b.motion.z) / 2;
        a.motion.x = a.motion.x * 0.2 + mx - px;
        a.motion.z = a.motion.z * 0.2 + mz - pz;
        b.motion.x = b.motion.x * 0.2 + mx + px;
        b.motion.z = b.motion.z * 0.2 + mz + pz;
    }
}

glm::dvec3 Vehicle::position(double alpha) const { return prev + (glm::dvec3(body.x, body.y, body.z) - prev) * alpha; }

glm::dvec3 Vehicle::seat(double alpha) const {
    return position(alpha) + glm::dvec3(0, kind == VehicleKind::Boat ? BOAT_SEAT : CART_SEAT, 0);
}

std::optional<float> Vehicle::ray_hit(glm::vec3 origin, glm::vec3 direction, float max_dist) const {
    const glm::vec3 lo(body.x - body.half, body.y, body.z - body.half);
    const glm::vec3 hi(body.x + body.half, body.y + body.height, body.z + body.half);
    return ray_box(origin, direction, max_dist, lo, hi);
}

bool Vehicle::overlaps(const Body& o) const {
    return o.x + o.half > body.x - body.half && o.x - o.half < body.x + body.half && o.z + o.half > body.z - body.half &&
           o.z - o.half < body.z + body.half && o.y + o.height > body.y && o.y < body.y + body.height;
}

std::vector<ModelPart> Vehicle::parts(double alpha) const {
    const glm::dvec3 p = position(alpha);
    double y = yaw;
    const double dy = wrap_degrees(yaw - prev_yaw);
    y = prev_yaw + dy * alpha;
    // Tipped up or down a slope, and rocking for a moment after a hit (Beta's RenderMinecart and RenderBoat).
    double pitch = 0;
    if (is_cart()) {
        const glm::dvec3 f(std::sin(radians(y)) * 0.3, 0, std::cos(radians(y)) * 0.3);
        auto front = point_on_rail(world_, p + f), back = point_on_rail(world_, p - f);
        if (front && back) pitch = degrees(std::atan2(back->y - front->y, 0.6));
    }
    double rock = 0;
    if (hurt_time > 0) rock = std::sin(static_cast<double>(hurt_time)) * hurt_time * std::max(0, damage) / 10.0 * rock_dir;
    const glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(p)) * rotate(y, {0, 1, 0}) * rotate(pitch, {1, 0, 0}) *
                           rotate(rock, {0, 0, 1});
    std::vector<ModelPart> out;
    if (kind == VehicleKind::Boat) {
        out.push_back({MODEL_BOAT, root});
        return out;
    }
    out.push_back({MODEL_MINECART, root});
    if (kind == VehicleKind::StorageMinecart || kind == VehicleKind::PoweredMinecart) {
        // The chest or furnace sits in the cart: the dropped-item cube (0.25 across) made 0.75 across.
        const ItemId inside = kind == VehicleKind::StorageMinecart ? CHEST : FURNACE;
        out.push_back({ITEM_MODEL_BASE + inside, root * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.5f, 0)) *
                                                     glm::scale(glm::mat4(1.0f), glm::vec3(3.0f))});
    }
    return out;
}

EntityRecord Vehicle::record() const {
    EntityRecord e;
    e.kind = EntityRecord::VEHICLE;
    e.x = body.x;
    e.y = body.y;
    e.z = body.z;
    e.yaw = static_cast<float>(yaw);
    e.mob = static_cast<int>(kind);
    e.fuel = fuel;
    e.push_x = static_cast<float>(push_x);
    e.push_z = static_cast<float>(push_z);
    if (kind == VehicleKind::StorageMinecart) e.chest.assign(chest.begin(), chest.end());
    return e;
}

std::unique_ptr<Vehicle> Vehicle::from_record(World& world, const EntityRecord& e) {
    if (e.mob < 0 || e.mob >= VEHICLE_KIND_COUNT) return nullptr;
    auto v = std::make_unique<Vehicle>(world, static_cast<VehicleKind>(e.mob), e.x, e.y, e.z);
    v->yaw = v->prev_yaw = e.yaw;
    if (v->kind == VehicleKind::PoweredMinecart) {
        v->fuel = std::max(0, e.fuel);
        v->push_x = e.push_x;
        v->push_z = e.push_z;
    }
    if (v->kind == VehicleKind::StorageMinecart)
        for (size_t i = 0; i < e.chest.size() && i < v->chest.size(); ++i) v->chest[i] = e.chest[i];
    return v;
}

void add_vehicle_models(std::map<int, MeshData>& models) {
    const glm::mat4 ident(1.0f);
    // A minecart: an iron tub 1.1 long and 0.85 wide, open on top.
    MeshData& cart = models[MODEL_MINECART];
    add_box(cart, ident, {0.85f, 0.1f, 1.1f}, {0, 0.15f, 0}, T_CART);
    for (float side : {-1.0f, 1.0f}) {
        add_box(cart, ident, {0.0625f, 0.5f, 1.1f}, {side * 0.394f, 0.45f, 0}, T_CART, 0.9f);
        add_box(cart, ident, {0.725f, 0.5f, 0.0625f}, {0, 0.45f, side * 0.519f}, T_CART, 0.8f);
    }
    // Axles under it.
    for (float end : {-0.32f, 0.32f}) add_box(cart, ident, {0.9f, 0.08f, 0.08f}, {0, 0.06f, end}, T_SHOE);
    // A boat: a planked hull 1.5 long and 1 wide.
    MeshData& boat = models[MODEL_BOAT];
    add_box(boat, ident, {1.0f, 0.12f, 1.5f}, {0, 0.06f, 0}, T_PLANKS);
    for (float side : {-1.0f, 1.0f}) {
        add_box(boat, ident, {0.0625f, 0.4f, 1.5f}, {side * 0.469f, 0.32f, 0}, T_PLANKS, 0.9f);
        add_box(boat, ident, {0.875f, 0.4f, 0.0625f}, {0, 0.32f, side * 0.719f}, T_PLANKS, 0.8f);
    }
}
