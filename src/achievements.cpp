#include "achievements.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "items.h"
#include "json.h"
#include "save.h"

namespace {

// Beta 1.7.3's AchievementList: names, descriptions, map places, icons and parents.
const AchievementInfo ACHIEVEMENTS[ACHIEVEMENT_COUNT] = {
    {"openInventory", "Taking Inventory", "Press 'E' to open your inventory.", 0, 0, BOOK, -1, false},
    {"mineWood", "Getting Wood", "Attack a tree until a block of wood pops out", 2, 1, LOG, ACH_OPEN_INVENTORY, false},
    {"buildWorkBench", "Benchmarking", "Craft a workbench with four blocks of planks", 4, -1, CRAFTING_TABLE, ACH_MINE_WOOD, false},
    {"buildPickaxe", "Time to Mine!", "Use planks and sticks to make a pickaxe", 4, 2, WOOD_PICKAXE, ACH_WORKBENCH, false},
    {"buildFurnace", "Hot Topic", "Construct a furnace out of eight stone blocks", 3, 4, FURNACE, ACH_PICKAXE, false},
    {"acquireIron", "Acquire Hardware", "Smelt an iron ingot", 1, 4, IRON_INGOT, ACH_FURNACE, false},
    {"buildHoe", "Time to Farm!", "Use planks and sticks to make a hoe", 2, -3, WOOD_HOE, ACH_WORKBENCH, false},
    {"makeBread", "Bake Bread", "Turn wheat into bread", -1, -3, BREAD, ACH_HOE, false},
    {"bakeCake", "The Lie", "Wheat, sugar, milk and eggs!", 0, -5, CAKE, ACH_HOE, false},
    {"buildBetterPickaxe", "Getting an Upgrade", "Construct a better pickaxe", 6, 2, STONE_PICKAXE, ACH_PICKAXE, false},
    {"cookFish", "Delicious Fish", "Catch and cook fish!", 2, 6, COOKED_FISH, ACH_FURNACE, false},
    {"onARail", "On A Rail", "Travel by minecart at least 1 km from where you started", 2, 3, RAIL, ACH_IRON, true},
    {"buildSword", "Time to Strike!", "Use planks and sticks to make a sword", 6, -1, WOOD_SWORD, ACH_WORKBENCH, false},
    {"killEnemy", "Monster Hunter", "Attack and destroy a monster", 8, -1, BONE, ACH_SWORD, false},
    {"killCow", "Cow Tipper", "Harvest some leather", 7, -3, LEATHER, ACH_SWORD, false},
    {"flyPig", "When Pigs Fly", "Fly a pig off a cliff", 8, -4, SADDLE, ACH_KILL_COW, true},
};

// Beta's StatList names for the general statistics.
const StatInfo STATS[STAT_COUNT] = {
    {"startGame", "Times played", StatUnit::Count},
    {"createWorld", "Worlds created", StatUnit::Count},
    {"loadWorld", "Saves loaded", StatUnit::Count},
    {"playOneMinute", "Time played", StatUnit::Time},
    {"walkOneCm", "Distance walked", StatUnit::Distance},
    {"swimOneCm", "Distance swum", StatUnit::Distance},
    {"fallOneCm", "Distance fallen", StatUnit::Distance},
    {"climbOneCm", "Distance climbed", StatUnit::Distance},
    {"flyOneCm", "Distance flown", StatUnit::Distance},
    {"diveOneCm", "Distance dove", StatUnit::Distance},
    {"minecartOneCm", "Distance by minecart", StatUnit::Distance},
    {"boatOneCm", "Distance by boat", StatUnit::Distance},
    {"pigOneCm", "Distance by pig", StatUnit::Distance},
    {"jump", "Jumps", StatUnit::Count},
    {"drop", "Items dropped", StatUnit::Count},
    {"damageDealt", "Damage dealt", StatUnit::Count},
    {"damageTaken", "Damage taken", StatUnit::Count},
    {"deaths", "Number of deaths", StatUnit::Count},
    {"mobKills", "Mob kills", StatUnit::Count},
    {"fishCaught", "Fish caught", StatUnit::Count},
};

std::string fixed(double v, int decimals, const char* unit) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.*f %s", decimals, v, unit);
    return buf;
}

// A map of counters as a JSON object keyed by id, and back.
template <class Key>
Json counters_to_json(const std::map<Key, int64_t>& m) {
    Json o = Json::object();
    for (const auto& [k, v] : m)
        if (v) o[std::to_string(k)] = Json(static_cast<long long>(v));
    return o;
}

