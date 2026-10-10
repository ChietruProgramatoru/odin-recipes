// The player's own hand in first person: the arm, or the selected item held in it, at the bottom
// right of the screen, swinging when the player hits, mines or uses something (Beta 1.7.3).
#pragma once
#include <map>
#include <optional>
#include <vector>

#include "image.h"
#include "items.h"
#include "mobs.h"

// The model of an item held in the hand is HELD_MODEL_BASE + item id.
constexpr int HELD_MODEL_BASE = 5000;

class Hand {
public:
    static constexpr double SWING_SECONDS = 0.4;  // Beta: 8 ticks

    // Start a swing. One already under way starts over only once it is half done, so holding the
    // mouse on a block keeps the arm going without it stuttering at the top.
    void swing();
    void update(double dt);
    bool swinging() const { return swing_time_ >= 0; }
    // 0 at rest, rising to 1 at the end of a swing.
    float swing_progress() const;

    // The arm (nothing held) or the held item, in camera space: x to the right, y up, looking
    // down -z. Light levels are left for the caller to fill in.
    std::vector<ModelPart> parts(std::optional<ItemId> held) const;

private:
    double swing_time_ = -1;  // seconds into the swing, or -1 when the hand is still
};

// A mesh for every item, for holding it: blocks are small cubes (slabs and stairs keep their
// shape) with the faces of their inventory icon, everything else is its icon given one pixel of
// thickness, like Beta. Built from the item table, so new items get a model without any changes here.
// `tiles` are the atlas tiles (make_tiles()), for the icons' see-through pixels.
std::map<int, MeshData> build_held_models(const std::map<int, Image>& tiles);
