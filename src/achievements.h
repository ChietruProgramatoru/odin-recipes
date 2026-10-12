// Achievements and statistics (M17), like Beta 1.7.3: the 16 achievements with their places on the
// achievement map, and the counters on the statistics screen (general ones, blocks mined, items
// crafted and tools worn out). Both belong to the player, not a world: they are kept in stats.json
// next to options.txt and carry over from world to world, as Beta's did.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "blocks.h"

enum Achievement {
    ACH_OPEN_INVENTORY,  // Taking Inventory
    ACH_MINE_WOOD,       // Getting Wood
    ACH_WORKBENCH,       // Benchmarking
    ACH_PICKAXE,         // Time to Mine!
    ACH_FURNACE,         // Hot Topic
    ACH_IRON,            // Acquire Hardware
    ACH_HOE,             // Time to Farm!
    ACH_BREAD,           // Bake Bread
    ACH_CAKE,            // The Lie
    ACH_BETTER_PICKAXE,  // Getting an Upgrade
    ACH_COOK_FISH,       // Delicious Fish
    ACH_ON_A_RAIL,       // On A Rail
    ACH_SWORD,           // Time to Strike!
    ACH_KILL_ENEMY,      // Monster Hunter
    ACH_KILL_COW,        // Cow Tipper
    ACH_FLY_PIG,         // When Pigs Fly
    ACHIEVEMENT_COUNT
};

struct AchievementInfo {
    const char* key;          // the name in stats.json (Beta's)
    const char* name;
    const char* description;
    int col, row;             // place on the achievement map (Beta's), in 24-unit steps
    ItemId icon;
    int parent;               // the achievement that has to come first, or -1
    bool special;             // drawn with a spiky frame (On A Rail, When Pigs Fly)
};
const AchievementInfo& achievement_info(int a);

// Beta's bounds of the achievement map, in map steps.
constexpr int ACH_MAP_MIN_COL = -3, ACH_MAP_MIN_ROW = -7, ACH_MAP_MAX_COL = 10, ACH_MAP_MAX_ROW = 7;

// What earns an achievement: taking a crafted item, taking a smelted one out of a furnace, or
// picking an item up off the ground.
std::optional<Achievement> achievement_for_craft(ItemId item);
std::optional<Achievement> achievement_for_smelt(ItemId item);
std::optional<Achievement> achievement_for_pickup(ItemId item);

constexpr double ON_A_RAIL_DISTANCE = 1000;  // blocks from where the minecart ride began (Beta)
constexpr double FLY_PIG_FALL = 5;           // blocks the ridden pig has to fall (Beta: more than 5)

// The general statistics, in Beta's order. Distances are in centimetres and the play time in game
// ticks, like Beta kept them.
enum Stat {
    STAT_GAMES_STARTED,  // Times played: the game was started
    STAT_WORLDS_CREATED,
    STAT_WORLDS_LOADED,  // Saves loaded
    STAT_PLAY_TIME,
    STAT_WALK,
    STAT_SWIM,
    STAT_FALL,
    STAT_CLIMB,
    STAT_FLY,
    STAT_DIVE,
    STAT_MINECART,
    STAT_BOAT,
    STAT_PIG,
    STAT_JUMPS,
    STAT_DROPS,
    STAT_DAMAGE_DEALT,
    STAT_DAMAGE_TAKEN,
    STAT_DEATHS,
    STAT_MOB_KILLS,
    STAT_FISH,
    STAT_COUNT
};
enum class StatUnit { Count, Time, Distance };
struct StatInfo {
    const char* key;
    const char* name;
    StatUnit unit;
};
const StatInfo& stat_info(int s);
// A value the way the statistics screen shows it: "12", "3.5 h", "1.24 km", "37.5 m"...
std::string format_stat(StatUnit unit, int64_t value);

struct Stats {
    std::array<int64_t, STAT_COUNT> general{};
    std::map<BlockId, int64_t> mined;    // blocks broken, by block
    std::map<ItemId, int64_t> crafted;   // items made at a crafting grid or furnace, by item
    std::map<ItemId, int64_t> depleted;  // tools and armor worn out
    std::array<bool, ACHIEVEMENT_COUNT> earned{};

    void add(Stat s, int64_t amount = 1) { general[s] += amount; }
    // Its parent is earned (or it has none), so it can be earned now.
    bool can_earn(int a) const;
    // Earn an achievement. False if it was already earned or its parent isn't yet (Beta ignores it then).
    bool earn(Achievement a);
    int earned_count() const;
    bool operator==(const Stats&) const = default;
};

std::string stats_to_json(const Stats& stats);
// Unknown keys are skipped; nothing if the text isn't a JSON object.
std::optional<Stats> stats_from_json(const std::string& text);
// A missing or damaged file gives empty statistics.
Stats load_stats(const std::filesystem::path& file);
bool save_stats(const std::filesystem::path& file, const Stats& stats);
