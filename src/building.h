// Building blocks (M11): the shapes of slabs, stairs, fences, ladders, doors, trapdoors, signs and
// paintings; where their items go; what they drop; doors and trapdoors opening; and the things
// that fall off when the block holding them goes. Beta 1.7.3's rules. No graphics.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "blocks.h"
#include "items.h"

class World;

// A box inside a block cell, in 0..1 (a fence's reaches 1.5 up, so nothing jumps over it).
struct Box {
    glm::vec3 lo, hi;
    bool operator==(const Box&) const = default;
};

// --- Data values -----------------------------------------------------------------------------
// Facings are Beta's order (FACING_DIRS in bed.h: 0 south +z, 1 west -x, 2 north -z, 3 east +x).
//   WOOL: the colour, 0 white .. 15 black (the wool item is WHITE_WOOL + colour).
//   SLAB, DOUBLE_SLAB: the kind, 0 stone, 1 sandstone, 2 wood, 3 cobblestone (STONE_SLAB + kind).
//   WOOD_STAIRS, COBBLE_STAIRS: the side the tall back is on (the way the player looked placing them).
//   LADDER, WALL_SIGN, PAINTING_BLOCK: the side the wall holding it is on.
//   SIGN_POST: which way it faces, in 16ths of a turn (0 south, 4 west, 8 north, 12 east).
//   Doors: bits 0-1 the way the player looked placing it, DOOR_OPEN, DOOR_UPPER for the top half.
//   TRAPDOOR: bits 0-1 the side its hinge (the wall holding it) is on, TRAPDOOR_OPEN.
//   CHEST: the side its front faces.
constexpr int DOOR_OPEN = 4, DOOR_UPPER = 8;
constexpr int TRAPDOOR_OPEN = 4;
constexpr int SLAB_KINDS = 4;

// The side of a cell a facing points to, as a unit offset.
glm::ivec3 facing_dir(int facing);
// The facing for a unit offset along x or z (-1 for anything else).
int facing_of(glm::ivec3 dir);

// Fences join the fences beside them (Beta 1.7.3: only fences), in the order of facing_dir.
using FenceLinks = std::array<bool, 4>;
FenceLinks fence_links(World& world, glm::ivec3 pos);

// The boxes of a shaped block. `collide`: what bodies bump into (none for signs and paintings,
// a fence 1.5 high); otherwise the boxes you aim at (and the outline drawn around them).
std::vector<Box> shape_boxes(BlockId block, int data, const FenceLinks& links, bool collide);
// The same for the block at a cell (any block: a full cube, a bed's or cake's lower box, or
// nothing for air, fluids and things with no box).
std::vector<Box> block_boxes(World& world, glm::ivec3 pos, bool collide);

// Where a ray (from `origin` along `dir`, in the cell's own coordinates) first meets one of the
// boxes, as a distance along the ray; nothing if it misses them all. `face` (if given) is set to the
// outward side of the box it came in through (all zero if the ray starts inside a box).
std::optional<float> ray_boxes(const std::vector<Box>& boxes, glm::vec3 origin, glm::vec3 dir, glm::ivec3* face = nullptr);

// --- Placing ---------------------------------------------------------------------------------

struct PlacedCell {
    glm::ivec3 pos;
    BlockId block;
    int data = 0;
    bool operator==(const PlacedCell&) const = default;
};

// What right-clicking the face `face` (outward, from the clicked block) of the block at `clicked`
// with `item` places, looking along `yaw` (degrees, as Game uses it). Covers wool, slabs (two of
// a kind make a double slab), stairs, fences, ladders, doors (two cells), trapdoors, signs, chests
// and every plain block. Nothing if the item can't go there (no support, no room, three chests in a
// row...). Paintings are placed by painting.h.
std::optional<std::vector<PlacedCell>> placement(World& world, ItemId item, glm::ivec3 clicked, glm::ivec3 face, double yaw);

// Can a block (with its data) be put in this cell: is it free (air, a fluid, fire, tall grass) and
// does what holds it up stand? Placing makes this check; so do the neighbour updates (fall_off).
bool cell_free(World& world, glm::ivec3 pos);
bool supported(World& world, glm::ivec3 pos, BlockId block, int data);

// --- Using ---------------------------------------------------------------------------------

// Right-clicking a wooden door or a trapdoor opens or shuts it (both halves of a door). Iron doors
// only open with redstone (M13). Returns the cells changed, or nothing if it isn't one of those.
std::optional<std::vector<PlacedCell>> toggle_door(World& world, glm::ivec3 pos);
bool door_open(int data);

// The other half of the door at `pos`.
glm::ivec3 door_other_half(glm::ivec3 pos, int data);

// --- Breaking ------------------------------------------------------------------------------

// The items a block drops when broken by something that can harvest it (tools.h decides that), for
// blocks whose drop depends on the data value or isn't the block: wool's colour, a slab's kind (two
// from a double slab), the door, sign or painting item. Nothing for the others, which drop their
// BlockInfo drop as before.
std::optional<std::vector<ItemId>> data_drops(BlockId block, int data);

// Cells that go together: both halves of a door, or every cell of a painting (painting.h finds
// those). Breaking one breaks the rest, and only one item drops.
std::vector<glm::ivec3> joined_cells(World& world, glm::ivec3 pos);

// After the block at `pos` changed, the neighbours that can't stay any more (a ladder whose wall
// went, a door with nothing under it, a sign...): each one's cell and what it drops. The caller
// removes them (and every cell joined to them).
std::vector<std::pair<glm::ivec3, ItemId>> fall_off(World& world, glm::ivec3 pos);

// --- Note blocks ---------------------------------------------------------------------------

// The instrument a note block plays, from the block under it (Beta's materials): 0 harp (anything
// else), 1 bass drum (stone), 2 snare drum (sand and gravel), 3 clicks (glass), 4 bass (wood). In
// the order of the Note* sounds in sound.h.
int note_instrument(BlockId below);

// --- Ladders and soul sand -----------------------------------------------------------------

// A body with its feet in this cell climbs (Beta's isOnLadder: the block at the feet).
bool climbable(BlockId block);
