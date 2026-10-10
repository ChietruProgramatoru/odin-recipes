// On-screen interface: crosshair, hotbar, hearts, hunger and overlays.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "survival.h"
#include "ui.h"

struct HudState {
    const Inventory* inventory;
    const Vitals* vitals;
    int armor;  // the armor bar, 0-20 (shown only while armor is worn)
    bool creative;
    std::string info;   // top left: the game name, or the F3 debug lines
    std::string help;   // controls, while show_help is on
    bool underwater, dead;
    bool burning = false;  // the player is on fire
    int bubbles = -1;      // air bubbles to show (0-10) while under water, -1 for none
};

class Hud {
public:
    explicit Hud(Painter& painter) : p_(painter) {}

    bool show_help = true;

    void flash_damage() { damage_alpha_ = 0.45f; }
    void show_message(const std::string& text, float seconds = 2.5f);
    bool has_message() const { return message_timer_ > 0; }
    void show_item_name(std::optional<ItemId> item);
    void set_mining(std::optional<float> progress) { mining_ = progress; }
    void update(float dt);
    void draw(int width, int height, const HudState& state);

private:
    Painter& p_;
    std::string message_, item_name_;
    float message_timer_ = 0, item_name_timer_ = 0, damage_alpha_ = 0;
    std::optional<float> mining_;
};
