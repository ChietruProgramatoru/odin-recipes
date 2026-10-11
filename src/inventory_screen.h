// The inventory (E), crafting table, furnace, chest and creative item picker screens, drawn Beta-style by the code
// over the running game. The rules for moving stacks are in container.h.
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "container.h"
#include "ui.h"

class InventoryScreen {
public:
    enum class Kind { Inventory, CraftingTable, Creative, Furnace, Chest, Dispenser };

    // Inventory and CraftingTable work on the survival inventory; Creative on the creative hotbar.
    InventoryScreen(Kind kind, Inventory& inventory);
    // A furnace's screen.
    InventoryScreen(Inventory& inventory, Furnace& furnace);
    // A chest's screen with its title ("Chest", "Large chest", "Minecart"); bottom: the lower half
    // of a double chest.
    InventoryScreen(Inventory& inventory, ChestSlots& top, ChestSlots* bottom, std::string title);
    // A dispenser's screen: its 9 slots (the first of `slots`) in a 3 x 3 square.
    InventoryScreen(Inventory& inventory, ChestSlots& slots);

    const Kind kind;
    std::optional<CraftingMenu> crafting;
    std::optional<CreativeMenu> creative;
    std::string title;  // a chest's

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
    int chest_rows() const;  // 3 for a chest, 6 for a double chest

    int scroll_row_ = 0;
};
