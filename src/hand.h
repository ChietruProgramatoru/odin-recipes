// The player's own hand in first person: the arm, or the selected item held in it, at the bottom
// right of the screen, swinging when the player hits, mines or uses something (Beta 1.7.3).
#pragma once
#include <map>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>

#include "image.h"
#include "items.h"
#include "mobs.h"

// The model of an item held in the hand is HELD_MODEL_BASE + item id.
constexpr int HELD_MODEL_BASE = 5000;

// How the player moves, for the view bob: speeds in blocks per second.
struct HandMotion {
    double horizontal = 0, vertical = 0;
    bool on_ground = true;  // false while flying or dead too
};

class Hand {
public:
    static constexpr double SWING_SECONDS = 0.4;  // Beta: 8 ticks
    static constexpr double EQUIP_RATE = 8.0;     // Beta: 0.4 of the way down or up per tick

    // View bobbing (the Options switch): the camera and the hand sway while walking.
    bool bobbing = true;

    // Start a swing. One already under way starts over only once it is half done, so holding the
    // mouse on a block keeps the arm going without it stuttering at the top.
    void swing();
    // `held` and `slot` are the selected hotbar item and slot. When either changes the hand dips out
    // of view, takes the new item and comes back up (Beta).
    void update(double dt, std::optional<ItemId> held, int slot, HandMotion motion = {});
    bool swinging() const { return swing_time_ >= 0; }
    // 0 at rest, rising to 1 at the end of a swing.
    float swing_progress() const;
    // 1 when the hand is fully up, 0 when it is out of view during a switch.
    float equipped() const { return static_cast<float>(equipped_); }
    // What the hand holds right now (it lags behind the hotbar while switching).
    std::optional<ItemId> shown() const { return shown_; }

    // Beta's view bob, to apply to the camera and the hand (identity when bobbing is off).
    glm::mat4 bob() const;
    // The arm (nothing held) or the held item, in camera space: x to the right, y up, looking
    // down -z, bob included. Light levels are left for the caller to fill in.
    std::vector<ModelPart> parts() const;

private:
    void tick(double dt, std::optional<ItemId> held, int slot, HandMotion motion);

    double swing_time_ = -1;  // seconds into the swing, or -1 when the hand is still
    double equipped_ = 0;
    std::optional<ItemId> shown_;
    int shown_slot_ = -1;
    double walked_ = 0;       // Beta's distanceWalkedModified: 0.6 per block walked
    double bob_amount_ = 0;   // Beta's cameraYaw: up to 0.1 at walking speed
    double bob_pitch_ = 0;    // Beta's cameraPitch: degrees the view tips while falling or jumping
};

// A mesh for every item, for holding it: blocks are small cubes (slabs and stairs keep their
// shape) with the faces of their inventory icon, everything else is its icon given one pixel of
// thickness, like Beta. Built from the item table, so new items get a model without any changes here.
// `tiles` are the atlas tiles (make_tiles()), for the icons' see-through pixels.
std::map<int, MeshData> build_held_models(const std::map<int, Image>& tiles);
