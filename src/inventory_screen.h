// The inventory (E), crafting table and creative item picker screens, drawn Beta-style by the code
// over the running game. The rules for moving stacks are in container.h.
#pragma once
#include <functional>
#include <optional>
#include <vector>

#include "container.h"
#include "ui.h"

class InventoryScreen {
public:
    enum class Kind { Inventory, CraftingTable, Creative };

    // Inventory and CraftingTable work on the survival inventory; Creative on the creative hotbar.
    InventoryScreen(Kind kind, Inventory& inventory);

    const Kind kind;
    std::optional<CraftingMenu> crafting;
    std::optional<CreativeMenu> creative;

    // Draws the player model into a box (window pixels, bottom-left origin), looking towards (look_x, look_y),
    // which is the mouse position relative to the box's centre in box heights.
    using PlayerPainter = std::function<void(int x, int y, int w, int h, float look_x, float look_y)>;

    // The mouse is in window pixels from the top left, as GLFW reports it scaled to the framebuffer.
    void draw(Painter& p, int width, int height, double mx, double my, const PlayerPainter& draw_player);
    // Returns any stacks thrown out of the window.
    std::vector<Stack> click(int width, int height, double mx, double my, int button, bool shift);
    // Q over a slot: returns the item to throw.
    std::optional<Stack> drop_hovered(int width, int height, double mx, double my);
    void scroll(double dy);
    // Returns what has to be dropped on the ground because the inventory is full.
    std::vector<Stack> close();
    const std::optional<Stack>& cursor() const;

    static int gui_scale(int width, int height);

    // Where things are, in GUI units from the panel's top left (public for the tests).
    struct SlotPos {
        Slot slot;
        int x, y;
    };
    std::vector<SlotPos> slot_positions() const;
    int panel_width() const;
    int panel_height() const;
    // Creative picker cell under (gx, gy) in GUI units from the panel's top left: an index into all_items(), or -1.
    int picker_index(double gx, double gy) const;

private:
    struct Hover {
        std::optional<SlotPos> slot;
        int picker = -1;  // creative picker item index
        bool inside = false;
    };
    Hover hover(int width, int height, double mx, double my) const;
    int rows() const;  // creative picker rows of items

    int scroll_row_ = 0;
};
