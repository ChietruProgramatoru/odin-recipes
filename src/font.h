// A 5x7 pixel font for the HUD, drawn in code like the textures.
#pragma once
#include <cstdint>
#include <string>

#include "image.h"

constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 7;
constexpr int GLYPH_ADVANCE = 6;  // pixels from one character to the next
constexpr int LINE_HEIGHT = 10;   // pixels from one line to the next
constexpr int FONT_COLS = 16;     // glyph cells per row in the font image
constexpr int CELL_W = 6, CELL_H = 8;

// Rows of a printable ASCII glyph, top first; bit 4 is the leftmost pixel. Others draw as '?'.
const uint8_t* glyph_rows(char c);
// White glyphs on transparent, one CELL_W x CELL_H cell per character from ' ' to '~'.
Image make_font_image();
// Cell (column, row) of a character in make_font_image().
void glyph_cell(char c, int& col, int& row);
// Size in font pixels of (possibly multi-line) text.
int text_width(const std::string& text);
int text_height(const std::string& text);
