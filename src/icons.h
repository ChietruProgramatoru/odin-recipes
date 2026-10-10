// Pixel art for items: flat inventory icons and the faces of item blocks (crafting table, wool...).
// These live in the block atlas from ITEM_TILE_BASE on, after the world's own tiles.
#pragma once
#include <map>

#include "image.h"
#include "items.h"

constexpr int ITEM_TILE_BASE = 128;

// Faces of blocks that come with the item table.
enum ItemTile : int {
    IT_CRAFT_TOP = ITEM_TILE_BASE, IT_CRAFT_SIDE, IT_CRAFT_FRONT,
    IT_CHEST_TOP, IT_CHEST_SIDE, IT_CHEST_FRONT,
    IT_FURNACE_TOP, IT_FURNACE_SIDE, IT_FURNACE_FRONT,
    IT_WOOL,  // 16 colours, in dye order reversed (white first), like the wool items
    IT_TNT_TOP = IT_WOOL + 16, IT_TNT_SIDE, IT_BOOKSHELF,
    IT_GOLD_BLOCK, IT_IRON_BLOCK, IT_DIAMOND_BLOCK, IT_LAPIS_BLOCK, IT_GLOWSTONE,
    IT_PUMPKIN_TOP, IT_PUMPKIN_SIDE, IT_JACK_FACE, IT_PUMPKIN_FACE,
    IT_SANDSTONE_TOP, IT_SANDSTONE_SIDE, IT_SNOW, IT_CLAY,
    IT_NOTE_BLOCK, IT_JUKEBOX_TOP, IT_DISPENSER_FRONT,
    IT_PISTON_TOP, IT_STICKY_TOP, IT_PISTON_SIDE, IT_SLAB_TOP, IT_SLAB_SIDE,
    IT_FLAT_BASE  // flat item icons follow, one per item, in item id order
};

// Every item tile (block faces and flat icons) by atlas index.
std::map<int, Image> make_item_tiles();