template <class Key>
void counters_from_json(const Json* o, std::map<Key, int64_t>& m) {
    if (!o || !o->is_object()) return;
    for (const auto& [k, v] : o->members()) {
        char* end = nullptr;
        const long id = std::strtol(k.c_str(), &end, 10);
        if (end == k.c_str() || *end || id < 0 || id > 0xffff || !v.is_number() || v.as_number() <= 0) continue;
        m[static_cast<Key>(id)] = static_cast<int64_t>(v.as_number());
    }
}

}  // namespace

const AchievementInfo& achievement_info(int a) { return ACHIEVEMENTS[a]; }
const StatInfo& stat_info(int s) { return STATS[s]; }

std::optional<Achievement> achievement_for_craft(ItemId item) {
    switch (item) {
        case CRAFTING_TABLE: return ACH_WORKBENCH;
        case WOOD_PICKAXE: return ACH_PICKAXE;
        case FURNACE: return ACH_FURNACE;
        case WOOD_HOE: return ACH_HOE;
        case BREAD: return ACH_BREAD;
        case CAKE: return ACH_CAKE;
        case STONE_PICKAXE: return ACH_BETTER_PICKAXE;
        case WOOD_SWORD: return ACH_SWORD;
        default: return std::nullopt;
    }
}

std::optional<Achievement> achievement_for_smelt(ItemId item) {
    if (item == IRON_INGOT) return ACH_IRON;
    if (item == COOKED_FISH) return ACH_COOK_FISH;
    return std::nullopt;
}

std::optional<Achievement> achievement_for_pickup(ItemId item) {
    // Any of the three woods counts, and Beta gave Cow Tipper for picking up leather.
    if (item == LOG || item == BIRCH_LOG || item == SPRUCE_LOG) return ACH_MINE_WOOD;
    if (item == LEATHER) return ACH_KILL_COW;
    return std::nullopt;
}

std::string format_stat(StatUnit unit, int64_t value) {
    switch (unit) {
        case StatUnit::Time: {
            // Beta: the biggest unit with more than half of one.
            const double s = value / 20.0, min = s / 60, h = min / 60, d = h / 24, y = d / 365;
            if (y > 0.5) return fixed(y, 2, "y");
            if (d > 0.5) return fixed(d, 2, "d");
            if (h > 0.5) return fixed(h, 2, "h");
            if (min > 0.5) return fixed(min, 2, "min");
            return fixed(s, 0, "s");
        }
        case StatUnit::Distance: {
            const double m = value / 100.0, km = m / 1000;
            if (km > 0.5) return fixed(km, 2, "km");
            if (m > 0.5) return fixed(m, 1, "m");
            return std::to_string(value) + " cm";
        }
        case StatUnit::Count: break;
    }
    return std::to_string(value);
}

bool Stats::can_earn(int a) const {
    const int parent = achievement_info(a).parent;
    return parent < 0 || earned[parent];
}

bool Stats::earn(Achievement a) {
    if (earned[a] || !can_earn(a)) return false;
    earned[a] = true;
    return true;
}

int Stats::earned_count() const {
    int n = 0;
    for (bool e : earned) n += e;
    return n;
}

std::string stats_to_json(const Stats& stats) {
    Json root = Json::object();
    Json general = Json::object();
    for (int s = 0; s < STAT_COUNT; ++s) general[stat_info(s).key] = Json(static_cast<long long>(stats.general[s]));
    root["general"] = general;
    root["mined"] = counters_to_json(stats.mined);
    root["crafted"] = counters_to_json(stats.crafted);
    root["depleted"] = counters_to_json(stats.depleted);
    Json earned = Json::array();
    for (int a = 0; a < ACHIEVEMENT_COUNT; ++a)
        if (stats.earned[a]) earned.push(achievement_info(a).key);
    root["achievements"] = earned;
    return root.dump();
}

std::optional<Stats> stats_from_json(const std::string& text) {
    auto root = Json::parse(text);
    if (!root || !root->is_object()) return std::nullopt;
    Stats stats;
    if (const Json* general = root->get("general"))
        for (int s = 0; s < STAT_COUNT; ++s)
            stats.general[s] = static_cast<int64_t>(std::max(0.0, general->number(stat_info(s).key, 0)));
    counters_from_json(root->get("mined"), stats.mined);
    counters_from_json(root->get("crafted"), stats.crafted);
    counters_from_json(root->get("depleted"), stats.depleted);
    if (const Json* earned = root->get("achievements"); earned && earned->is_array())
        for (const Json& key : earned->items())
            for (int a = 0; a < ACHIEVEMENT_COUNT; ++a)
                if (key.is_string() && key.as_string() == achievement_info(a).key) stats.earned[a] = true;
    return stats;
}

Stats load_stats(const std::filesystem::path& file) {
    if (auto text = read_file(file))
        if (auto stats = stats_from_json(*text)) return *stats;
    return {};
}

bool save_stats(const std::filesystem::path& file, const Stats& stats) { return write_file_atomic(file, stats_to_json(stats)); }
