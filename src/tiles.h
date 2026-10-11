// What some blocks keep besides their id and data value (Beta's tile entities): a chest's items,
// a sign's text, which part of which picture a painting cell shows, a note block's pitch and the
// disc in a jukebox. Kept with the block's chunk (see Chunk in world.h) and saved with it.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "survival.h"

constexpr int CHEST_SLOTS = 27;  // one chest; a double chest is two of them
using ChestSlots = std::array<std::optional<Stack>, CHEST_SLOTS>;

constexpr int SIGN_LINES = 4, SIGN_LINE_LENGTH = 15;
using SignText = std::array<std::string, SIGN_LINES>;

// One cell of a painting: the picture (painting.h) and which of its block-sized parts this is,
// counted from the left as you look at it and from the bottom.
struct PaintingPart {
    uint8_t motif = 0, x = 0, y = 0;
    bool operator==(const PaintingPart&) const = default;
};

constexpr int NOTE_PITCHES = 25;  // a note block's two octaves, F# to F#
