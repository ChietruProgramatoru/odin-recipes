// Paintings (M11): pictures in Beta's sizes, 1x1 up to 4x4 blocks, hung on a wall. Each cell of a
// painting is a PAINTING_BLOCK (its data value is the side the wall is on) and its chunk keeps
// which part of which picture it shows (PaintingPart in tiles.h). The pictures are this game's own,
// drawn by textures.cpp.
#pragma once
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "rng.h"
#include "tiles.h"

class World;

struct PaintingMotif {
    const char* name;
    int w, h;    // size in blocks
    int tile;    // its top-left tile in the atlas (the rest follow row by row, see textures.h)
};
const std::vector<PaintingMotif>& painting_motifs();

// The tile showing part (x, y) of a motif (x from the left, y from the bottom).
int painting_tile(const PaintingPart& part);

// The direction to the right of someone looking at a painting whose front faces `front`.
glm::ivec3 painting_right(glm::ivec3 front);

// Hanging a painting by right-clicking the side face `face` of the block at `clicked`: like Beta,
// a random one of the pictures that fit there (every cell free, solid wall behind all of it),
// centred on the clicked spot. Returns each cell and its part, or nothing if none fits.
std::optional<std::vector<std::pair<glm::ivec3, PaintingPart>>> hang_painting(World& world, glm::ivec3 clicked, glm::ivec3 face,
                                                                               Rng& rng);
// Put a painting's cells into the world (the blocks and their parts). Returns the chunks to mesh again.
std::vector<struct ChunkPos> place_painting(World& world, int wall_facing,
                                            const std::vector<std::pair<glm::ivec3, PaintingPart>>& cells);

// Every cell of the painting that has a cell at `pos` (just `pos` if its part can't be found).
std::vector<glm::ivec3> painting_cells(World& world, glm::ivec3 pos);
