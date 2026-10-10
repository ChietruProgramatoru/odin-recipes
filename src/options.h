// Player settings, kept in options.txt next to the game in Beta's "name:value" format.
#pragma once
#include <array>
#include <filesystem>
#include <string>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>  // key codes only

// Rebindable controls, in the order the Controls screen lists them.
enum KeyAction : int {
    KEY_FORWARD, KEY_LEFT, KEY_BACK, KEY_RIGHT, KEY_JUMP, KEY_SNEAK,
    KEY_INVENTORY, KEY_DROP, KEY_CRAFT, KEY_CREATIVE,
    KEY_ACTION_COUNT
};

struct KeyActionInfo {
    const char* id;      // name in options.txt (after "key_")
    const char* label;   // shown on the Controls screen
    int default_key;     // GLFW key code
};
const KeyActionInfo& key_action_info(int action);

// "W", "SPACE", "LSHIFT", "F3", ... for a GLFW key code.
std::string key_name(int key);

// Beta's four view distances, in chunks.
enum ViewDistance : int { VIEW_FAR, VIEW_NORMAL, VIEW_SHORT, VIEW_TINY, VIEW_COUNT };
int view_distance_chunks(int view);  // 16, 8, 4, 2
const char* view_distance_name(int view);

struct Options {
    bool invert_mouse = false;
    float sensitivity = 0.5f;  // 0..1, shown as 0..200%
    int view_distance = VIEW_NORMAL;
    bool fancy_graphics = true;
    bool smooth_lighting = true;
    bool clouds = true;  // Fancy graphics draws them as 3D boxes, Fast as a flat layer
    std::array<int, KEY_ACTION_COUNT> keys;

    Options();
    // Beta's curve: 0.5 gives 0.15 degrees of turn per pixel of mouse movement.
    double degrees_per_pixel() const;

    std::string to_text() const;
    // Unknown lines and bad values are ignored, so an old or hand-edited file still loads.
    static Options from_text(const std::string& text);
    static Options load(const std::filesystem::path& path);  // defaults if the file is missing
    bool save(const std::filesystem::path& path) const;
};
