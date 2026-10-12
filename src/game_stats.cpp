// Achievements and statistics (M17): what earns each achievement, and the counting of distances,
// crafting and kills for the statistics screen.
#include <algorithm>
#include <cmath>

#include "game.h"

void Game::earn(Achievement a) {
    if (creative) return;  // the creative cheat earns nothing
    if (stats_->earn(a)) hud->show_achievement(a);
}

void Game::picked_up(const Stack& stack) {
    if (auto a = achievement_for_pickup(stack.item)) earn(*a);
}

void Game::collect_crafting() {
    if (!gui || !gui->crafting) return;
    CraftingMenu& menu = *gui->crafting;
    for (const Stack& s : menu.crafted) {
        stats_->crafted[s.item] += s.count;
        if (auto a = achievement_for_craft(s.item)) earn(*a);
    }
    for (const Stack& s : menu.smelted) {
        stats_->crafted[s.item] += s.count;  // Beta counts smelted items as crafted
        if (auto a = achievement_for_smelt(s.item)) earn(*a);
    }
    menu.crafted.clear();
    menu.smelted.clear();
}

void Game::killed(bool monster) {
    count(STAT_MOB_KILLS);
    if (monster) earn(ACH_KILL_ENEMY);
}

void Game::track_movement() {
    const glm::dvec3 now(player.x, player.y, player.z);
    const std::optional<glm::dvec3> last = last_pos_;
    last_pos_ = now;

    // On A Rail: 1 km in a straight line from where the minecart ride began (Beta).
    if (in_vehicle && in_vehicle->is_cart()) {
        if (!ride_start_) ride_start_ = now;
        if (glm::length(now - *ride_start_) >= ON_A_RAIL_DISTANCE) earn(ACH_ON_A_RAIL);
    } else {
        ride_start_.reset();
    }
    // When Pigs Fly: the pig the player rides falls more than 5 blocks.
    if (riding) {
        const Body& b = riding->body;
        if (!b.on_ground && !b.in_liquid()) {
            pig_peak_ = std::max(pig_peak_, b.y);
        } else {
            if (pig_peak_ - b.y > FLY_PIG_FALL) earn(ACH_FLY_PIG);
            pig_peak_ = b.y;
        }
    } else {
        pig_peak_ = player.y;
    }

    if (!last || vitals.dead()) return;
    const glm::dvec3 d = now - *last;
    const double horizontal = std::hypot(d.x, d.z), all = glm::length(d);
    if (all > 20) return;  // a teleport, a respawn or a trip through a portal: not travelling
    auto cm = [](double blocks) { return static_cast<int64_t>(std::lround(blocks * 100)); };
    // Like Beta's addMovementStat: one kind of travel per step.
    if (in_vehicle) {
        count(in_vehicle->is_cart() ? STAT_MINECART : STAT_BOAT, cm(all));
    } else if (riding) {
        count(STAT_PIG, cm(all));
    } else if (head_under_water()) {
        count(STAT_DIVE, cm(all));
    } else if (player.in_water) {
        count(STAT_SWIM, cm(horizontal));
    } else if (on_ladder_ && d.y > 0) {
        count(STAT_CLIMB, cm(d.y));
    } else if (player.on_ground) {
        count(STAT_WALK, cm(horizontal));
    } else {
        count(STAT_FLY, cm(horizontal));  // in the air: jumping, falling or flying
    }
}
