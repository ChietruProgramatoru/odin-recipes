"""Voxel world: chunk storage, terrain generation, meshing and ray casting.

Nothing in here touches the graphics engine, so it can be tested without a window.
"""
import math
from array import array

from .blocks import (AIR, GRASS, DIRT, STONE, SAND, LOG, LEAVES, BEDROCK, WATER, BLOCKS,
                     NON_SOLID)
from .noise import Perlin2D
from .textures import tile_uv

CHUNK = 16
HEIGHT = 64
SEA_LEVEL = 20
SNOWLESS_PEAK = 44   # above this height the surface is bare stone

# --- Padded chunk layout used while meshing ---------------------------------
# An 18 x (HEIGHT + 2) x 18 copy of a chunk plus a one-block border from its neighbours,
# so neighbour lookups are plain index offsets.
PW = CHUNK + 2
LAYER = PW * PW


def _off(dx, dy, dz):
    return dy * LAYER + dz * PW + dx


# Each face: normal, corners (counter-clockwise seen from outside: BL, BR, TR, TL),
# light level and which tile to use (0 top, 1 side, 2 bottom).
_FACE_DEFS = [
    ((1, 0, 0), ((1, 0, 0), (1, 0, 1), (1, 1, 1), (1, 1, 0)), 0.80, 1),
    ((-1, 0, 0), ((0, 0, 1), (0, 0, 0), (0, 1, 0), (0, 1, 1)), 0.80, 1),
    ((0, 0, 1), ((1, 0, 1), (0, 0, 1), (0, 1, 1), (1, 1, 1)), 0.68, 1),
    ((0, 0, -1), ((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), 0.68, 1),
    ((0, 1, 0), ((0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1)), 1.00, 0),
    ((0, -1, 0), ((0, 0, 1), (1, 0, 1), (1, 0, 0), (0, 0, 0)), 0.50, 2),
]


def _build_faces():
    faces = []
    for normal, corners, light, slot in _FACE_DEFS:
        axis = [abs(c) for c in normal].index(1)
        tangents = [a for a in range(3) if a != axis]
        ao = []
        for corner in corners:
            d = [0, 0, 0]
            vecs = []
            for t in tangents:
                v = list(normal)
                v[t] += 1 if corner[t] == 1 else -1
                vecs.append(v)
                d[t] = 1 if corner[t] == 1 else -1
            both = [normal[k] + d[k] for k in range(3)]
            ao.append((_off(*vecs[0]), _off(*vecs[1]), _off(*both)))
        faces.append((_off(*normal), corners, light, slot, ao))
    return faces


FACES = _build_faces()
AO_LEVELS = (1.0, 0.78, 0.62, 0.48)
OPAQUE = [b not in (AIR, WATER) for b in range(256)]

# Per block: (top uv, side uv, bottom uv)
BLOCK_UVS = {b: tuple(tile_uv(t) for t in info.tiles) for b, info in BLOCKS.items()}

WATER_SURFACE = 0.875


class Chunk:
    __slots__ = ('cx', 'cz', 'blocks', 'max_y', 'mob_spawned')

    def __init__(self, cx, cz):
        self.cx, self.cz = cx, cz
        self.blocks = bytearray(CHUNK * CHUNK * HEIGHT)
        self.max_y = 0
        self.mob_spawned = False


def _hash01(x, z, salt, seed):
    h = (x * 73856093) ^ (z * 19349663) ^ (salt * 83492791) ^ (seed * 2654435761)
    h &= 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFFFF) / 4294967296.0


