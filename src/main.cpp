// PetrutzCraft - a small Minecraft-style survival game.
//
//   PetrutzCraft                      starts at the title screen
//   PetrutzCraft --seed 42            fills in the seed when you create a new world
//   PetrutzCraft --fullscreen
//
// options.txt, saves/ and screenshots/ live in the folder the program is in.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include <GLFW/glfw3.h>

#include "app.h"
#include "autotest.h"
#include "gl.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // last: it defines macros such as near and far
#endif

namespace {

void usage() {
    std::printf(
        "usage: PetrutzCraft [--seed N] [--fullscreen]\n"
        "  --seed N             seed to fill in when creating a new world\n"
        "  --fullscreen         fill the screen\n"
        "  --autotest OUT_DIR   run the scripted smoke test (uses --seed, default 7)\n");
}

void fail(const char* msg) {
    std::fprintf(stderr, "PetrutzCraft: %s\n", msg);
#ifdef _WIN32
    // There is no console window on Windows, so show the error in a message box too.
    MessageBoxA(nullptr, msg, "PetrutzCraft", MB_OK | MB_ICONERROR);
#endif
}

App* app_of(GLFWwindow* w) { return static_cast<App*>(glfwGetWindowUserPointer(w)); }

// The folder the program is in: options and saves are kept next to the game.
std::filesystem::path program_dir() {
#ifdef _WIN32
    std::wstring buf(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size()) return std::filesystem::path(buf.substr(0, n)).parent_path();
#else
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#endif
    return std::filesystem::current_path();
}

}  // namespace

int main(int argc, char** argv) {
    std::optional<uint64_t> seed;
    bool fullscreen = false;
    std::string autotest_dir;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--seed")
            seed = std::strtoull(value().c_str(), nullptr, 10);
        else if (a == "--fullscreen")
            fullscreen = true;
        else if (a == "--autotest")
            autotest_dir = value();
        else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }

    if (!glfwInit()) {
        fail("could not start GLFW (no display?)");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    GLFWmonitor* monitor = nullptr;
    int width = 1280, height = 720;
    if (fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
    }
    GLFWwindow* window = glfwCreateWindow(width, height, "PetrutzCraft", monitor, nullptr);
    if (!window) {
        fail("could not open a window with OpenGL 3.3 (update your graphics driver)");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!load_gl()) {
        fail("this graphics driver is missing OpenGL 3.3 functions");
        glfwTerminate();
        return 1;
    }
    std::unique_ptr<AutoTest> autotest;
    if (!autotest_dir.empty()) {
        autotest = std::make_unique<AutoTest>(autotest_dir, seed.value_or(7));
        std::printf("[autotest] OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    }
    glfwSwapInterval(autotest ? 0 : 1);

    int code = 0;
    try {
        AppConfig config;
        config.data_dir = autotest ? std::filesystem::path(autotest_dir) : program_dir();
        config.seed = seed;
        config.autotest = autotest.get();
        App app(window, config);
        glfwSetWindowUserPointer(window, &app);
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int mods) { app_of(w)->on_key(key, action, mods); });
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int c) { app_of(w)->on_char(c); });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int) { app_of(w)->on_mouse_button(button, action); });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) { app_of(w)->on_cursor(x, y); });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double, double dy) { app_of(w)->on_scroll(dy); });
        glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) { app_of(w)->on_focus(focused != 0); });
        app.run();
        glfwSetWindowUserPointer(window, nullptr);
        if (autotest && autotest->failures()) code = 1;
    } catch (const std::exception& e) {
        fail(e.what());
        code = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return code;
}
