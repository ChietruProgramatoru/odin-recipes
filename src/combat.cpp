#include "combat.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

#include "world.h"

// --- Difficulty --------------------------------------------------------------------------------

const char* difficulty_name(int difficulty) {
    static const char* NAMES[DIFFICULTY_COUNT] = {"Peaceful", "Easy", "Normal", "Hard"};
    return NAMES[std::clamp(difficulty, 0, DIFFICULTY_COUNT - 1)];
}

int scale_monster_damage(int amount, int difficulty) {
    switch (difficulty) {
        case PEACEFUL: return 0;
        case EASY: return amount / 3 + 1;
        case HARD: return amount * 3 / 2;
        default: return amount;
    }
}

// --- Being hurt -------------------------------------------------------------------------------

int HurtCooldown::take(int amount, bool& fresh) {
    fresh = false;
    if (amount <= 0) return 0;
    if (invulnerable()) {
        if (amount <= last_) return 0;
        int extra = amount - last_;
        last_ = amount;
        return extra;
    }
    fresh = true;
    last_ = amount;
    timer_ = LENGTH;
    return amount;
}

// --- Monsters -----------------------------------------------------------------------------------

const char* mob_name(MobKind kind) {
    static const char* NAMES[MOB_KIND_COUNT] = {"Zombie", "Skeleton", "Creeper", "Spider", "Slime"};
    return NAMES[static_cast<int>(kind)];
}

MobInfo mob_info(MobKind kind, int size) {
    // Beta's walking speeds are fractions of the player's (zombies 0.5, spiders 0.8, the rest 0.7).
    switch (kind) {
        case MobKind::Zombie: return {20, 2.15, 0.3, 1.8, 5, true};
        case MobKind::Skeleton: return {20, 3.0, 0.3, 1.8, 0, true};
        case MobKind::Creeper: return {20, 3.0, 0.3, 1.7, 0, false};
        case MobKind::Spider: return {16, 3.45, 0.7, 0.9, 2, false};
        case MobKind::Slime: {
            const double half = 0.3 * size;
            // Only slimes bigger than the smallest hurt you, by their size.
            return {size * size, 1.0 + size * 0.5, half, half * 2, size > 1 ? size : 0, false};
        }
    }
    return {20, 3.0, 0.3, 1.8, 0, false};
}

std::vector<ItemId> mob_drops(MobKind kind, int size, Rng& rng) {
    std::vector<ItemId> out;
    auto some = [&](ItemId item) {
        for (int n = rng.below(3); n > 0; --n) out.push_back(item);
    };
    switch (kind) {
        case MobKind::Zombie: some(FEATHER); break;
        case MobKind::Skeleton: some(ARROW); some(BONE); break;
        case MobKind::Creeper: some(GUNPOWDER); break;
        case MobKind::Spider: some(STRING); break;
        case MobKind::Slime: if (size == 1) some(SLIMEBALL); break;
    }
    return out;
}

int slime_split_count(int size, Rng& rng) { return size > 1 ? 2 + rng.below(3) : 0; }

double light_brightness(int level) {
    const double f = 1.0 - std::clamp(level, 0, 15) / 15.0;
    return (1.0 - f) / (f * 3.0 + 1.0) * 0.95 + 0.05;
}

bool spider_hostile(int light) { return light_brightness(light) < 0.5; }

bool catches_fire_in_daylight(int light, bool sees_sky, int sky_darkness, Rng& rng) {
    const double f = light_brightness(light);
    if (sky_darkness >= 4 || f <= 0.5 || !sees_sky) return false;
    return rng.random() * 30.0 < (f - 0.4) * 2.0;
}

// --- Spawning -----------------------------------------------------------------------------------

bool monster_spawn_light_ok(int raw_sky, int light, Rng& rng) {
    if (raw_sky > rng.below(32)) return false;
    return light <= rng.below(8);
}

int monster_cap(int chunks) { return 70 * chunks / 256; }

MobKind random_monster(Rng& rng) { return static_cast<MobKind>(rng.below(MOB_KIND_COUNT)); }

bool is_slime_chunk(uint64_t seed, int cx, int cz) {
    Rng rng(seed ^ (static_cast<uint64_t>(static_cast<uint32_t>(cx)) * 0x9E3779B97F4A7C15ull) ^
            (static_cast<uint64_t>(static_cast<uint32_t>(cz)) * 0xC2B2AE3D27D4EB4Full) ^ 987234911ull);
    return rng.below(10) == 0;
}

