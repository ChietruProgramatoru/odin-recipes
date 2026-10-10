"""Procedurally generated pixel-art textures, so the game ships without image files."""
import random

from PIL import Image

from .blocks import (BLOCKS, APPLE, BEEF, T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE,
                     T_SAND, T_LOG_SIDE, T_LOG_TOP, T_LEAVES, T_PLANKS, T_BEDROCK, T_WATER, T_BRICK,
                     T_COW_HIDE, T_WHITE, T_HOOF, T_COW_HEAD, T_SNOUT, T_BLACK, T_HORN, T_APPLE, T_BEEF)

TILE = 16
ATLAS_COLS = 8
ATLAS_ROWS = 4


def _clamp(v):
    return max(0, min(255, int(v)))


def _shade(rgb, f, a=255):
    return (_clamp(rgb[0] * f), _clamp(rgb[1] * f), _clamp(rgb[2] * f), a)


def _noise_tile(rng, base, spread=0.15, alpha=255):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = _shade(base, 1 + rng.uniform(-spread, spread), alpha)
    return img


def _grass_top(rng):
    return _noise_tile(rng, (95, 159, 53), 0.18)


def _dirt(rng):
    img = _noise_tile(rng, (134, 96, 67), 0.12)
    px = img.load()
    for _ in range(14):
        x, y = rng.randrange(TILE), rng.randrange(TILE)
        px[x, y] = _shade((134, 96, 67), rng.choice((0.7, 1.25)))
    return img


def _grass_side(rng):
    img = _dirt(rng)
    px = img.load()
    for x in range(TILE):
        depth = 3 + rng.choice((0, 0, 1, 1, 2))
        for y in range(depth):
            px[x, y] = _shade((95, 159, 53), 1 + rng.uniform(-0.15, 0.15))
    return img


def _stone(rng):
    img = _noise_tile(rng, (125, 125, 125), 0.08)
    px = img.load()
    for _ in range(6):
        x, y, n = rng.randrange(TILE), rng.randrange(TILE), rng.randint(2, 5)
        for i in range(n):
            px[(x + i) % TILE, y] = _shade((125, 125, 125), 0.78)
    return img