class TerrainGenerator:
    def __init__(self, seed):
        self.seed = seed
        self.continent = Perlin2D(seed)
        self.hills = Perlin2D(seed + 1)
        self.mountains = Perlin2D(seed + 2)
        self._height_cache = {}

    def height(self, wx, wz):
        key = (wx, wz)
        h = self._height_cache.get(key)
        if h is None:
            c = self.continent.fbm(wx / 220.0, wz / 220.0, 3)
            hl = self.hills.fbm(wx / 55.0, wz / 55.0, 4)
            m = max(0.0, self.mountains.fbm(wx / 110.0 + 50, wz / 110.0 + 50, 3))
            h = int(SEA_LEVEL + 3 + c * 14 + hl * 6 + m * m * 70)
            h = max(3, min(HEIGHT - 12, h))
            if len(self._height_cache) > 200000:
                self._height_cache.clear()
            self._height_cache[key] = h
        return h

    def is_tree(self, wx, wz):
        return _hash01(wx, wz, 1, self.seed) < 0.011

    def generate(self, chunk):
        b = chunk.blocks
        bx, bz = chunk.cx * CHUNK, chunk.cz * CHUNK
        top = SEA_LEVEL
        for lz in range(CHUNK):
            for lx in range(CHUNK):
                h = self.height(bx + lx, bz + lz)
                beach = h <= SEA_LEVEL + 1
                col = (lz << 4) | lx
                b[col] = BEDROCK
                for y in range(1, h + 1):
                    if y < h - 3:
                        blk = STONE
                    elif y < h:
                        blk = SAND if beach else DIRT
                    elif beach:
                        blk = SAND
                    else:
                        blk = STONE if h > SNOWLESS_PEAK else GRASS
                    b[(y << 8) | col] = blk
                for y in range(h + 1, SEA_LEVEL + 1):
                    b[(y << 8) | col] = WATER
                top = max(top, h)

        # Trees (may hang over from neighbouring columns, so scan a margin).
        for wz in range(bz - 2, bz + CHUNK + 2):
            for wx in range(bx - 2, bx + CHUNK + 2):
                if not self.is_tree(wx, wz):
                    continue
                h = self.height(wx, wz)
                if h <= SEA_LEVEL + 1 or h > SNOWLESS_PEAK - 2:
                    continue
                trunk = 4 + int(_hash01(wx, wz, 2, self.seed) * 3)
                crown = h + trunk
                for dy in range(-2, 2):
                    r = 2 if dy < 0 else 1
                    for dz in range(-r, r + 1):
                        for dx in range(-r, r + 1):
                            if r == 2 and abs(dx) == 2 and abs(dz) == 2 and \
                                    _hash01(wx + dx, wz + dz, 3 + dy, self.seed) < 0.6:
                                continue
                            if dy == 1 and abs(dx) == 1 and abs(dz) == 1:
                                continue
                            self._place(chunk, wx + dx, crown + dy, wz + dz, LEAVES, only_air=True)
                for y in range(h + 1, crown):
                    self._place(chunk, wx, y, wz, LOG)
                self._place(chunk, wx, h, wz, DIRT)
                top = max(top, crown + 2)
        chunk.max_y = min(HEIGHT - 1, top + 1)

    @staticmethod
    def _place(chunk, wx, y, wz, blk, only_air=False):
        lx, lz = wx - chunk.cx * CHUNK, wz - chunk.cz * CHUNK
        if 0 <= lx < CHUNK and 0 <= lz < CHUNK and 0 <= y < HEIGHT:
            i = (y << 8) | (lz << 4) | lx
            if not only_air or chunk.blocks[i] == AIR:
                chunk.blocks[i] = blk