int random_slime_size(Rng& rng) { return 1 << rng.below(3); }

bool spawns_as_jockey(Rng& rng) { return rng.below(100) == 0; }

// --- Mob spawners --------------------------------------------------------------------------------

MobKind spawner_mob(uint64_t seed, glm::ivec3 pos) {
    int pick = static_cast<int>(hash01(pos.x, pos.z, 4000 + pos.y, seed) * 4);
    return pick == 0 ? MobKind::Skeleton : pick == 3 ? MobKind::Spider : MobKind::Zombie;
}

int spawner_delay(Rng& rng) { return 200 + rng.below(600); }

// --- Explosions -----------------------------------------------------------------------------------

float explosion_resistance(uint8_t block) {
    switch (block) {
        case AIR: return 0;
        case GLASS: return 0.3f;
        case STONE: case COBBLE: case BRICK: case MOSSY_COBBLE: return 6;
        case PLANKS: return 3;
        case COAL_ORE: case IRON_ORE: case GOLD_ORE: case DIAMOND_ORE: case REDSTONE_ORE: case LAPIS_ORE: return 3;
        case WATER: case LAVA: return 100;
        case OBSIDIAN: return 1200;
        case BEDROCK: return 3600000;
        default: break;
    }
    const BlockInfo* info = block_info(block);
    return info && info->hardness ? *info->hardness : 3600000;
}

std::vector<glm::ivec3> explosion_blocks(World& world, glm::dvec3 center, double power, Rng& rng) {
    std::set<std::tuple<int, int, int>> seen;
    std::vector<glm::ivec3> out;
    const int n = 16;
    const double step = 0.3;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) {
                if (i != 0 && i != n - 1 && j != 0 && j != n - 1 && k != 0 && k != n - 1) continue;
                glm::dvec3 dir(i / (n - 1.0) * 2 - 1, j / (n - 1.0) * 2 - 1, k / (n - 1.0) * 2 - 1);
                dir = glm::normalize(dir);
                double strength = power * (0.7 + rng.random() * 0.6);
                glm::dvec3 pos = center;
                for (; strength > 0; strength -= step * 0.75) {
                    const glm::ivec3 cell(static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y)),
                                          static_cast<int>(std::floor(pos.z)));
                    if (cell.y < 0 || cell.y >= HEIGHT) break;
                    const BlockId b = world.get_block(cell.x, cell.y, cell.z);
                    if (b != AIR) strength -= (explosion_resistance(b) + 0.3) * step;
                    if (strength > 0 && b != AIR && seen.insert({cell.x, cell.y, cell.z}).second) out.push_back(cell);
                    pos += dir * step;
                }
            }
    return out;
}

double explosion_exposure(World& world, glm::dvec3 center, glm::dvec3 lo, glm::dvec3 hi) {
    const glm::dvec3 size = hi - lo;
    const glm::dvec3 step(1.0 / (size.x * 2 + 1), 1.0 / (size.y * 2 + 1), 1.0 / (size.z * 2 + 1));
    int open = 0, total = 0;
    for (double fx = 0; fx <= 1; fx += step.x)
        for (double fy = 0; fy <= 1; fy += step.y)
            for (double fz = 0; fz <= 1; fz += step.z) {
                const glm::dvec3 p = lo + size * glm::dvec3(fx, fy, fz);
                const glm::dvec3 to = center - p;
                const double d = glm::length(to);
                ++total;
                // Stop just short of the centre: a blast at a mob's feet sits right on the ground's top face.
                if (d < 0.01 || !world.raycast(glm::vec3(p), glm::vec3(to / d), static_cast<float>(d - 0.01), is_solid)) ++open;
            }
    return total ? static_cast<double>(open) / total : 0.0;
}

namespace {
// Beta's impact: 1 at the centre down to 0 at twice the power, times the exposure.
double impact(double power, double distance, double exposure) {
    const double reach = power * 2;
    const double d = distance / reach;
    if (d > 1) return 0;
    return (1 - d) * exposure;
}
}  // namespace

int explosion_damage(double power, double distance, double exposure) {
    if (distance > power * 2) return 0;
    const double i = impact(power, distance, exposure);
    return static_cast<int>((i * i + i) / 2 * 8 * power * 2 + 1);
}

double explosion_push(double power, double distance, double exposure) { return impact(power, distance, exposure); }
