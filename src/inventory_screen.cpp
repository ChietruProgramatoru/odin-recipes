#include "inventory_screen.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int PICKER_COLS = 9, PICKER_ROWS = 5;
constexpr int PICKER_Y = 18;     // top of the creative picker
constexpr int CREATIVE_HOTBAR_Y = 112;

const Color PANEL{198 / 255.f, 198 / 255.f, 198 / 255.f, 1};
const Color SLOT{139 / 255.f, 139 / 255.f, 139 / 255.f, 1};
const Color DARK{55 / 255.f, 55 / 255.f, 55 / 255.f, 1};
const Color SHADOW{85 / 255.f, 85 / 255.f, 85 / 255.f, 1};
const Color WHITE{1, 1, 1, 1};
const Color BLACK{0, 0, 0, 1};
const Color LABEL{64 / 255.f, 64 / 255.f, 64 / 255.f, 1};

// GUI units -> window pixels: a helper that knows the scale and where the panel is.
struct Frame {
    Painter& p;
    int s, height;
    int left, top;  // panel corner in GUI units
    void rect(int x, int y, int w, int h, Color c) const {
        p.rect(static_cast<float>((left + x) * s), static_cast<float>(height - (top + y + h) * s), static_cast<float>(w * s),
               static_cast<float>(h * s), c);
    }
    float px(double x) const { return static_cast<float>((left + x) * s); }
    float py(double y) const { return static_cast<float>(height - (top + y) * s); }  // y of the top edge
    void label(const std::string& text, int x, int y) const {
        p.text(text, px(x), py(y), s, 0, 1, false, LABEL, false);
    }
    // Beta's raised panel: black outline, white top-left edge, grey bottom-right edge.
    void panel(int w, int h) const {
        rect(1, 0, w - 2, h, BLACK);
        rect(0, 1, w, h - 2, BLACK);
        rect(1, 1, w - 2, h - 2, PANEL);
        rect(1, 1, w - 3, 2, WHITE);
        rect(1, 1, 2, h - 3, WHITE);
        rect(3, h - 3, w - 4, 2, SHADOW);
        rect(w - 3, 3, 2, h - 4, SHADOW);
    }
    // A sunken 18x18 slot around the 16x16 item at (x, y).
    void slot(int x, int y) const {
        rect(x - 1, y - 1, 18, 18, DARK);
        rect(x, y, 17, 17, WHITE);
        rect(x, y, 16, 16, SLOT);
    }
    // 22 x 15, pointing right. Only the first `width` columns are drawn (a furnace's progress).
    void arrow(int x, int y, Color c = SLOT, int width = 22) const {
        rect(x, y + 5, std::min(15, width), 5, c);
        for (int i = 0; i < 7 && 15 + i < width; ++i) rect(x + 15 + i, y + i, 1, 15 - 2 * i, c);
    }
    // A furnace's 14 x 14 flame, lit from the bottom up to `lit` rows.
    void flame(int x, int y, int lit) const {
        static const char* ROWS[14] = {
            "......#.......", ".....##.......", ".....###......", "....####......", "....####..#...",
            "...#####.##...", "..#######.##..", "..##########..", ".############.", ".############.",
            "##############", "##############", ".############.", "..##########..",
        };
        for (int r = 0; r < 14; ++r)
            for (int c = 0; c < 14; ++c) {
                if (ROWS[r][c] != '#') continue;
                const bool on = r >= 14 - lit;
                const Color fire = r < 6 ? Color{1, 0.85f, 0.25f, 1} : r < 10 ? Color{1, 0.55f, 0.1f, 1} : Color{0.85f, 0.25f, 0.05f, 1};
                rect(x + c, y + r, 1, 1, on ? fire : SLOT);
            }
    }
};

}  // namespace

InventoryScreen::InventoryScreen(Kind k, Inventory& inventory) : kind(k) {
    if (kind == Kind::Creative)
        creative.emplace(inventory);
    else
        crafting.emplace(inventory, kind == Kind::CraftingTable ? 3 : 2);
}

InventoryScreen::InventoryScreen(Inventory& inventory, Furnace& furnace) : kind(Kind::Furnace) {
    crafting.emplace(inventory, furnace);
}

int InventoryScreen::gui_scale(int width, int height) {
    // Like Beta's automatic GUI scale: as big as fits a 320 x 240 screen.
    int s = 1;
    while (width / (s + 1) >= 320 && height / (s + 1) >= 240) ++s;
    return s;
}

int InventoryScreen::panel_width() const { return kind == Kind::Creative ? 195 : 176; }
int InventoryScreen::panel_height() const { return kind == Kind::Creative ? 136 : 166; }

int InventoryScreen::rows() const {
    return (static_cast<int>(all_items().size()) + PICKER_COLS - 1) / PICKER_COLS;
}

