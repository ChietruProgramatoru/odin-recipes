#include "hud.h"

#include <algorithm>
#include <cmath>

namespace {
// Pyglet font size (points) -> integer pixel scale for the 7-pixel-tall font.
int font_scale(int points) { return std::max(1, (points * 4 / 3 + 4) / 8); }
int text_lines(const std::string& s) { return 1 + static_cast<int>(std::count(s.begin(), s.end(), '\n')); }
}  // namespace

void Hud::show_message(const std::string& text, float seconds) {
    message_ = text;
    message_timer_ = seconds;
}

void Hud::show_item_name(std::optional<ItemId> item) {
    item_name_ = item ? item_name(*item) : "";
    item_name_timer_ = 2.0f;
}

void Hud::update(float dt) {
    item_name_timer_ -= dt;
    message_timer_ -= dt;
    damage_alpha_ = std::max(0.0f, damage_alpha_ - dt * 1.2f);
}

// --- The HUD ----------------------------------------------------------------------------

void Hud::draw(int width, int height, const HudState& st) {
    p_.begin(width, height);
    const float w = static_cast<float>(p_.width()), h = static_cast<float>(p_.height());
    const float slot = static_cast<float>(std::max(36, std::min(64, static_cast<int>(std::min(w / 14, h / 9)))));
    const float x0 = w / 2 - slot * 4.5f, y0 = 8;

    if (st.underwater) p_.rect(0, 0, w, h, 25 / 255.f, 64 / 255.f, 180 / 255.f, 110 / 255.f);
    if (damage_alpha_ > 0) p_.rect(0, 0, w, h, 200 / 255.f, 0, 0, damage_alpha_);

    const float ca = 220 / 255.f;
    p_.rect(w / 2 - 10, h / 2 - 1, 20, 2, 1, 1, 1, ca);
    p_.rect(w / 2 - 1, h / 2 - 10, 2, 20, 1, 1, 1, ca);

    // Hotbar
    const Inventory& inv = *st.inventory;
    p_.rect(x0 + inv.selected * slot, y0, slot, slot, 1, 1, 1, 240 / 255.f);
    for (int i = 0; i < Inventory::HOTBAR; ++i)
        p_.rect(x0 + i * slot + 3, y0 + 3, slot - 6, slot - 6, 25 / 255.f, 25 / 255.f, 25 / 255.f, 200 / 255.f);
    if (mining_) {
        p_.rect(w / 2 - 50, h / 2 - 34, 100, 8, 0, 0, 0, 150 / 255.f);
        p_.rect(w / 2 - 48, h / 2 - 32, 96 * std::min(1.0f, *mining_), 4, 1, 1, 1, 230 / 255.f);
    }
    const int count_scale = st.creative ? 0 : font_scale(std::max(9, static_cast<int>(slot) / 5));
    for (int i = 0; i < Inventory::HOTBAR; ++i)
        if (const auto& s = inv.slots[i]) p_.stack(*s, x0 + i * slot + slot * 0.18f, y0 + slot * 0.18f, slot * 0.64f, count_scale);

    // Hearts and hunger
    const float size = slot * 0.42f;
    const float row_y = y0 + slot + 6;
    if (!st.creative)
        for (int i = 0; i < 10; ++i) {
            int hp = st.vitals->health - i * 2;
            p_.icon(hp >= 2 ? "heart_full" : hp == 1 ? "heart_half" : "heart_empty", x0 + i * size * 1.05f, row_y, size);
            int fd = st.vitals->hunger - i * 2;
            p_.icon(fd >= 2 ? "food_full" : fd == 1 ? "food_half" : "food_empty", x0 + slot * 9 - (i + 1) * size * 1.05f, row_y, size);
            // Armor, above the hearts.
            int ar = st.armor - i * 2;
            if (st.armor > 0)
                p_.icon(ar >= 2 ? "armor_full" : ar == 1 ? "armor_half" : "armor_empty", x0 + i * size * 1.05f,
                        row_y + size * 1.15f, size);
        }

    if (item_name_timer_ > 0) p_.text(item_name_, w / 2, row_y + size * (st.armor > 0 && !st.creative ? 2.15f : 1.0f) + 16, font_scale(14), 0.5f, 0);
    if (message_timer_ > 0) p_.text(message_, w / 2, h * 0.68f, font_scale(18), 0.5f, 0);
    p_.text(st.info, 10, h - 10, font_scale(12), 0, 1);
    if (show_help) p_.text(st.help, 10, h - 16 - text_lines(st.info) * 10 * font_scale(12), font_scale(11), 0, 1);

    if (st.dead) {
        p_.rect(0, 0, w, h, 115 / 255.f, 0, 0, 150 / 255.f);
        p_.text("You died!\n\nPress R to respawn", w / 2, h / 2, font_scale(30), 0.5f, 0.5f, true);
    }
    p_.end();
}
