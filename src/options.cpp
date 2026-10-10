#include "options.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace {

const KeyActionInfo KEY_ACTIONS[KEY_ACTION_COUNT] = {
    {"key.forward", "Forward", GLFW_KEY_W},
    {"key.left", "Left", GLFW_KEY_A},
    {"key.back", "Back", GLFW_KEY_S},
    {"key.right", "Right", GLFW_KEY_D},
    {"key.jump", "Jump", GLFW_KEY_SPACE},
    {"key.sneak", "Sneak", GLFW_KEY_LEFT_SHIFT},
    {"key.sprint", "Sprint", GLFW_KEY_LEFT_CONTROL},
    {"key.inventory", "Inventory", GLFW_KEY_E},
    {"key.drop", "Drop", GLFW_KEY_Q},
    {"key.craft", "Craft Planks", GLFW_KEY_F},
    {"key.creative", "Creative Cheat", GLFW_KEY_C},
};

std::string format_float(float v) {
    char buf[32];
    auto res = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, res.ptr);
}

bool parse_float(const std::string& s, float& out) {
    float v;
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc() || res.ptr != s.data() + s.size()) return false;
    out = v;
    return true;
}

bool parse_int(const std::string& s, int& out) {
    int v;
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc() || res.ptr != s.data() + s.size()) return false;
    out = v;
    return true;
}

bool parse_bool(const std::string& s, bool& out) {
    if (s == "true") return out = true, true;
    if (s == "false") return out = false, true;
    return false;
}

void parse_unit(const std::string& s, float& out) {
    float v;
    if (parse_float(s, v)) out = std::clamp(v, 0.0f, 1.0f);
}

}  // namespace

const KeyActionInfo& key_action_info(int action) { return KEY_ACTIONS[action]; }

std::string key_name(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return std::string(1, static_cast<char>('A' + key - GLFW_KEY_A));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return std::string(1, static_cast<char>('0' + key - GLFW_KEY_0));
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return "NUMPAD" + std::to_string(key - GLFW_KEY_KP_0);
    switch (key) {
        case GLFW_KEY_SPACE: return "SPACE";
        case GLFW_KEY_APOSTROPHE: return "'";
        case GLFW_KEY_COMMA: return ",";
        case GLFW_KEY_MINUS: return "-";
        case GLFW_KEY_PERIOD: return ".";
        case GLFW_KEY_SLASH: return "/";
        case GLFW_KEY_SEMICOLON: return ";";
        case GLFW_KEY_EQUAL: return "=";
        case GLFW_KEY_LEFT_BRACKET: return "[";
        case GLFW_KEY_BACKSLASH: return "\\";
        case GLFW_KEY_RIGHT_BRACKET: return "]";
        case GLFW_KEY_GRAVE_ACCENT: return "`";
        case GLFW_KEY_ESCAPE: return "ESCAPE";
        case GLFW_KEY_ENTER: return "RETURN";
        case GLFW_KEY_TAB: return "TAB";
        case GLFW_KEY_BACKSPACE: return "BACK";
        case GLFW_KEY_INSERT: return "INSERT";
        case GLFW_KEY_DELETE: return "DELETE";
        case GLFW_KEY_RIGHT: return "RIGHT";
        case GLFW_KEY_LEFT: return "LEFT";
        case GLFW_KEY_DOWN: return "DOWN";
        case GLFW_KEY_UP: return "UP";
        case GLFW_KEY_PAGE_UP: return "PRIOR";
        case GLFW_KEY_PAGE_DOWN: return "NEXT";
        case GLFW_KEY_HOME: return "HOME";
        case GLFW_KEY_END: return "END";
        case GLFW_KEY_CAPS_LOCK: return "CAPITAL";
        case GLFW_KEY_LEFT_SHIFT: return "LSHIFT";
        case GLFW_KEY_RIGHT_SHIFT: return "RSHIFT";
        case GLFW_KEY_LEFT_CONTROL: return "LCONTROL";
        case GLFW_KEY_RIGHT_CONTROL: return "RCONTROL";
        case GLFW_KEY_LEFT_ALT: return "LMENU";
        case GLFW_KEY_RIGHT_ALT: return "RMENU";
        default: return "KEY " + std::to_string(key);
    }
}

int view_distance_chunks(int view) {
    static const int CHUNKS[VIEW_COUNT] = {16, 8, 4, 2};
    return CHUNKS[std::clamp(view, 0, VIEW_COUNT - 1)];
}

const char* view_distance_name(int view) {
    static const char* NAMES[VIEW_COUNT] = {"Far", "Normal", "Short", "Tiny"};
    return NAMES[std::clamp(view, 0, VIEW_COUNT - 1)];
}

const char* difficulty_name(int difficulty) {
    static const char* NAMES[DIFFICULTY_COUNT] = {"Peaceful", "Easy", "Normal", "Hard"};
    return NAMES[std::clamp(difficulty, 0, DIFFICULTY_COUNT - 1)];
}

Options::Options() {
    for (int i = 0; i < KEY_ACTION_COUNT; ++i) keys[i] = KEY_ACTIONS[i].default_key;
}

double Options::degrees_per_pixel() const {
    double f = sensitivity * 0.6 + 0.2;
    return f * f * f * 8.0 * 0.15;
}

std::string Options::to_text() const {
    std::ostringstream out;
    auto b = [](bool v) { return v ? "true" : "false"; };
    out << "music:" << format_float(music) << "\n";
    out << "sound:" << format_float(sound) << "\n";
    out << "invertYMouse:" << b(invert_mouse) << "\n";
    out << "mouseSensitivity:" << format_float(sensitivity) << "\n";
    out << "viewDistance:" << view_distance << "\n";
    out << "difficulty:" << difficulty << "\n";
    out << "fancyGraphics:" << b(fancy_graphics) << "\n";
    out << "ao:" << b(smooth_lighting) << "\n";
    out << "clouds:" << b(clouds) << "\n";
    for (int i = 0; i < KEY_ACTION_COUNT; ++i) out << "key_" << KEY_ACTIONS[i].id << ":" << keys[i] << "\n";
    return out.str();
}

Options Options::from_text(const std::string& text) {
    Options o;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon), value = line.substr(colon + 1);
        int n;
        if (key == "music") parse_unit(value, o.music);
        else if (key == "sound") parse_unit(value, o.sound);
        else if (key == "invertYMouse") parse_bool(value, o.invert_mouse);
        else if (key == "mouseSensitivity") parse_unit(value, o.sensitivity);
        else if (key == "viewDistance" && parse_int(value, n)) o.view_distance = std::clamp(n, 0, VIEW_COUNT - 1);
        else if (key == "difficulty" && parse_int(value, n)) o.difficulty = std::clamp(n, 0, DIFFICULTY_COUNT - 1);
        else if (key == "fancyGraphics") parse_bool(value, o.fancy_graphics);
        else if (key == "ao") parse_bool(value, o.smooth_lighting);
        else if (key == "clouds") parse_bool(value, o.clouds);
        else if (key.rfind("key_", 0) == 0 && parse_int(value, n) && n > 0 && n <= GLFW_KEY_LAST)
            for (int i = 0; i < KEY_ACTION_COUNT; ++i)
                if (key.substr(4) == KEY_ACTIONS[i].id) o.keys[i] = n;
    }
    return o;
}

Options Options::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Options();
    std::ostringstream ss;
    ss << f.rdbuf();
    return from_text(ss.str());
}

bool Options::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << to_text();
    return static_cast<bool>(f);
}