std::vector<InventoryScreen::SlotPos> InventoryScreen::slot_positions() const {
    std::vector<SlotPos> out;
    if (kind == Kind::Creative) {
        for (int c = 0; c < Inventory::HOTBAR; ++c) out.push_back({{Slot::Inv, c}, 8 + c * 18, CREATIVE_HOTBAR_Y});
        return out;
    }
    if (kind == Kind::Inventory) {
        for (int i = 0; i < Inventory::ARMOR; ++i) out.push_back({{Slot::Armor, i}, 8, 8 + i * 18});
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 2; ++c) out.push_back({{Slot::Grid, r * 2 + c}, 88 + c * 18, 26 + r * 18});
        out.push_back({{Slot::Result, 0}, 144, 36});
    } else if (kind == Kind::Furnace) {
        out.push_back({{Slot::Furnace, FURNACE_INPUT}, 56, 17});
        out.push_back({{Slot::Furnace, FURNACE_FUEL}, 56, 53});
        out.push_back({{Slot::Furnace, FURNACE_OUTPUT}, 116, 35});
    } else {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) out.push_back({{Slot::Grid, r * 3 + c}, 30 + c * 18, 17 + r * 18});
        out.push_back({{Slot::Result, 0}, 124, 35});
    }
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 9; ++c) out.push_back({{Slot::Inv, 9 + r * 9 + c}, 8 + c * 18, 84 + r * 18});
    for (int c = 0; c < 9; ++c) out.push_back({{Slot::Inv, c}, 8 + c * 18, 142});
    return out;
}

int InventoryScreen::picker_index(double gx, double gy) const {
    if (kind != Kind::Creative) return -1;
    int c = static_cast<int>(std::floor((gx - 7) / 18)), r = static_cast<int>(std::floor((gy - PICKER_Y + 1) / 18));
    if (gx < 7 || gy < PICKER_Y - 1 || c >= PICKER_COLS || r >= PICKER_ROWS) return -1;
    int index = (scroll_row_ + r) * PICKER_COLS + c;
    return index < static_cast<int>(all_items().size()) ? index : -1;
}

InventoryScreen::Hover InventoryScreen::hover(int width, int height, double mx, double my) const {
    const int s = gui_scale(width, height);
    const double left = std::floor((width / s - panel_width()) / 2.0), top = std::floor((height / s - panel_height()) / 2.0);
    const double gx = mx / s - left, gy = my / s - top;
    Hover h;
    h.inside = gx >= 0 && gy >= 0 && gx < panel_width() && gy < panel_height();
    for (const SlotPos& sp : slot_positions())
        if (gx >= sp.x - 1 && gx < sp.x + 17 && gy >= sp.y - 1 && gy < sp.y + 17) h.slot = sp;
    if (!h.slot) h.picker = picker_index(gx, gy);
    return h;
}

const std::optional<Stack>& InventoryScreen::cursor() const { return crafting ? crafting->cursor : creative->cursor; }

std::vector<Stack> InventoryScreen::click(int width, int height, double mx, double my, int button, bool shift) {
    Hover h = hover(width, height, mx, my);
    std::vector<Stack> thrown;
    if (creative) {
        if (h.slot)
            creative->click_hotbar(h.slot->slot.index, button, shift);
        else if (h.picker >= 0)
            creative->click_item(h.picker, button, shift);
        else if (!h.inside && creative->cursor) {
            thrown.push_back(*creative->cursor);
            creative->cursor.reset();
        }
        return thrown;
    }
    if (h.slot)
        crafting->click(h.slot->slot, button, shift);
    else if (!h.inside)
        if (auto t = crafting->click_outside(button)) thrown.push_back(*t);
    return thrown;
}

std::optional<Stack> InventoryScreen::drop_hovered(int width, int height, double mx, double my) {
    Hover h = hover(width, height, mx, my);
    if (!h.slot || cursor()) return std::nullopt;
    if (crafting) return crafting->drop_one(h.slot->slot);
    auto& slot = creative->inv.slots[h.slot->slot.index];
    if (!slot) return std::nullopt;
    Stack one = *slot;
    one.count = 1;
    if (--slot->count == 0) slot.reset();
    return one;
}

void InventoryScreen::scroll(double dy) {
    if (kind != Kind::Creative || dy == 0) return;
    scroll_row_ = std::clamp(scroll_row_ + (dy > 0 ? -1 : 1), 0, std::max(0, rows() - PICKER_ROWS));
}

std::vector<Stack> InventoryScreen::close() {
    if (creative) {
        creative->close();
        return {};
    }
    return crafting->close();
}

