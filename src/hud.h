// On-screen interface: crosshair, hotbar, hearts, hunger and overlays.
#pragma once
#include <deque>
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
    // A hint in the achievement pop-up's place while nothing pops up (Beta's "Press 'E' to open
    // your inventory." until Taking Inventory is earned): its achievement, and the text under it.
    int hint = -1;
    std::string hint_text = "";
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
    // "Achievement get!": slides down in the top right corner for a few seconds.
    void show_achievement(int achievement);
    // The achievement popping up now, or -1.
    int showing_achievement() const { return toasts_.empty() ? -1 : toasts_.front(); }
    void update(float dt);
    void draw(int width, int height, const HudState& state);
    // The achievement pop-up (or hint), over everything else, open screens included.
    void draw_achievements(int width, int height, const HudState& state);

private:
    Painter& p_;
    std::string message_, item_name_;
    float message_timer_ = 0, item_name_timer_ = 0, damage_alpha_ = 0;
    std::optional<float> mining_;
    std::deque<int> toasts_;  // the achievement popping up (none or one)
    float toast_time_ = 0;    // seconds the first one has been showing
    void draw_toast(int achievement, float slide, const std::string& title, const std::string& text, Color title_color);
};
