// PetrutzCraft - a small Minecraft-style survival game.
//
//   PetrutzCraft                      random world
//   PetrutzCraft --seed 42            same world every time
//   PetrutzCraft --render-distance 4  lower it if the game is slow (default 6, max 12)
//   PetrutzCraft --fullscreen
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include <GLFW/glfw3.h>

#include "autotest.h"
#include "game.h"
#include "gl.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // last: it defines macros such as near and far
#endif

namespace {

void usage() {
    std::printf(
        "usage: PetrutzCraft [--seed N] [--render-distance N] [--fullscreen]\n"
        "  --seed N             world seed (random if omitted)\n"
        "  --render-distance N  view distance in chunks (default 6, 2 to 12)\n"
        "  --fullscreen         fill the screen\n");
}

void fail(const char* msg) {
    std::fprintf(stderr, "PetrutzCraft: %s\n", msg);
#ifdef _WIN32
    // There is no console window on Windows, so show the error in a message box too.
    MessageBoxA(nullptr, msg, "PetrutzCraft", MB_OK | MB_ICONERROR);
#endif
}

Game* game_of(GLFWwindow* w) { return static_cast<Game*>(glfwGetWindowUserPointer(w)); }

}  // namespace

int main(int argc, char** argv) {
    std::optional<uint64_t> seed;
    int render_distance = 6;
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
        else if (a == "--render-distance")
            render_distance = std::atoi(value().c_str());
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
    render_distance = std::clamp(render_distance, 2, 12);
    if (!seed) seed = std::random_device{}() % (1u << 30);

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
        autotest = std::make_unique<AutoTest>(autotest_dir);
        std::printf("[autotest] OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    }
    glfwSwapInterval(autotest ? 0 : 1);

    int code = 0;
    try {
        Game game(window, *seed, render_distance, autotest.get());
        glfwSetWindowUserPointer(window, &game);
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int mods) { game_of(w)->on_key(key, action, mods); });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int) { game_of(w)->on_mouse_button(button, action); });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) { game_of(w)->on_cursor(x, y); });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double, double dy) { game_of(w)->on_scroll(dy); });
        glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) { game_of(w)->on_focus(focused != 0); });
        game.run();
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
