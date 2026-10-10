"""Block and item definitions."""

AIR, GRASS, DIRT, STONE, COBBLE, SAND, LOG, LEAVES, PLANKS, BEDROCK, WATER, BRICK = range(12)

# Food items live above the block id range.
APPLE, BEEF = 100, 101

# Atlas tile indices (see textures.py).
T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE, T_SAND, T_LOG_SIDE, T_LOG_TOP, \
    T_LEAVES, T_PLANKS, T_BEDROCK, T_WATER, T_BRICK = range(13)
# Tiles for cows, dropped food and the block outline.
T_COW_HIDE, T_WHITE, T_HOOF, T_COW_HEAD, T_SNOUT, T_BLACK, T_HORN, T_APPLE, T_BEEF = range(13, 22)


class BlockInfo:
    def __init__(self, name, top, side, bottom, hardness, drop):
        self.name = name
        self.tiles = (top, side, bottom)   # tile index for top, sides, bottom
        self.hardness = hardness           # seconds to break by hand (None = unbreakable)
        self.drop = drop                   # item id dropped when broken (None = nothing)


BLOCKS = {
    GRASS:   BlockInfo('Grass',       T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, 0.6, DIRT),
    DIRT:    BlockInfo('Dirt',        T_DIRT, T_DIRT, T_DIRT, 0.5, DIRT),
    STONE:   BlockInfo('Stone',       T_STONE, T_STONE, T_STONE, 1.5, COBBLE),
    COBBLE:  BlockInfo('Cobblestone', T_COBBLE, T_COBBLE, T_COBBLE, 1.6, COBBLE),
    SAND:    BlockInfo('Sand',        T_SAND, T_SAND, T_SAND, 0.5, SAND),
    LOG:     BlockInfo('Oak Log',     T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP, 1.5, LOG),
    LEAVES:  BlockInfo('Leaves',      T_LEAVES, T_LEAVES, T_LEAVES, 0.2, None),
    PLANKS:  BlockInfo('Oak Planks',  T_PLANKS, T_PLANKS, T_PLANKS, 1.2, PLANKS),
    BEDROCK: BlockInfo('Bedrock',     T_BEDROCK, T_BEDROCK, T_BEDROCK, None, None),
    WATER:   BlockInfo('Water',       T_WATER, T_WATER, T_WATER, None, None),
    BRICK:   BlockInfo('Bricks',      T_BRICK, T_BRICK, T_BRICK, 2.0, BRICK),
}

# food item -> (hunger restored, saturation restored)
FOODS = {
    APPLE: (4, 2.4),
    BEEF: (6, 4.0),
}

ITEM_NAMES = {APPLE: 'Apple', BEEF: 'Beef'}
for _id, _info in BLOCKS.items():
    ITEM_NAMES[_id] = _info.name

# Blocks you can walk through.
NON_SOLID = {AIR, WATER}

# Items shown in the creative hotbar.
CREATIVE_ITEMS = [GRASS, DIRT, STONE, COBBLE, SAND, LOG, PLANKS, BRICK, LEAVES]


def is_solid(block_id):
    return block_id not in NON_SOLID


def is_block(item_id):
    return item_id in BLOCKS


def icon_tile(item_id):
    """Atlas tile used to draw an item (in the hotbar and as a dropped item)."""
    if item_id == APPLE:
        return T_APPLE
    if item_id == BEEF:
        return T_BEEF
    return BLOCKS[item_id].tiles[1]
