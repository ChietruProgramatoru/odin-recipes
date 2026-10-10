// Sky light and block light, 0..15 each, spread one block at a time like Beta.
//
// Every chunk is first lit on its own (on a worker thread, see light_chunk). When it joins the
// world, light flows across its edges in both directions. A changed block takes away the light
// that depended on it, then the surrounding light flows back in.
#include <algorithm>
#include <deque>
#include <unordered_set>

#include "world.h"

namespace {

constexpr int SKY = 1, BLK = 0;  // the two light channels

int get(const Chunk& c, int i, int ch) { return ch == SKY ? c.light[i] >> 4 : c.light[i] & 15; }
void put(Chunk& c, int i, int ch, int v) {
    c.light[i] = static_cast<uint8_t>(ch == SKY ? (c.light[i] & 0x0F) | (v << 4) : (c.light[i] & 0xF0) | v);
}

// Light a cell has without help from its neighbours: what the block gives off, or for the top
// layer of the world, the open sky above it.
int own_light(BlockId b, int y, int ch) {
    if (ch == BLK) return light_emission(b);
    return y == HEIGHT - 1 ? std::max(0, spread_light(MAX_LIGHT, b, true)) : 0;
}

const glm::ivec3 DIRS[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}};

// Spread light inside one chunk from the queued cell indices.
void spread_local(Chunk& c, std::vector<int>& queue, int ch) {
    for (size_t q = 0; q < queue.size(); ++q) {
        int i = queue[q];
        int v = get(c, i, ch);
        if (v <= 1) continue;
        int x = i & 15, z = (i >> 4) & 15, y = i >> 8;
        for (const glm::ivec3& d : DIRS) {
            int nx = x + d.x, ny = y + d.y, nz = z + d.z;
            if (nx < 0 || nx >= CHUNK || nz < 0 || nz >= CHUNK || ny < 0 || ny >= HEIGHT) continue;
            int n = Chunk::index(nx, ny, nz);
            int nv = spread_light(v, c.blocks[n], ch == SKY && d.y == -1);
            if (nv > get(c, n, ch)) {
                put(c, n, ch, nv);
                queue.push_back(n);
            }
        }
    }
}

// The cells around a changed cell whose chunk meshes can see it (each mesh copies a one-block border).
void touch(std::vector<ChunkPos>* dirty, const glm::ivec3& p) {
    if (!dirty) return;
    for (int dx = -1; dx <= 1; dx += 2)
        for (int dz = -1; dz <= 1; dz += 2) {
            ChunkPos k{chunk_of(p.x + dx), chunk_of(p.z + dz)};
            if (std::find(dirty->begin(), dirty->end(), k) == dirty->end()) dirty->push_back(k);
        }
}

}  // namespace

void light_chunk(Chunk& c) {
    c.light.fill(0);
    // Sky light straight down each column until something stops it.
    for (int col = 0; col < CHUNK * CHUNK; ++col) {
        int v = MAX_LIGHT;
        for (int y = HEIGHT - 1; y >= 0; --y) {
            int i = (y << 8) | col;
            v = spread_light(v, c.blocks[i], true);
            if (v <= 0) break;
            put(c, i, SKY, v);
        }
    }
    // Then sideways (and on down) from every lit cell next to a darker one.
    std::vector<int> queue;
    for (int i = 0; i < CHUNK * CHUNK * HEIGHT; ++i) {
        int v = get(c, i, SKY);
        if (v <= 1) continue;
        int x = i & 15, z = (i >> 4) & 15;
        auto darker = [&](int n) { return get(c, n, SKY) < v - 1; };
        if ((x > 0 && darker(i - 1)) || (x < 15 && darker(i + 1)) || (z > 0 && darker(i - 16)) || (z < 15 && darker(i + 16)))
            queue.push_back(i);
    }
    spread_local(c, queue, SKY);

    queue.clear();
    for (int i = 0; i < CHUNK * CHUNK * HEIGHT; ++i)
        if (int e = light_emission(c.blocks[i])) {
            put(c, i, BLK, e);
            queue.push_back(i);
        }
    spread_local(c, queue, BLK);
    c.lit = true;
}

namespace {

// Light spreading through the loaded world, from queued block positions.
struct WorldLight {
    World& world;
    std::vector<ChunkPos>* dirty;

    Chunk* at(const glm::ivec3& p, int& i) {
        i = 0;
        if (p.y < 0 || p.y >= HEIGHT) return nullptr;
        Chunk* c = world.find(chunk_of(p.x), chunk_of(p.z));
        i = Chunk::index(p.x & 15, p.y, p.z & 15);
        return c;
    }

