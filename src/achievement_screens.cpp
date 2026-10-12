// The achievement map and the statistics screen (M17), opened from the game menu. Laid out like
// Beta's GuiAchievements and GuiStats, in menu units.
#include <algorithm>
#include <cmath>
#include <set>

#include "achievements.h"
#include "app.h"
#include "blocks.h"
#include "items.h"
#include "menu.h"
#include "rng.h"

namespace {

// An item's icon in menu units (top-left origin).
void gui_item(Gui& g, ItemId item, float x, float y, float size = 16) {
    g.p.item(item, x * g.s, static_cast<float>(g.p.height()) - (y + size) * g.s, size * g.s);
}

void gui_tile(Gui& g, int tile, float x, float y, float size, Color tint) {
    g.p.tile(tile, x * g.s, static_cast<float>(g.p.height()) - (y + size) * g.s, size * g.s, size * g.s, tint);
}

// Only draw inside a rectangle given in menu units.
void gui_clip(Gui& g, float x, float y, float w, float h) {
    g.p.clip(static_cast<int>(x * g.s), static_cast<int>(static_cast<float>(g.p.height()) - (y + h) * g.s),
             static_cast<int>(w * g.s), static_cast<int>(h * g.s));
}

// Words of `text` wrapped into lines no wider than `width` menu units.
std::vector<std::string> wrap(const std::string& text, float width) {
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i < text.size()) {
        size_t space = text.find(' ', i);
        std::string word = text.substr(i, space == std::string::npos ? std::string::npos : space - i);
        i = space == std::string::npos ? text.size() : space + 1;
        std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && Painter::text_width(longer, 1) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = longer;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

int block_tile(BlockId b) { return block_info(b)->tiles[1]; }

// --- Achievements -----------------------------------------------------------------------------

class AchievementsScreen : public Screen {
public:
    AchievementsScreen(App& a, ScreenFactory back) : Screen(a), back_(std::move(back)) {
        // Beta starts with Taking Inventory near the top left of the map.
        const AchievementInfo& first = achievement_info(ACH_OPEN_INVENTORY);
        map_x_ = first.col * STEP - 141 / 2.0 - 12;
        map_y_ = first.row * STEP - 141 / 2.0;
    }
    enum { DONE };
    static constexpr int W = 256, H = 202, MAP_W = 224, MAP_H = 155, STEP = 24;

    void layout(int gw, int gh) override {
        x0_ = (gw - W) / 2;
        y0_ = (gh - H) / 2;
        widgets.push_back(button(DONE, gw / 2 + 24, gh / 2 + 74, 80, "Done"));
    }
    void on_button(int id) override {
        if (id == DONE) app.open(back_());
    }
    void on_escape() override { app.open(back_()); }
    bool over_game() const override { return app.game != nullptr; }
    void on_click(double gx, double gy) override { drag_ok_ = in_map(gx, gy); }
    void on_drag(double dx, double dy) override {
        if (!drag_ok_) return;
        map_x_ -= dx;
        map_y_ -= dy;
        clamp();
    }

    void draw(Gui& g) override {
        clamp();
        const float mx = static_cast<float>(x0_ + 16), my = static_cast<float>(y0_ + 17);
        const double ox = std::floor(map_x_), oy = std::floor(map_y_);
        // The window: a light frame around the map, with the title above it.
        g.rect(static_cast<float>(x0_), static_cast<float>(y0_), W, H, {0.78f, 0.78f, 0.78f, 1});
        g.rect(static_cast<float>(x0_ + 2), static_cast<float>(y0_ + 2), W - 4, H - 4, {0.6f, 0.6f, 0.6f, 1});
        g.rect(mx - 1, my - 1, MAP_W + 2, MAP_H + 2, {0.2f, 0.2f, 0.2f, 1});
        g.text("Achievements", static_cast<float>(x0_ + 15), static_cast<float>(y0_ + 6), {0.25f, 0.25f, 0.25f, 1});
        const Stats& st = app.stats;
        g.text(std::to_string(st.earned_count()) + "/" + std::to_string(ACHIEVEMENT_COUNT), static_cast<float>(x0_ + W - 15),
               static_cast<float>(y0_ + 6), {0.25f, 0.25f, 0.25f, 1}, 1);

        gui_clip(g, mx, my, MAP_W, MAP_H);
        draw_ground(g, mx, my, ox, oy);
        const long frame = app.frame;
        const bool pulse = std::sin((frame % 36) / 36.0 * 2 * 3.14159265) > 0.6;  // Beta's blinking path
        // The paths from each achievement to the one before it.
        for (int a = 0; a < ACHIEVEMENT_COUNT; ++a) {
            const AchievementInfo& info = achievement_info(a);
            if (info.parent < 0) continue;
            const AchievementInfo& par = achievement_info(info.parent);
            const float ax = static_cast<float>(info.col * STEP - ox + 11) + mx, ay = static_cast<float>(info.row * STEP - oy + 11) + my;
            const float px = static_cast<float>(par.col * STEP - ox + 11) + mx, py = static_cast<float>(par.row * STEP - oy + 11) + my;
            Color c = st.earned[a] ? Color{0.44f, 0.44f, 0.44f, 1}
                      : st.can_earn(a) ? Color{0, pulse ? 1.0f : 0.51f, 0, 1}
                                       : Color{0, 0, 0, 1};
            g.rect(std::min(ax, px), ay, std::abs(ax - px) + 1, 1, c);
            g.rect(px, std::min(ay, py), 1, std::abs(ay - py) + 1, c);
        }
        // The achievements: a frame and the icon, dark until they can be earned.
        int hovered = -1;
        for (int a = 0; a < ACHIEVEMENT_COUNT; ++a) {
            const AchievementInfo& info = achievement_info(a);
            const float ax = static_cast<float>(info.col * STEP - ox) + mx, ay = static_cast<float>(info.row * STEP - oy) + my;
            if (ax < mx - 24 || ay < my - 24 || ax > mx + MAP_W || ay > my + MAP_H) continue;
            float bright = st.earned[a] ? 1.0f : st.can_earn(a) ? (pulse ? 0.75f : 0.6f) : 0.3f;
            draw_frame(g, ax - 2, ay - 2, info.special, bright);
            gui_item(g, info.icon, ax + 3, ay + 3);
            if (!st.earned[a]) g.rect(ax + 3, ay + 3, 16, 16, {0, 0, 0, st.can_earn(a) ? 0.35f : 0.8f});
            if (mouse_x >= ax && mouse_x < ax + 22 && mouse_y >= ay && mouse_y < ay + 22 && in_map(mouse_x, mouse_y)) hovered = a;
        }
        g.p.clip_off();
        if (hovered >= 0) tooltip(g, hovered);
    }

private:
    bool in_map(double gx, double gy) const {
        return gx >= x0_ + 16 && gx < x0_ + 16 + MAP_W && gy >= y0_ + 17 && gy < y0_ + 17 + MAP_H;
    }
    void clamp() {
        map_x_ = std::clamp(map_x_, ACH_MAP_MIN_COL * STEP - 112.0, ACH_MAP_MAX_COL * STEP - 77.0);
        map_y_ = std::clamp(map_y_, ACH_MAP_MIN_ROW * STEP - 112.0, ACH_MAP_MAX_ROW * STEP - 77.0);
    }
    // Beta's background: sand and dirt at the top, stone with ores further down, bedrock at the
    // bottom, darker the deeper it goes. It scrolls with the map.
    void draw_ground(Gui& g, float mx, float my, double ox, double oy) {
        const int sx = static_cast<int>(ox) + 288, sy = static_cast<int>(oy) + 288;
        const int col0 = sx >> 4, row0 = sy >> 4, fx = sx & 15, fy = sy & 15;
        for (int r = 0; r * 16 - fy < MAP_H; ++r) {
            const int depth = row0 + r;
            const float f = std::max(0.1f, 0.6f - depth / 25.0f * 0.3f);
            for (int c = 0; c * 16 - fx < MAP_W; ++c) {
                Rng rng(1234 + static_cast<uint64_t>(col0 + c) * 7919 + static_cast<uint64_t>(depth) * 104729);
                const int v = rng.below(1 + std::max(0, depth)) + depth / 2;
                BlockId b = SAND;
                if (v > 37 || depth == 35) b = BEDROCK;
                else if (v == 22) b = rng.below(2) ? DIAMOND_ORE : REDSTONE_ORE;
                else if (v == 10) b = IRON_ORE;
                else if (v == 8) b = COAL_ORE;
                else if (v > 4) b = STONE;
                else if (v > 0) b = DIRT;
                gui_tile(g, block_tile(b), mx + c * 16 - fx, my + r * 16 - fy, 16, {f, f, f, 1});
            }
        }
    }
    // A 26 x 26 frame: square, or with notched corners for the special ones.
    void draw_frame(Gui& g, float x, float y, bool special, float bright) {
        const Color edge{0.1f * bright, 0.1f * bright, 0.1f * bright, 1};
        const Color light{0.85f * bright, 0.85f * bright, 0.85f * bright, 1};
        const Color fill{0.55f * bright, 0.55f * bright, 0.55f * bright, 1};
        if (special) {
            // Corners cut off, a little star shape like Beta's.
            g.rect(x + 3, y, 20, 26, edge);
            g.rect(x, y + 3, 26, 20, edge);
            g.rect(x + 4, y + 1, 18, 24, light);
            g.rect(x + 1, y + 4, 24, 18, light);
            g.rect(x + 4, y + 2, 18, 22, fill);
            g.rect(x + 2, y + 4, 22, 18, fill);
        } else {
            g.rect(x, y, 26, 26, edge);
            g.rect(x + 1, y + 1, 24, 24, light);
            g.rect(x + 2, y + 2, 22, 22, fill);
        }
    }
    void tooltip(Gui& g, int a) {
        const Stats& st = app.stats;
        const AchievementInfo& info = achievement_info(a);
        const bool can = st.can_earn(a);
        const std::string desc =
            can ? std::string(info.description) : "Requires '" + std::string(achievement_info(info.parent).name) + "'";
        const float w = std::max(120.0f, Painter::text_width(info.name, 1));
        std::vector<std::string> lines = wrap(desc, w);
        if (st.earned[a]) lines.push_back("");
        float x = static_cast<float>(mouse_x) + 12, y = static_cast<float>(mouse_y) - 4;
        const float h = 12 + lines.size() * 9.0f;
        g.rect(x - 3, y - 3, w + 6, h + 3, {0.06f, 0.06f, 0.06f, 0.94f});
        Color name = can ? (info.special ? Color{1, 1, 0.5f, 1} : Color{1, 1, 1, 1})
                         : (info.special ? Color{0.5f, 0.5f, 0.25f, 1} : Color{0.5f, 0.5f, 0.5f, 1});
        g.text(info.name, x, y, name);
        for (size_t i = 0; i < lines.size(); ++i)
            g.text(lines[i], x, y + 12 + i * 9.0f, can ? Color{0.63f, 0.63f, 0.63f, 1} : Color{0.44f, 0.27f, 0.27f, 1});
        if (st.earned[a]) g.text("Taken!", x, y + 12 + (lines.size() - 1) * 9.0f, {0.56f, 0.56f, 1, 1});
    }

    ScreenFactory back_;
    int x0_ = 0, y0_ = 0;
    double map_x_ = 0, map_y_ = 0;
    bool drag_ok_ = false;
};

// --- Statistics -------------------------------------------------------------------------------

class StatisticsScreen : public Screen {
public:
    StatisticsScreen(App& a, ScreenFactory back) : Screen(a), back_(std::move(back)) {}
    enum { DONE, GENERAL, BLOCKS, ITEMS };
    static constexpr int TOP = 32;

    void layout(int gw, int gh) override {
        gw_ = gw;
        gh_ = gh;
        widgets.push_back(button(DONE, gw / 2 + 4, gh - 28, 150, "Done"));
        widgets.push_back(button(GENERAL, gw / 2 - 154, gh - 52, 100, "General", list_ != GENERAL));
        widgets.push_back(button(BLOCKS, gw / 2 - 46, gh - 52, 100, "Blocks", list_ != BLOCKS));
        widgets.push_back(button(ITEMS, gw / 2 + 62, gh - 52, 100, "Items", list_ != ITEMS));
    }
    void on_button(int id) override {
        if (id == DONE) {
            app.open(back_());
            return;
        }
        list_ = id;
        scroll_ = 0;
        for (int b : {GENERAL, BLOCKS, ITEMS})
            if (Widget* w = find(b)) w->enabled = b != list_;
    }
    void on_escape() override { app.open(back_()); }
    void on_scroll(double dy) override { scroll_ -= static_cast<float>(dy) * 20; }
    bool over_game() const override { return app.game != nullptr; }
    int showing() const { return list_; }

    void draw(Gui& g) override {
        const float bottom = static_cast<float>(gh_ - 64);
        g.text("Statistics", gw_ / 2.0f, 20, TEXT_WHITE, 0.5f);
        g.rect(0, TOP, static_cast<float>(gw_), bottom - TOP, {0, 0, 0, 0.35f});
        gui_clip(g, 0, TOP, static_cast<float>(gw_), bottom - TOP);
        const float left = gw_ / 2.0f - 140, right = gw_ / 2.0f + 140;
        if (list_ == GENERAL) {
            const float rows = static_cast<int>(STAT_COUNT) * 10.0f;
            scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, rows + 8 - (bottom - TOP)));
            for (int s = 0; s < STAT_COUNT; ++s) {
                const float y = TOP + 4 + s * 10.0f - scroll_;
                if (s % 2) g.rect(left - 4, y - 1, right - left + 8, 10, {1, 1, 1, 0.06f});
                g.text(stat_info(s).name, left, y, s % 2 ? TEXT_GRAY : TEXT_WHITE);
                g.text(format_stat(stat_info(s).unit, app.stats.general[s]), right, y, s % 2 ? TEXT_GRAY : TEXT_WHITE, 1);
            }
        } else {
            draw_table(g, left, right, bottom);
        }
        g.p.clip_off();
    }

private:
    struct Row {
        ItemId item;
        int64_t a, b;
    };
    // Blocks: crafted and mined. Items: crafted and worn out. Only what has a count, by id.
    std::vector<Row> rows() const {
        const Stats& st = app.stats;
        std::set<ItemId> ids;
        auto is_block = [](ItemId id) { return id < 256 && block_info(id); };
        if (list_ == BLOCKS) {
            for (const auto& [id, n] : st.mined) ids.insert(id);
            for (const auto& [id, n] : st.crafted)
                if (is_block(id)) ids.insert(id);
        } else {
            for (const auto& [id, n] : st.crafted)
                if (!is_block(id)) ids.insert(id);
            for (const auto& [id, n] : st.depleted) ids.insert(id);
        }
        auto get = [](const auto& m, ItemId id) {
            auto it = m.find(id);
            return it == m.end() ? int64_t{0} : it->second;
        };
        std::vector<Row> out;
        for (ItemId id : ids) {
            if (list_ == BLOCKS)
                out.push_back({id, get(st.crafted, id), get(st.mined, static_cast<BlockId>(id))});
            else
                out.push_back({id, get(st.crafted, id), get(st.depleted, id)});
        }
        return out;
    }
    void draw_table(Gui& g, float left, float right, float bottom) {
        const std::vector<Row> list = rows();
        const char* head_a = "Crafted";
        const char* head_b = list_ == BLOCKS ? "Mined" : "Used up";
        const float col_a = right - 60, col_b = right;
        g.text(head_a, col_a, TOP + 4, TEXT_YELLOW, 1);
        g.text(head_b, col_b, TOP + 4, TEXT_YELLOW, 1);
        const float first = TOP + 16;
        scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, list.size() * 20.0f + 20 - (bottom - TOP)));
        if (list.empty())
            g.text(list_ == BLOCKS ? "No blocks mined or crafted yet" : "No items crafted or used up yet", gw_ / 2.0f, first + 10,
                   TEXT_DARK_GRAY, 0.5f);
        for (size_t i = 0; i < list.size(); ++i) {
            const Row& r = list[i];
            const float y = first + i * 20.0f - scroll_;
            if (y + 20 < TOP || y > bottom) continue;
            if (i % 2) g.rect(left - 4, y, right - left + 8, 20, {1, 1, 1, 0.06f});
            // Blocks that are no item (a lit furnace, a door half, a crop) show what they drop.
            ItemId icon = r.item;
            std::string name;
            if (list_ == BLOCKS && block_info(r.item)) {
                name = block_info(r.item)->name;
                if (!valid_item(icon) && block_info(r.item)->drop) icon = *block_info(r.item)->drop;
            } else {
                name = item_name(r.item);
            }
            g.rect(left, y + 1, 18, 18, {0, 0, 0, 0.4f});
            if (valid_item(icon)) gui_item(g, icon, left + 1, y + 2);
            g.text(name, left + 24, y + 6, TEXT_WHITE);
            g.text(r.a ? std::to_string(r.a) : "-", col_a, y + 6, TEXT_WHITE, 1);
            g.text(r.b ? std::to_string(r.b) : "-", col_b, y + 6, TEXT_WHITE, 1);
        }
    }

    ScreenFactory back_;
    int gw_ = 0, gh_ = 0;
    int list_ = GENERAL;
    float scroll_ = 0;
};

}  // namespace

std::unique_ptr<Screen> achievements_screen(App& app, ScreenFactory back) {
    return std::make_unique<AchievementsScreen>(app, std::move(back));
}

std::unique_ptr<Screen> statistics_screen(App& app, ScreenFactory back) {
    return std::make_unique<StatisticsScreen>(app, std::move(back));
}