void InventoryScreen::draw(Painter& p, int width, int height, double mx, double my, const PlayerPainter& draw_player) {
    const int s = gui_scale(width, height);
    const int pw = panel_width(), ph = panel_height();
    Frame f{p, s, height, (width / s - pw) / 2, (height / s - ph) / 2};
    const Hover h = hover(width, height, mx, my);

    p.rect(0, 0, static_cast<float>(width), static_cast<float>(height), 16 / 255.f, 16 / 255.f, 16 / 255.f, 0.75f);
    f.panel(pw, ph);
    for (const SlotPos& sp : slot_positions()) {
        if (sp.slot.kind == Slot::Furnace && sp.slot.index == FURNACE_OUTPUT) {  // the big output slot
            f.rect(sp.x - 5, sp.y - 5, 26, 26, DARK);
            f.rect(sp.x - 4, sp.y - 4, 25, 25, WHITE);
            f.rect(sp.x - 4, sp.y - 4, 24, 24, SLOT);
        } else {
            f.slot(sp.x, sp.y);
        }
    }

    if (kind == Kind::Inventory) {
        f.label("Crafting", 86, 16);
        f.arrow(116, 36);
        // The player, in a dark box, turning to look at the mouse.
        f.rect(25, 7, 53, 72, DARK);
        f.rect(26, 8, 52, 71, WHITE);
        f.rect(26, 8, 51, 70, BLACK);
        p.flush();
        const float bx = f.px(26), by = f.py(78), bw = 51.0f * s, bh = 70.0f * s;
        draw_player(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), static_cast<int>(bh),
                    static_cast<float>((mx - (bx + bw / 2)) / bh), static_cast<float>((my - (height - by - bh * 0.8f)) / bh));
        p.begin(width, height);
    } else if (kind == Kind::Furnace) {
        const Furnace& fur = *crafting->furnace;
        f.label("Furnace", 60, 6);
        f.label("Inventory", 8, 72);
        // The flame shows how much of the fuel is left, the arrow how far the item is cooked.
        const int lit = fur.burning() ? 1 + fur.burn_left * 13 / std::max(1, fur.burn_total) : 0;
        f.flame(56, 36, std::min(14, lit));
        f.arrow(79, 34);
        f.arrow(79, 34, WHITE, fur.cook * 24 / SMELT_TICKS);
    } else if (kind == Kind::CraftingTable) {
        f.label("Crafting", 28, 6);
        f.arrow(90, 35);
        f.label("Inventory", 8, 72);
    } else {
        f.label("Items", 8, 6);
        for (int r = 0; r < PICKER_ROWS; ++r)
            for (int c = 0; c < PICKER_COLS; ++c) f.slot(8 + c * 18, PICKER_Y + r * 18);
        // Scroll bar
        f.rect(175, PICKER_Y - 1, 12, PICKER_ROWS * 18, DARK);
        const int total = std::max(1, rows() - PICKER_ROWS);
        const int thumb_h = std::max(10, PICKER_ROWS * 18 * PICKER_ROWS / std::max(PICKER_ROWS, rows()));
        const int thumb_y = PICKER_Y - 1 + (PICKER_ROWS * 18 - thumb_h) * scroll_row_ / total;
        f.rect(176, thumb_y, 10, thumb_h, PANEL);
        f.rect(176, thumb_y + thumb_h - 1, 10, 1, SHADOW);
        const auto& items = all_items();
        for (int r = 0; r < PICKER_ROWS; ++r)
            for (int c = 0; c < PICKER_COLS; ++c) {
                int index = (scroll_row_ + r) * PICKER_COLS + c;
                if (index >= static_cast<int>(items.size())) continue;
                p.item(items[index], f.px(8 + c * 18), f.py(PICKER_Y + r * 18 + 16), 16.0f * s);
            }
    }

    // Items in the slots
    for (const SlotPos& sp : slot_positions()) {
        const std::optional<Stack>* stack;
        if (creative)
            stack = &creative->inv.slots[sp.slot.index];
        else
            stack = &crafting->at(sp.slot);
        if (*stack) p.stack(**stack, f.px(sp.x), f.py(sp.y + 16), 16.0f * s, s);
    }

    // Highlight the slot under the mouse, then draw the held stack and the name of what's under the mouse.
    std::optional<ItemId> hovered_item;
    if (h.slot) {
        f.rect(h.slot->x, h.slot->y, 16, 16, {1, 1, 1, 0.5f});
        const std::optional<Stack>& st = creative ? creative->inv.slots[h.slot->slot.index] : crafting->at(h.slot->slot);
        if (st) hovered_item = st->item;
    } else if (h.picker >= 0) {
        int r = h.picker / PICKER_COLS - scroll_row_, c = h.picker % PICKER_COLS;
        f.rect(8 + c * 18, PICKER_Y + r * 18, 16, 16, {1, 1, 1, 0.5f});
        hovered_item = all_items()[h.picker];
    }
    const float mouse_x = static_cast<float>(mx), mouse_y = static_cast<float>(height - my);
    if (const auto& held = cursor()) {
        p.stack(*held, mouse_x - 8.0f * s, mouse_y - 8.0f * s, 16.0f * s, s);
    } else if (hovered_item) {
        std::string name = item_name(*hovered_item);
        float tw = Painter::text_width(name, s), tx = mouse_x + 12.0f * s, ty = mouse_y + 6.0f * s;
        p.rect(tx - 3.0f * s, ty - 10.0f * s, tw + 6.0f * s, 13.0f * s, 16 / 255.f, 0, 16 / 255.f, 0.94f);
        p.text(name, tx, ty, s, 0, 1);
    }
}
