cmake_minimum_required(VERSION 3.20)
project(PetrutzCraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Libraries, downloaded at configure time -------------------------------
# Plain release downloads (no git needed), each checked against its SHA-256.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)   # X11 only on Linux (Wayland desktops use XWayland)
FetchContent_Declare(glfw
  URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
  URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9)

# GLM is headers only. Unpack it into a folder called glm so includes read <glm/...>.
FetchContent_Declare(glm
  URL https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
  URL_HASH SHA256=9a995de4da09723bd33ef194e6b79818950e5a8f2e154792f02e4615277cfb8d
  SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/glm-src/glm
  SOURCE_SUBDIR no-cmake)

FetchContent_MakeAvailable(glfw glm)
add_library(glm_headers INTERFACE)
target_include_directories(glm_headers SYSTEM INTERFACE ${FETCHCONTENT_BASE_DIR}/glm-src)

# doctest is a single header.
set(DOCTEST_HEADER ${FETCHCONTENT_BASE_DIR}/doctest/doctest/doctest.h)
if(NOT EXISTS ${DOCTEST_HEADER})
  file(DOWNLOAD https://github.com/doctest/doctest/releases/download/v2.4.11/doctest.h ${DOCTEST_HEADER}
    EXPECTED_HASH SHA256=44faa038e9c3f9728efbda143748d01124ea0a27f4bf78f35a15d8fab2e039fb
    SHOW_PROGRESS)
endif()
add_library(doctest_header INTERFACE)
target_include_directories(doctest_header SYSTEM INTERFACE ${FETCHCONTENT_BASE_DIR}/doctest)

# miniaudio (public domain) plays the sound effects. It is a single header too.
set(MINIAUDIO_HEADER ${FETCHCONTENT_BASE_DIR}/miniaudio/miniaudio.h)
if(NOT EXISTS ${MINIAUDIO_HEADER})
  file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h ${MINIAUDIO_HEADER}
    EXPECTED_HASH SHA256=6b2029714f8634c4d7c70cc042f45074e0565766113fc064f20cd27c986be9c9
    SHOW_PROGRESS)
endif()
add_library(miniaudio_header INTERFACE)
target_include_directories(miniaudio_header SYSTEM INTERFACE ${FETCHCONTENT_BASE_DIR}/miniaudio)

find_package(Threads REQUIRED)

if(MSVC)
  add_compile_options(/W3 /utf-8 /permissive- /MP)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# --- Game logic: no window or OpenGL, shared by the game and the tests ------
add_library(petrutz_core STATIC
  src/blocks.cpp
  src/items.cpp
  src/icons.cpp
  src/crafting.cpp
  src/furnace.cpp
  src/container.cpp
  src/noise.cpp
  src/world.cpp
  src/worldgen.cpp
  src/biome.cpp
  src/light.cpp
  src/sky.cpp
  src/weather.cpp
  src/bed.cpp
  src/building.cpp
  src/chest.cpp
  src/painting.cpp
  src/blockupdate.cpp
  src/rails.cpp
  src/vehicles.cpp
  src/nature.cpp
  src/streamer.cpp
  src/physics.cpp
  src/survival.cpp
  src/farming.cpp
  src/tools.cpp
  src/mobs.cpp
  src/animals.cpp
  src/hand.cpp
  src/combat.cpp
  src/monsters.cpp
  src/sound.cpp
  src/textures.cpp
  src/building_art.cpp
  src/font.cpp
  src/png.cpp
  src/zlib.cpp
  src/json.cpp
  src/save.cpp
  src/options.cpp)
target_include_directories(petrutz_core PUBLIC src)
# GLFW's header only, for its key codes (options.txt stores them). The game links GLFW itself.
target_include_directories(petrutz_core SYSTEM PUBLIC ${glfw_SOURCE_DIR}/include)
target_link_libraries(petrutz_core PUBLIC glm_headers Threads::Threads)

# --- The game ----------------------------------------------------------------
add_executable(PetrutzCraft
  src/main.cpp
  src/app.cpp
  src/gl.cpp
  src/renderer.cpp
  src/ui.cpp
  src/inventory_screen.cpp
  src/hud.cpp
  src/menu.cpp
  src/game.cpp
  src/game_vehicles.cpp
  src/audio.cpp
  src/autotest.cpp)
target_link_libraries(PetrutzCraft PRIVATE petrutz_core glfw miniaudio_header ${CMAKE_DL_LIBS})
target_compile_definitions(PetrutzCraft PRIVATE GLFW_INCLUDE_NONE)
if(WIN32)
  # A normal windowed program: no console window when you double-click it.
  set_target_properties(PetrutzCraft PROPERTIES WIN32_EXECUTABLE TRUE)
  if(MSVC)
    target_link_options(PetrutzCraft PRIVATE /ENTRY:mainCRTStartup)
  else()
    target_link_options(PetrutzCraft PRIVATE -mwindows)
  endif()
endif()

# --- Logic tests (doctest) ---------------------------------------------------
enable_testing()
add_executable(petrutz_tests
  tests/test_main.cpp
  tests/test_world.cpp
  tests/test_worldgen.cpp
  tests/test_survival.cpp
  tests/test_farming.cpp
  tests/test_crafting.cpp
  tests/test_streamer.cpp
  tests/test_misc.cpp
  tests/test_save.cpp
  tests/test_light.cpp
  tests/test_tools.cpp
  tests/test_weather.cpp
  tests/test_monsters.cpp
  tests/test_animals.cpp
  tests/test_furnace.cpp
  tests/test_hand.cpp
  tests/test_blockupdate.cpp
  tests/test_building.cpp
  tests/test_transport.cpp
  tests/test_nature.cpp)
target_link_libraries(petrutz_tests PRIVATE petrutz_core doctest_header)
add_test(NAME logic COMMAND petrutz_tests)