class World:
    def __init__(self, seed=2024):
        self.seed = seed
        self.generator = TerrainGenerator(seed)
        self.chunks = {}

    # --- storage -------------------------------------------------------------
    def chunk(self, cx, cz):
        c = self.chunks.get((cx, cz))
        if c is None:
            c = Chunk(cx, cz)
            self.generator.generate(c)
            self.chunks[(cx, cz)] = c
        return c

    def get_block(self, x, y, z):
        if y < 0:
            return BEDROCK
        if y >= HEIGHT:
            return AIR
        c = self.chunks.get((x >> 4, z >> 4))
        if c is None:
            return AIR
        return c.blocks[(y << 8) | ((z & 15) << 4) | (x & 15)]

    def set_block(self, x, y, z, blk):
        """Set a block. Returns the chunk keys whose meshes need rebuilding."""
        if not 0 <= y < HEIGHT:
            return []
        cx, cz = x >> 4, z >> 4
        c = self.chunk(cx, cz)
        lx, lz = x & 15, z & 15
        c.blocks[(y << 8) | (lz << 4) | lx] = blk
        if blk != AIR:
            c.max_y = max(c.max_y, min(HEIGHT - 1, y + 1))
        xs = [cx] + ([cx - 1] if lx == 0 else []) + ([cx + 1] if lx == 15 else [])
        zs = [cz] + ([cz - 1] if lz == 0 else []) + ([cz + 1] if lz == 15 else [])
        # Faces and ambient occlusion reach one block into neighbouring chunks.
        dirty = [(a, b) for a in xs for b in zs if (a, b) in self.chunks]
        return dirty

    def is_solid(self, x, y, z):
        return self.get_block(x, y, z) not in NON_SOLID

    def surface_y(self, x, z):
        """Y of the first free block above the highest solid one in a column."""
        self.chunk(x >> 4, z >> 4)
        for y in range(HEIGHT - 1, -1, -1):
            if self.is_solid(x, y, z):
                return y + 1
        return 1

    def box_collides(self, x0, y0, z0, x1, y1, z1):
        for bx in range(math.floor(x0), math.floor(x1) + 1):
            for by in range(math.floor(y0), math.floor(y1) + 1):
                for bz in range(math.floor(z0), math.floor(z1) + 1):
                    if self.get_block(bx, by, bz) not in NON_SOLID:
                        return True
        return False

    def box_in_water(self, x0, y0, z0, x1, y1, z1):
        for bx in range(math.floor(x0), math.floor(x1) + 1):
            for by in range(math.floor(y0), math.floor(y1) + 1):
                for bz in range(math.floor(z0), math.floor(z1) + 1):
                    if self.get_block(bx, by, bz) == WATER:
                        return True
        return False

    # --- ray casting ----------------------------------------------------------
    def raycast(self, origin, direction, max_dist):
        """Voxel DDA. Returns (hit_block_pos, previous_empty_pos, distance) or None."""
        ox, oy, oz = origin
        dx, dy, dz = direction
        x, y, z = math.floor(ox), math.floor(oy), math.floor(oz)
        step = [1 if d > 0 else -1 for d in (dx, dy, dz)]
        t_max, t_delta = [], []
        for o, d, cell in ((ox, dx, x), (oy, dy, y), (oz, dz, z)):
            if abs(d) < 1e-9:
                t_max.append(math.inf)
                t_delta.append(math.inf)
            else:
                bound = cell + 1 if d > 0 else cell
                t_max.append((bound - o) / d)
                t_delta.append(abs(1 / d))
        prev = None
        t = 0.0
        pos = [x, y, z]
        while t <= max_dist:
            blk = self.get_block(*pos)
            if blk != AIR and blk != WATER:
                return tuple(pos), prev, t
            prev = tuple(pos)
            axis = t_max.index(min(t_max))
            t = t_max[axis]
            pos[axis] += step[axis]
            t_max[axis] += t_delta[axis]
        return None

    # --- meshing --------------------------------------------------------------
    def _padded(self, chunk):
        cx, cz = chunk.cx, chunk.cz
        P = bytearray(LAYER * (HEIGHT + 2))
        P[0:LAYER] = bytes([BEDROCK]) * LAYER          # y = -1
        b = chunk.blocks
        nxm = self.chunk(cx - 1, cz).blocks
        nxp = self.chunk(cx + 1, cz).blocks
        nzm = self.chunk(cx, cz - 1).blocks
        nzp = self.chunk(cx, cz + 1).blocks
        dmm = self.chunk(cx - 1, cz - 1).blocks
        dpm = self.chunk(cx + 1, cz - 1).blocks
        dmp = self.chunk(cx - 1, cz + 1).blocks
        dpp = self.chunk(cx + 1, cz + 1).blocks
        top = min(HEIGHT, chunk.max_y + 2)
        for y in range(top):
            py = (y + 1) * LAYER
            sy = y << 8
            for z in range(CHUNK):
                dst = py + (z + 1) * PW
                src = sy | (z << 4)
                P[dst + 1:dst + 17] = b[src:src + 16]
                P[dst] = nxm[src | 15]
                P[dst + 17] = nxp[src]
            P[py + 1:py + 17] = nzm[(sy | 240):(sy | 240) + 16]
            row = py + 17 * PW
            P[row + 1:row + 17] = nzp[sy:sy + 16]
            P[py] = dmm[sy | 255]
            P[py + 17] = dpm[sy | 240]
            P[row] = dmp[sy | 15]
            P[row + 17] = dpp[sy]
        return P

    def build_mesh_data(self, chunk):
        """Returns (solid, water) meshes, each a MeshData with positions, shades, uvs and indices."""
        P = self._padded(chunk)
        ox, oz = chunk.cx * CHUNK, chunk.cz * CHUNK
        solid, water = MeshData(), MeshData()
        opaque = OPAQUE
        uvs = BLOCK_UVS
        faces = FACES
        ao_levels = AO_LEVELS
        up = _off(0, 1, 0)
        for y in range(min(HEIGHT, chunk.max_y + 1)):
            for z in range(CHUNK):
                base = (y + 1) * LAYER + (z + 1) * PW + 1
                for x in range(CHUNK):
                    i = base + x
                    blk = P[i]
                    if blk == AIR:
                        continue
                    wx, wz = ox + x, oz + z
                    if blk == WATER:
                        h = 1.0 if P[i + up] == WATER else WATER_SURFACE
                        uv = uvs[WATER][0]
                        for off, corners, light, slot, _ao in faces:
                            if off == -up or P[i + off] != AIR:
                                continue
                            water.quad(wx, y, wz, corners, h, uv, (light, light, light, light))
                        continue
                    block_uvs = uvs[blk]
                    for off, corners, light, slot, ao in faces:
                        if opaque[P[i + off]]:
                            continue
                        shades = []
                        for s1, s2, c in ao:
                            a, bb = opaque[P[i + s1]], opaque[P[i + s2]]
                            occ = 3 if (a and bb) else a + bb + opaque[P[i + c]]
                            shades.append(light * ao_levels[occ])
                        solid.quad(wx, y, wz, corners, 1.0, block_uvs[slot], shades)
        return solid, water


