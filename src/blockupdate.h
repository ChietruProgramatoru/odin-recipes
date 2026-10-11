// Blocks that change by themselves (M6): flowing water and lava, falling sand and gravel, fire
// spreading, and (part 2, rules in nature.h) leaves decaying, ice and snow melting, snow settling,
// water freezing and cactus growing. Beta's block ticks: a block that changes tells its six neighbours, which can ask
// for a tick a few game ticks later; chunks near the player also get random ticks (lava starting
// fires, fire burning on). Pure logic on the World, no graphics.
#pragma once
#include <array>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "physics.h"
#include "redstone.h"
#include "rng.h"
#include "world.h"

// Game ticks between updates (Beta's tick rates).
constexpr int WATER_TICKS = 5;
constexpr int LAVA_TICKS = 30;
constexpr int FALL_TICKS = 3;
constexpr int FIRE_TICKS = 40;
constexpr int RANDOM_TICKS_PER_CHUNK = 80;  // Beta picks 80 random cells in each chunk near a player every tick
constexpr int MAX_TICKS_PER_TICK = 1000;    // Beta runs at most this many due block ticks per game tick

// What the game has to do after a tick of block updates.
struct BlockEvents {
    std::vector<ChunkPos> dirty;                             // meshes to rebuild
    std::vector<std::pair<glm::ivec3, ItemId>> drops;        // things water washed away (torches)
    std::vector<glm::ivec3> fizz;                            // lava met water (the block is already set)
    std::vector<std::pair<glm::ivec3, BlockId>> falling;     // sand or gravel let go (the block is already gone)
    std::vector<glm::ivec3> tnt;                             // TNT the fire reached (the block is already gone)
    RedstoneEvents redstone;                                 // what the circuits did (M13)
    void rebuild(const std::vector<ChunkPos>& chunks);
};

// Is rain falling on this cell right now? Fire there goes out (Beta's canLightningStrikeAt).
using RainCheck = std::function<bool(glm::ivec3)>;

// Fire (Beta's BlockFire tables): how much a block helps fire spread next to it, and how easily it
// catches fire and burns away. Both 0 for blocks that don't burn.
int fire_spread_chance(BlockId b);
int burn_chance(BlockId b);
bool is_flammable(BlockId b);
// Fire can stay at a cell: solid ground under it, or something next to it that burns.
bool fire_can_stay(World& world, glm::ivec3 p);

// Fluids, as Beta: a fluid's level at a cell (its data value) or -1 if the cell holds no `fluid`.
int fluid_level(World& world, glm::ivec3 p, BlockId fluid);
// Which way the water (or lava) at a cell is flowing, normalised (zero when still). Currents push
// players, mobs and items this way (Beta's BlockFluid.getFlowVector).
glm::dvec3 flow_vector(World& world, glm::ivec3 p);
// Is a point below the surface of a fluid (Beta's Entity.isInsideOfMaterial)?
bool under_fluid(World& world, glm::dvec3 p, BlockId fluid);
// Can sand or gravel fall into this block?
bool can_fall_into(BlockId b);

class BlockUpdater {
public:
    explicit BlockUpdater(World& world) : world_(world), redstone_(world) {}

    // Redstone runs inside the block ticks (it sees every change); the game tells it who stands on
    // pressure plates, and flips levers and presses buttons through it.
    Redstone& redstone() { return redstone_; }
    void set_plate_sensor(PlateSensor sensor) { plate_sensor_ = std::move(sensor); }

    // Ask for a block tick at a cell `delay` game ticks from now (once: a cell already waiting
    // keeps its time). The tick only runs if the cell still holds the same block.
    void schedule(glm::ivec3 pos, int delay);
    bool scheduled(glm::ivec3 pos) const { return waiting_.count(key(pos)) > 0; }
    size_t pending() const { return queue_.size(); }
    long now() const { return now_; }

    // One game tick: blocks around every cell set since the last tick react to it, then the block
    // ticks that are due run.
    void tick(Rng& rng, const RainCheck& rain, BlockEvents& out);
    // Random ticks in the chunks within `radius` of a chunk (lava lighting fires, fire burning,
    // leaves decaying, ice and snow melting, cactus growing).
    void random_ticks(ChunkPos center, int radius, Rng& rng, const RainCheck& rain, BlockEvents& out);
    // Cold biomes within `radius` (Beta's World.tickBlocks): now and then still water at the top of a
    // column freezes, and while it snows (`snowing`: it is raining) a snow layer settles there.
    void snow_ticks(ChunkPos center, int radius, bool snowing, BlockEvents& out);
    // Start up the blocks of a chunk that were busy when it was saved: fires, water or lava
    // that was flowing, pressed buttons and plates (block ticks aren't saved).
    void wake_chunk(const Chunk& c);
    // Forget every waiting tick (the world was swapped for another dimension).
    void clear() {
        queue_.clear();
        waiting_.clear();
    }

private:
    using Key = std::tuple<int, int, int>;
    static Key key(glm::ivec3 p) { return {p.x, p.y, p.z}; }

    void changed(glm::ivec3 p, BlockEvents& out);
    void neighbour_changed(glm::ivec3 p, BlockEvents& out);
    void run(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out);
    void tick_fluid(glm::ivec3 p, BlockId fluid, Rng& rng, BlockEvents& out);
    void tick_fire(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out);
    void tick_falling(glm::ivec3 p, BlockEvents& out);
    void random_tick(glm::ivec3 p, Rng& rng, const RainCheck& rain, BlockEvents& out);
    void decay(glm::ivec3 p, BlockEvents& out);  // leaves marked LEAVES_CHECK
    void grow_cactus(glm::ivec3 p, BlockEvents& out);

    // Fluid helpers (Beta's BlockFlowing).
    bool harden(glm::ivec3 p, BlockEvents& out);  // lava next to water turns to obsidian or cobblestone
    bool blocks_flow(glm::ivec3 p);
    bool can_displace(glm::ivec3 p, BlockId fluid);
    void flow_into(glm::ivec3 p, BlockId fluid, int level, BlockEvents& out);
    int flow_cost(glm::ivec3 p, BlockId fluid, int depth, int from);
    std::array<bool, 4> flow_directions(glm::ivec3 p, BlockId fluid);
    void catch_fire(glm::ivec3 p, int chance, int age, Rng& rng, const RainCheck& rain, BlockEvents& out);

    void set(glm::ivec3 p, BlockId b, int data, BlockEvents& out);
    // The chunks around a cell are all there, so updating it won't generate new chunks.
    bool area_loaded(glm::ivec3 p);

    World& world_;
    Redstone redstone_;
    PlateSensor plate_sensor_;
    // Its own for snow and leaf decay, so they don't change the random numbers fire and the rest get.
    Rng nature_rng_{0x5A0F};
    long now_ = 0;
    std::set<std::tuple<long, int, int, int>> queue_;  // (due tick, x, y, z), in the order they run
    std::map<Key, std::pair<long, BlockId>> waiting_;   // cell -> (due tick, block it was for)
};

// Sand or gravel falling (Beta's EntityFallingSand): a block-sized body with gravity that turns
// back into the block where it lands.
class FallingBlock {
public:
    FallingBlock(World& world, BlockId block, glm::ivec3 from);
    BlockId block;
    Body body;
    double age = 0;

    // Fall for dt seconds. Returns true once it has landed (or fallen for too long).
    bool tick(double dt);
    // The cell it ends up in.
    glm::ivec3 cell() const;
    // Can it become a block there (or does it drop as an item)?
    bool can_land(World& world) const;
};