def _cobble(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    # Voronoi-ish stones with dark mortar between them.
    seeds = [(rng.randrange(TILE), rng.randrange(TILE), rng.uniform(0.8, 1.15)) for _ in range(9)]
    for y in range(TILE):
        for x in range(TILE):
            dists = sorted(((min(abs(x - sx), TILE - abs(x - sx)) ** 2 +
                             min(abs(y - sy), TILE - abs(y - sy)) ** 2), f) for sx, sy, f in seeds)
            edge = dists[1][0] - dists[0][0] < 6
            f = 0.55 if edge else dists[0][1] * rng.uniform(0.93, 1.07)
            px[x, y] = _shade((128, 128, 128), f)
    return img


def _sand(rng):
    return _noise_tile(rng, (219, 207, 163), 0.07)


def _log_side(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    col_f = [rng.uniform(0.8, 1.15) for _ in range(TILE)]
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = _shade((104, 83, 50), col_f[x] * rng.uniform(0.94, 1.06))
    return img


def _log_top(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    c = (TILE - 1) / 2
    for y in range(TILE):
        for x in range(TILE):
            r = max(abs(x - c), abs(y - c))
            if r > 6.6:
                px[x, y] = _shade((104, 83, 50), rng.uniform(0.85, 1.05))
            else:
                ring = 0.85 if int(r) % 2 else 1.0
                px[x, y] = _shade((176, 143, 88), ring * rng.uniform(0.95, 1.05))
    return img


def _leaves(rng):
    img = _noise_tile(rng, (58, 120, 36), 0.22)
    px = img.load()
    for _ in range(26):
        px[rng.randrange(TILE), rng.randrange(TILE)] = _shade((58, 120, 36), 0.5)
    return img


def _planks(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    for y in range(TILE):
        board = y // 4
        offset = (board * 7) % TILE
        board_f = rng.uniform(0.92, 1.08)
        for x in range(TILE):
            seam = (y % 4 == 3) or ((x + offset) % TILE == 0)
            f = 0.62 if seam else board_f * rng.uniform(0.95, 1.05)
            px[x, y] = _shade((162, 130, 78), f)
    return img


def _bedrock(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = _shade((90, 90, 90), rng.choice((0.4, 0.6, 0.8, 1.0, 1.2)))
    return img


def _water(rng):
    img = _noise_tile(rng, (50, 90, 200), 0.1, alpha=170)
    return img


def _brick(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    for y in range(TILE):
        row = y // 4
        offset = 4 if row % 2 else 0
        for x in range(TILE):
            mortar = (y % 4 == 3) or ((x + offset) % 8 == 7)
            if mortar:
                px[x, y] = _shade((190, 185, 175), rng.uniform(0.9, 1.0))
            else:
                px[x, y] = _shade((150, 72, 55), rng.uniform(0.85, 1.12))
    return img


def _cow_hide(rng):
    img = Image.new('RGBA', (TILE, TILE))
    px = img.load()
    patches = [(rng.randrange(TILE), rng.randrange(TILE), rng.uniform(3, 5)) for _ in range(4)]
    for y in range(TILE):
        for x in range(TILE):
            dark = any(min(abs(x - cx), TILE - abs(x - cx)) ** 2 + min(abs(y - cy), TILE - abs(y - cy)) ** 2 < r * r
                       for cx, cy, r in patches)
            px[x, y] = _shade((60, 40, 30) if dark else (235, 230, 225), rng.uniform(0.94, 1.06))
    return img


def _solid(rgb, spread=0.04):
    return lambda rng: _noise_tile(rng, rgb, spread)


def _padded_sprite(rows, palette):
    def make(rng):
        img = Image.new('RGBA', (TILE, TILE), (0, 0, 0, 0))
        spr = _sprite(rows, palette)
        img.paste(spr, ((TILE - spr.width) // 2, (TILE - spr.height) // 2))
        return img
    return make


_GENERATORS = {
    T_GRASS_TOP: _grass_top, T_GRASS_SIDE: _grass_side, T_DIRT: _dirt, T_STONE: _stone,
    T_COBBLE: _cobble, T_SAND: _sand, T_LOG_SIDE: _log_side, T_LOG_TOP: _log_top,
    T_LEAVES: _leaves, T_PLANKS: _planks, T_BEDROCK: _bedrock, T_WATER: _water, T_BRICK: _brick,
    T_COW_HIDE: _cow_hide, T_WHITE: _solid((255, 255, 255), 0), T_HOOF: _solid((70, 48, 35)),
    T_COW_HEAD: _solid((85, 60, 45)), T_SNOUT: _solid((225, 165, 160)), T_BLACK: _solid((16, 16, 16), 0),
    T_HORN: _solid((235, 230, 215)),
}


def _late_generators():
    # The food sprites are defined further down this file.
    _GENERATORS[T_APPLE] = _padded_sprite(APPLE_SPRITE, {'g': (70, 110, 30, 255), 'r': (210, 30, 30, 255),
                                                         'w': (255, 180, 180, 255)})
    _GENERATORS[T_BEEF] = _padded_sprite(BEEF_SPRITE, {'k': (90, 20, 20, 255), 'r': (215, 70, 70, 255),
                                                       'p': (240, 150, 150, 255), 'w': (245, 225, 225, 255)})


def make_tiles(seed=1234):
    _late_generators()
    tiles = {}
    for index, gen in _GENERATORS.items():
        tiles[index] = gen(random.Random(seed * 100 + index))
    return tiles


def make_atlas(tiles):
    atlas = Image.new('RGBA', (TILE * ATLAS_COLS, TILE * ATLAS_ROWS), (255, 0, 255, 255))
    for index, img in tiles.items():
        atlas.paste(img, ((index % ATLAS_COLS) * TILE, (index // ATLAS_COLS) * TILE))
    return atlas


def tile_uv(index):
    """UV rectangle (u0, v0, u1, v1) of an atlas tile. Images are uploaded so v=1 is the top row."""
    col, row = index % ATLAS_COLS, index // ATLAS_COLS
    inset = 0.0005
    u0 = col / ATLAS_COLS + inset
    u1 = (col + 1) / ATLAS_COLS - inset
    v1 = 1 - row / ATLAS_ROWS - inset
    v0 = 1 - (row + 1) / ATLAS_ROWS + inset
    return u0, v0, u1, v1


# --- Pixel-art sprites -------------------------------------------------------

def _sprite(rows, palette, scale=1):
    h, w = len(rows), len(rows[0])
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in palette:
                px[x, y] = palette[ch]
    if scale != 1:
        img = img.resize((w * scale, h * scale), Image.NEAREST)
    return img


HEART = [
    '.kk...kk.',
    'krrk.krrk',
    'krwrkrrrk',
    'krrrrrrrk',
    '.krrrrrk.',
    '..krrrk..',
    '...krk...',
    '....k....',
]
DRUMSTICK = [
    '.....kkkk',
    '....kbbbk',
    '...kbbbbk',
    '..kbbbbbk',
    '.kbbbbbk.',
    'kwkbbkk..',
    'kwwkk....',
    '.kk......',
]
APPLE_SPRITE = [
    '.....g....',
    '....g.....',
    '..rr.rrr..',
    '.rrrrrrwr.',
    '.rrrrrrrr.',
    '.rrrrrrrr.',
    '.rrrrrrrr.',
    '..rrrrrr..',
    '...rr.rr..',
]
BEEF_SPRITE = [
    '..........',
    '...kkkk...',
    '..krrrpk..',
    '.krrrrrpk.',
    'krrwrrrrk.',
    'krrrrwrrk.',
    '.krrrrrrk.',
    '..kkrrrk..',
    '....kkk...',
]


def _halve(full, empty):
    """Left half from `full`, right half from `empty`."""
    img = empty.copy()
    w, h = full.size
    img.paste(full.crop((0, 0, w // 2 + 1, h)), (0, 0))
    return img


def make_hud_icons():
    heart_full = _sprite(HEART, {'k': (40, 10, 10, 255), 'r': (220, 30, 30, 255), 'w': (255, 200, 200, 255)}, 4)
    heart_empty = _sprite(HEART, {'k': (40, 10, 10, 255), 'r': (60, 30, 30, 200), 'w': (60, 30, 30, 200)}, 4)
    food_full = _sprite(DRUMSTICK, {'k': (50, 25, 10, 255), 'b': (170, 100, 40, 255), 'w': (240, 230, 220, 255)}, 4)
    food_empty = _sprite(DRUMSTICK, {'k': (50, 25, 10, 255), 'b': (60, 40, 30, 200), 'w': (60, 40, 30, 200)}, 4)
    return {
        'heart_full': heart_full, 'heart_half': _halve(heart_full, heart_empty), 'heart_empty': heart_empty,
        'food_full': food_full, 'food_half': _halve(food_full, food_empty), 'food_empty': food_empty,
    }


def make_item_icons(tiles):
    from .blocks import icon_tile
    icons = {}
    for item in list(BLOCKS) + [APPLE, BEEF]:
        icons[item] = tiles[icon_tile(item)].resize((32, 32), Image.NEAREST)
    return icons
