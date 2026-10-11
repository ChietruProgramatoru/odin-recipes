#include "painting.h"

#include "building.h"
#include "textures.h"
#include "world.h"

const std::vector<PaintingMotif>& painting_motifs() {
    static const std::vector<PaintingMotif> motifs = [] {
        // Beta's sizes: small ones are common, the big ones need a big wall.
        std::vector<PaintingMotif> m = {
            {"Sunrise", 1, 1, 0}, {"Pine", 1, 1, 0},     {"Pond", 1, 1, 0},      {"Mushroom", 1, 1, 0},
            {"Hills", 2, 1, 0},   {"Sea", 2, 1, 0},      {"Tower", 1, 2, 0},     {"Waterfall", 1, 2, 0},
            {"Lake", 2, 2, 0},    {"Cave", 2, 2, 0},     {"Orchard", 2, 2, 0},   {"Valley", 4, 2, 0},
            {"Peaks", 4, 3, 0},   {"Night Sky", 4, 4, 0}, {"Volcano", 4, 4, 0},
        };
        int tile = PAINTING_TILE_BASE;
        for (auto& p : m) {
            p.tile = tile;
            tile += p.w * p.h;
        }
        return m;
    }();
    return motifs;
}

int painting_tile(const PaintingPart& part) {
    const auto& motifs = painting_motifs();
    const PaintingMotif& m = motifs[part.motif < motifs.size() ? part.motif : 0];
    return m.tile + (m.h - 1 - part.y) * m.w + part.x;
}

glm::ivec3 painting_right(glm::ivec3 front) { return {front.z, 0, -front.x}; }

namespace {

// The cells of a w x h painting centred on `spot`, facing `front`, with each one's part.
std::vector<std::pair<glm::ivec3, PaintingPart>> cells_of(int motif, glm::ivec3 spot, glm::ivec3 front) {
    const PaintingMotif& m = painting_motifs()[motif];
    const glm::ivec3 right = painting_right(front), up(0, 1, 0);
    const glm::ivec3 origin = spot - right * ((m.w - 1) / 2) - up * ((m.h - 1) / 2);
    std::vector<std::pair<glm::ivec3, PaintingPart>> out;
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x)
            out.push_back({origin + right * x + up * y,
                           PaintingPart{static_cast<uint8_t>(motif), static_cast<uint8_t>(x), static_cast<uint8_t>(y)}});
    return out;
}

}  // namespace

std::optional<std::vector<std::pair<glm::ivec3, PaintingPart>>> hang_painting(World& world, glm::ivec3 clicked, glm::ivec3 face,
                                                                               Rng& rng) {
    if (face.y != 0) return std::nullopt;  // only on the side of a block
    const glm::ivec3 spot = clicked + face;
    std::vector<int> fits;
    for (int m = 0; m < static_cast<int>(painting_motifs().size()); ++m) {
        bool ok = true;
        for (const auto& [cell, part] : cells_of(m, spot, face)) {
            const glm::ivec3 wall = cell - face;
            if (cell.y < 0 || cell.y >= HEIGHT || world.get_block(cell.x, cell.y, cell.z) != AIR ||
                !is_solid(world.get_block(wall.x, wall.y, wall.z)) || !world.is_loaded(cell.x, cell.z)) {
                ok = false;
                break;
            }
        }
        if (ok) fits.push_back(m);
    }
    if (fits.empty()) return std::nullopt;
    return cells_of(fits[rng.below(static_cast<int>(fits.size()))], spot, face);
}

std::vector<ChunkPos> place_painting(World& world, int wall_facing, const std::vector<std::pair<glm::ivec3, PaintingPart>>& cells) {
    std::vector<ChunkPos> dirty;
    for (const auto& [cell, part] : cells) {
        auto d = world.set_block(cell.x, cell.y, cell.z, PAINTING_BLOCK, wall_facing);
        if (Chunk* c = world.chunk_at(cell)) c->paintings[cell_index(cell)] = part;
        dirty.insert(dirty.end(), d.begin(), d.end());
    }
    return dirty;
}

std::vector<glm::ivec3> painting_cells(World& world, glm::ivec3 pos) {
    Chunk* c = world.chunk_at(pos);
    if (!c) return {pos};
    auto it = c->paintings.find(cell_index(pos));
    if (it == c->paintings.end() || it->second.motif >= painting_motifs().size()) return {pos};
    const PaintingPart part = it->second;
    const glm::ivec3 front = -facing_dir(world.get_data(pos.x, pos.y, pos.z));
    const glm::ivec3 right = painting_right(front), up(0, 1, 0);
    const glm::ivec3 origin = pos - right * static_cast<int>(part.x) - up * static_cast<int>(part.y);
    const PaintingMotif& m = painting_motifs()[part.motif];
    std::vector<glm::ivec3> out;
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x) {
            const glm::ivec3 p = origin + right * x + up * y;
            if (world.get_block(p.x, p.y, p.z) != PAINTING_BLOCK) continue;
            Chunk* pc = world.chunk_at(p);
            if (!pc) continue;
            auto pi = pc->paintings.find(cell_index(p));
            if (pi != pc->paintings.end() && pi->second == PaintingPart{part.motif, static_cast<uint8_t>(x), static_cast<uint8_t>(y)})
                out.push_back(p);
        }
    if (out.empty()) out.push_back(pos);
    return out;
}
