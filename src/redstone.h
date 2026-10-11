// Redstone (M13), as Beta 1.7.3: dust that carries power 15 blocks, torches that invert, levers,
// buttons, pressure plates, detector rails, repeaters, and what power does (doors and trapdoors,
// TNT, note blocks, dispensers, powered rails, pistons). Pure logic on the World, no graphics.
//
// Power, like Beta: a source gives power to the cells next to it, and some sources also power a
// solid block "strongly" (a torch the block above it, a lever or button the block it is on, a
// plate or a detector rail the block under it, a repeater the block in front of it, dust the block
// under it and the one it points into). A strongly powered block powers everything next to it
// in turn, except that a block powered only by dust doesn't power more dust.
#pragma once
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <glm/vec3.hpp>

#include "building.h"
#include "world.h"

// --- Data values -----------------------------------------------------------------------------
// Facings (f) are building.h's (0 south +z, 1 west -x, 2 north -z, 3 east +x).
//   REDSTONE_WIRE: its power, 0 .. 15.
//   REDSTONE_TORCH_ON/OFF, LEVER_BLOCK (bits 0-2): 0 standing on the block below, 1 + f on the wall
//     on side f. LEVER_ON (8): the lever is down.
//   BUTTON_BLOCK: bits 0-1 the side its wall is on, BUTTON_PRESSED (8).
//   STONE_PLATE_BLOCK, WOOD_PLATE_BLOCK: PLATE_PRESSED (1).
//   REPEATER_OFF/ON: bits 0-1 the side it sends power out of, bits 2-3 its delay less one (0 .. 3).
//   PISTON_BASE, STICKY_PISTON_BASE: bits 0-2 the way it pushes (a DIR6), PISTON_EXTENDED (8).
//   PISTON_HEAD: bits 0-2 the way it points, PISTON_HEAD_STICKY (8).
//   DISPENSER_BLOCK: bits 0-1 the side its front is on.
constexpr int LEVER_ON = 8, BUTTON_PRESSED = 8, PLATE_PRESSED = 1;
constexpr int PISTON_EXTENDED = 8, PISTON_HEAD_STICKY = 8;
constexpr int MAX_POWER = 15;
constexpr int PISTON_PUSH_LIMIT = 12;  // blocks a piston can push (Beta)

// Game ticks (Beta's tick rates): a torch turns over 2 ticks after its block's power changes, a
// repeater waits 2 ticks per delay step, a button springs back after 20, a plate is checked every
// 20, a dispenser fires 4 after it is powered, glowing ore fades at random.
constexpr int TORCH_TICKS = 2, REPEATER_STEP_TICKS = 2, BUTTON_TICKS = 20, PLATE_TICKS = 20, DISPENSER_TICKS = 4;

// The six directions, Beta's order for pistons: 0 down, 1 up, 2 north (-z), 3 south (+z), 4 west (-x), 5 east (+x).
glm::ivec3 dir6(int d);
int dir6_of(glm::ivec3 v);  // -1 if v isn't one of them

bool is_redstone_torch(BlockId b);
bool is_repeater(BlockId b);
bool is_plate(BlockId b);
bool is_piston(BlockId b);  // a piston's base (sticky or not)
// Shaped blocks of redstone (building.h's shapes): dust, levers, buttons, plates, repeaters and the
// piston's head. Dust, torches, levers, buttons and plates are also thin (nothing to bump into).
bool is_redstone_shaped(BlockId b);
bool is_redstone_thin(BlockId b);
// Gives power by itself (a torch, lever, button, plate, repeater, detector rail) or carries it (dust):
// what dust joins to, and the blocks whose changes power things react to.
bool is_redstone_component(BlockId b);
// Items placed by redstone_placement: dust, the torch, lever, button, plates, repeater, pistons, dispenser.
bool is_redstone_item(ItemId item);

int repeater_delay(int data);  // 1 .. 4 steps
int repeater_facing(int data);

// The boxes of a redstone shape, like building.h's shape_boxes (the dust's depend on nothing).
std::vector<Box> redstone_boxes(BlockId block, int data, bool collide);

// The offset from a torch, lever, button, plate, dust or repeater to the block holding it up.
glm::ivec3 redstone_support(BlockId block, int data);
// Can it stay where it is (is the block holding it up still there)?
bool redstone_can_stay(World& world, glm::ivec3 pos);

// --- Placing and using -------------------------------------------------------------------------

// What right-clicking face `face` of the block at `clicked` with a redstone item places, looking
// along yaw and pitch (degrees, as Game has them). Pistons and dispensers face the player.
std::optional<PlacedCell> redstone_placement(World& world, ItemId item, glm::ivec3 clicked, glm::ivec3 face, double yaw,
                                             double pitch);
// The item a redstone block drops (the piston's head drops its piston), or nothing for others.
std::optional<ItemId> redstone_drop(BlockId block, int data);
// The piston's base and its head go together (breaking either takes both).
std::vector<glm::ivec3> piston_cells(World& world, glm::ivec3 pos);

// --- Power ---------------------------------------------------------------------------------------

// Dust at `pos` joins towards side `d` (one of the four sideways directions): other dust there (or a
// step up or down from there), or something that gives power.
bool wire_joins(World& world, glm::ivec3 pos, glm::ivec3 d);
// Dust sends its power out sideways along its line: to side d if it joins along d's axis and not
// across it, or if it joins nothing at all (a dot powers all four sides).
bool wire_points(World& world, glm::ivec3 pos, glm::ivec3 d);