    void spread(std::deque<glm::ivec3>& queue, int ch) {
        while (!queue.empty()) {
            glm::ivec3 p = queue.front();
            queue.pop_front();
            int i = 0;
            Chunk* c = at(p, i);
            if (!c) continue;
            int v = get(*c, i, ch);
            if (v <= 1) continue;
            for (const glm::ivec3& d : DIRS) {
                glm::ivec3 n = p + d;
                int ni = 0;
                Chunk* nc = at(n, ni);
                if (!nc) continue;
                int nv = spread_light(v, nc->blocks[ni], ch == SKY && d.y == -1);
                if (nv > get(*nc, ni, ch)) {
                    put(*nc, ni, ch, nv);
                    touch(dirty, n);
                    queue.push_back(n);
                }
            }
        }
    }

    // The light at p went down from `old`: take away all the light that came through p, then
    // let the light from around the dark area flow back in.
    void unlight(const glm::ivec3& p, int old, int ch) {
        std::deque<std::pair<glm::ivec3, int>> dark{{p, old}};
        std::vector<glm::ivec3> removed{p};
        std::deque<glm::ivec3> refill;
        int i = 0;
        Chunk* pc = at(p, i);
        put(*pc, i, ch, 0);
        touch(dirty, p);
        while (!dark.empty()) {
            auto [c, v] = dark.front();
            dark.pop_front();
            for (const glm::ivec3& d : DIRS) {
                glm::ivec3 n = c + d;
                int ni = 0;
                Chunk* nc = at(n, ni);
                if (!nc) continue;
                int ln = get(*nc, ni, ch);
                if (ln == 0) continue;
                // Dimmer neighbours may have been lit through c (and full sky light straight below it).
                if (ln < v || (ch == SKY && d.y == -1 && v == MAX_LIGHT)) {
                    put(*nc, ni, ch, 0);
                    touch(dirty, n);
                    dark.push_back({n, ln});
                    removed.push_back(n);
                } else {
                    refill.push_back(n);
                }
            }
        }
        for (const glm::ivec3& r : removed) {
            Chunk* c = at(r, i);
            int own = own_light(c->blocks[i], r.y, ch);
            if (own > get(*c, i, ch)) {
                put(*c, i, ch, own);
                refill.push_back(r);
            }
        }
        spread(refill, ch);
    }
};

}  // namespace

void World::join_light(Chunk& chunk) {
    WorldLight wl{*this, nullptr};
    const int bx = chunk.cx * CHUNK, bz = chunk.cz * CHUNK;
    for (int ch : {SKY, BLK}) {
        std::deque<glm::ivec3> queue;
        for (int side = 0; side < 4; ++side) {
            const glm::ivec3 d = DIRS[side];
            if (!find(chunk.cx + d.x, chunk.cz + d.z)) continue;
            for (int k = 0; k < CHUNK; ++k) {
                // The cell on our edge and the one across it in the neighbour.
                int lx = d.x > 0 ? 15 : d.x < 0 ? 0 : k, lz = d.z > 0 ? 15 : d.z < 0 ? 0 : k;
                glm::ivec3 a(bx + lx, 0, bz + lz), b = a + d;
                for (int y = 0; y < HEIGHT; ++y) {
                    a.y = b.y = y;
                    int ai = 0, bi = 0;
                    Chunk* ac = wl.at(a, ai);
                    Chunk* bc = wl.at(b, bi);
                    int la = get(*ac, ai, ch), lb = get(*bc, bi, ch);
                    if (spread_light(la, bc->blocks[bi], false) > lb) queue.push_back(a);
                    if (spread_light(lb, ac->blocks[ai], false) > la) queue.push_back(b);
                }
            }
        }
        wl.spread(queue, ch);
    }
}

void World::relight(int x, int y, int z, std::vector<ChunkPos>& dirty) {
    WorldLight wl{*this, &dirty};
    const glm::ivec3 p(x, y, z);
    int i = 0;
    Chunk* c = wl.at(p, i);
    if (!c) return;
    const BlockId b = c->blocks[i];
    for (int ch : {SKY, BLK}) {
        int old = get(*c, i, ch);
        int now = own_light(b, y, ch);
        for (const glm::ivec3& d : DIRS) {
            int ni = 0;
            Chunk* nc = wl.at(p + d, ni);
            if (nc) now = std::max(now, spread_light(get(*nc, ni, ch), b, ch == SKY && d.y == 1));
        }
        if (now < old) {
            wl.unlight(p, old, ch);
        } else if (now > old) {
            put(*c, i, ch, now);
            touch(&dirty, p);
            std::deque<glm::ivec3> queue{p};
            wl.spread(queue, ch);
        }
    }
}

int World::sky_light(int x, int y, int z) {
    if (y >= HEIGHT) return MAX_LIGHT;
    if (y < 0) return 0;
    Chunk* c = find(chunk_of(x), chunk_of(z));
    return c ? get(*c, Chunk::index(x & 15, y, z & 15), SKY) : MAX_LIGHT;
}

int World::block_light(int x, int y, int z) {
    if (y < 0 || y >= HEIGHT) return 0;
    Chunk* c = find(chunk_of(x), chunk_of(z));
    return c ? get(*c, Chunk::index(x & 15, y, z & 15), BLK) : 0;
}