class MeshData:
    """Vertex attributes for indexed triangles: position (3f), shade (1f), uv (2f)."""
    __slots__ = ('positions', 'shades', 'uvs', 'indices', 'count')

    def __init__(self):
        self.positions = array('f')
        self.shades = array('f')
        self.uvs = array('f')
        self.indices = array('I')
        self.count = 0

    def quad(self, x, y, z, corners, h, uv, shades):
        u0, v0, u1, v1 = uv
        (c0, c1, c2, c3) = corners
        self.positions.extend((x + c0[0], y + c0[1] * h, z + c0[2], x + c1[0], y + c1[1] * h, z + c1[2],
                               x + c2[0], y + c2[1] * h, z + c2[2], x + c3[0], y + c3[1] * h, z + c3[2]))
        s0, s1, s2, s3 = shades
        self.shades.extend(shades)
        self.uvs.extend((u0, v0, u1, v0, u1, v1, u0, v1))
        n = self.count
        # Flip the quad's diagonal so occlusion gradients interpolate smoothly.
        if s0 + s2 < s1 + s3:
            self.indices.extend((n + 1, n + 2, n + 3, n + 1, n + 3, n))
        else:
            self.indices.extend((n, n + 1, n + 2, n, n + 2, n + 3))
        self.count = n + 4

    def quad_points(self, pts, uv, shade):
        """Append a quad from four absolute corner points (BL, BR, TR, TL) with one shade."""
        u0, v0, u1, v1 = uv
        for p in pts:
            self.positions.extend(p)
        self.shades.extend((shade, shade, shade, shade))
        self.uvs.extend((u0, v0, u1, v0, u1, v1, u0, v1))
        n = self.count
        self.indices.extend((n, n + 1, n + 2, n, n + 2, n + 3))
        self.count = n + 4