// The power the block at `from` gives straight into its neighbour `to` (0 .. 15). Dust only counts
// with `wires`.
int power_from(World& world, glm::ivec3 from, glm::ivec3 to, bool wires);
// How strongly the solid block at `pos` is powered by what points into it (0 .. 15).
int strong_power(World& world, glm::ivec3 pos, bool wires);
// The power reaching a cell from its six neighbours, straight or through a strongly powered block
// (Beta's isBlockIndirectlyGettingPowered). `except`: a side to leave out (a piston's front).
int power_into(World& world, glm::ivec3 pos, bool wires = true, std::optional<glm::ivec3> except = std::nullopt);
inline bool is_powered(World& world, glm::ivec3 pos) { return power_into(world, pos) > 0; }
// Should a powered rail be powered: power reaches it, or a powered rail in line with it within 8
// rails is (Beta).
bool rail_gets_power(World& world, glm::ivec3 pos);

// --- Pistons -------------------------------------------------------------------------------------

// What a piston moving did: the cells blocks (and the head) moved into, the way they moved, and the
// blocks broken off on the way (with what they drop).
struct PistonMove {
    glm::ivec3 piston{0};
    bool extended = false;  // pushed out (or pulled back)
    std::vector<glm::ivec3> moved_to;
    glm::ivec3 dir{0};
    std::vector<std::pair<glm::ivec3, ItemId>> drops;
    std::vector<ChunkPos> dirty;
};
// Push out: up to 12 blocks in front move one cell along, the head goes in front. Fails (returns
// false, nothing changes) past 12 blocks, at the top or bottom of the world, or against something
// that doesn't move (obsidian, bedrock, chests and the like, an extended piston).
bool extend_piston(World& world, glm::ivec3 pos, PistonMove& out);
// Pull back: the head goes, and a sticky piston pulls the block in front of its head back with it.
void retract_piston(World& world, glm::ivec3 pos, PistonMove& out);

// --- The ticking -----------------------------------------------------------------------------------

// What redstone wants the game to do after a tick.
struct RedstoneEvents {
    std::vector<glm::ivec3> notes;      // note blocks that were just powered
    std::vector<glm::ivec3> dispense;   // dispensers firing
    std::vector<glm::ivec3> clicks;     // a lever, button or plate went on or off
    std::vector<glm::ivec3> tnt;        // TNT powered (the block is already gone)
    std::vector<PistonMove> pistons;    // pistons that moved
    std::vector<std::pair<glm::ivec3, ItemId>> drops;  // dust, torches and the like whose block went
};

// Is something standing on this plate? (The game knows where the players, mobs and items are.)
using PlateSensor = std::function<bool(glm::ivec3, BlockId)>;

// Keeps circuits running a game tick at a time: every cell changed since the last tick has the dust
// around it worked out again (all at once, so it never runs away) and the things around it check
// their power; torches, repeaters, buttons, plates and dispensers wait their ticks. At most
// MAX_REDSTONE_TICKS run per game tick, so a clock can't freeze the game.
class Redstone {
public:
    static constexpr int MAX_REDSTONE_TICKS = 1000;
    explicit Redstone(World& world) : world_(world) {}

    // A cell changed (BlockUpdater passes on every change).
    void changed(glm::ivec3 p) { changed_.push_back(p); }
    // One game tick at time `now`: react to the changes, then run the ticks that are due. The
    // events's dirty chunks go to `dirty`.
    void tick(long now, const PlateSensor& sensor, RedstoneEvents& out, std::vector<ChunkPos>& dirty);
    // Wait `delay` ticks, then look at the cell again.
    void schedule(glm::ivec3 p, long due);
    bool scheduled(glm::ivec3 p) const { return waiting_.count({p.x, p.y, p.z}) > 0; }

    // Right click: flip a lever, press a button, step a repeater's delay. False for other blocks.
    bool use(glm::ivec3 p, long now, std::vector<ChunkPos>& dirty, RedstoneEvents& out);
    // Something stepped on a plate: it goes down (if it wasn't) and is checked again in a second.
    void press_plate(glm::ivec3 p, long now, std::vector<ChunkPos>& dirty, RedstoneEvents& out);
    // A chunk came in: start up its pressed buttons and plates and its glowing ore again (scheduled
    // ticks aren't saved).
    void wake_chunk(const Chunk& c, long now);

private:
    using Key = std::tuple<int, int, int>;
    void update_wires(const std::vector<glm::ivec3>& around, std::vector<ChunkPos>& dirty);
    void check(glm::ivec3 p, long now, RedstoneEvents& out, std::vector<ChunkPos>& dirty);
    void run(glm::ivec3 p, long now, const PlateSensor& sensor, RedstoneEvents& out, std::vector<ChunkPos>& dirty);
    void set(glm::ivec3 p, BlockId b, int data, std::vector<ChunkPos>& dirty);
    void set_data(glm::ivec3 p, int data, std::vector<ChunkPos>& dirty);

    World& world_;
    std::vector<glm::ivec3> changed_;
    std::set<std::tuple<long, int, int, int>> queue_;
    std::map<Key, long> waiting_;
    // Doors, trapdoors, note blocks and dispensers act when their power comes on (or goes): which of
    // them had power when last looked at. Not saved; they look again when something changes.
    std::set<Key> powered_;
};
