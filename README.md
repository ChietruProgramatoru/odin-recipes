# PetrutzCraft

A small Minecraft-style survival game for the desktop, written in C++20 with OpenGL 3.3.
It runs on Windows (the main target) and Linux.

![Spawn](screenshots/spawn.png)

## Features

- **Endless terrain** generated from Perlin noise: grassy hills, sandy beaches, lakes, rare stone peaks and oak trees. Chunks are generated and meshed on background threads as you walk, so the game doesn't stutter.
- **Building**: hold the left mouse button to mine a block. The block drops as an item that you pick up by walking over it. Right click places the selected block.
- **Health and hunger**: you have 10 hearts and 10 drumsticks. Walking, sprinting, jumping, mining and fighting use up hunger. When you are well fed you slowly heal. When you are starving you slowly lose health, down to half a heart. Falls of more than 3 blocks hurt, and a big enough fall kills you.
- **Cows** wander, graze and hop up ledges. Punch one and it panics and runs away. Each cow drops 1 to 3 beef, which you eat with a right click to restore 6 hunger.
- Apples sometimes drop from leaves.
- Crafting: turn 1 log into 4 planks.
- **Creative mode** (a cheat key) gives you unlimited blocks, instant mining and flying.
- All textures and the font are drawn by the code itself (no image files), with smooth ambient-occlusion lighting and distance fog.

## Build and run on Windows

You need a graphics card with OpenGL 3.3, which almost every computer from the last ten years has.

1. Install **Visual Studio 2022 Build Tools** from <https://visualstudio.microsoft.com/downloads/> (under "Tools for Visual Studio"). In the installer, tick the **Desktop development with C++** workload. It includes the compiler, the Windows SDK and CMake.
2. Install **VS Code** and, from its Extensions panel, the **C/C++** and **CMake Tools** extensions (both by Microsoft).
3. Open this folder in VS Code (File > Open Folder).
4. When CMake Tools asks for a kit, pick **Visual Studio Build Tools 2022 Release - amd64**. (Later you can change it with the command palette: *CMake: Select a Kit*.)
5. In the status bar, set the build variant to **Release**, then click **Build** (or press F7). The first build downloads GLFW, GLM and doctest, which takes a minute.
6. Click the ▷ button in the status bar (or *CMake: Run Without Debugging*, Shift+F5) to play. The program is `build/PetrutzCraft.exe` (or `build/Release/PetrutzCraft.exe`, depending on the generator).

From a terminal instead, open **Developer PowerShell for VS 2022** in this folder and run:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\PetrutzCraft.exe
```

Every push also builds `PetrutzCraft.exe` on GitHub: open the latest run under the **Actions** tab and download the **PetrutzCraft-windows-x64** artifact.

## Build and run on Linux

```bash
sudo apt install build-essential cmake ninja-build libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PetrutzCraft
```

## Options

```bash
PetrutzCraft --seed 42              # same world every time
PetrutzCraft --render-distance 4    # lower it if the game is slow (default 6, 2 to 12)
PetrutzCraft --fullscreen
```

## Controls

| Key | Action |
| --- | --- |
| Mouse | Look around |
| W A S D | Move |
| Space | Jump, or swim up in water |
| Left Shift | Sprint (not possible when hunger is 3 drumsticks or less) |
| Left mouse (hold) | Mine a block, or hit a cow |
| Right mouse | Place the selected block, or eat the selected food |
| 1–9 / mouse wheel | Choose a hotbar slot |
| F | Craft 1 oak log into 4 planks |
| C | Cheat: switch between survival and creative |
| Space twice (creative) | Toggle flying. While flying, Space goes up and Shift goes down |
| H | Show or hide the help text |
| Esc | Pause and free the mouse. Click to resume |
| R | Respawn after dying |

## Survival tips

- Punch a tree (about 1.5 s per log), press **F** to make planks, then build a shelter.
- Cows are found on grass. Five spawn near you at the start, and more appear as you explore.
- Keep at least 9 drumsticks of hunger to regenerate health.

## Tests

```bash
ctest --test-dir build --output-on-failure        # logic tests (add -C Release on Windows)
xvfb-run -a -s "-screen 0 1280x720x24" ./build/PetrutzCraft --seed 7 --autotest OUT_DIR
```

The second command plays a scripted game (find a cow, kill it, pick up and eat the beef, mine and place a block, fall, fly) and saves screenshots plus `report.txt` to `OUT_DIR`. It exits with an error if any check fails. On Windows or a Linux desktop, run it without `xvfb-run`. GitHub runs both on every push (the screenshot test on Linux only, since GitHub's Windows machines have no graphics card).

## Project layout

```
CMakeLists.txt     build script; downloads GLFW, GLM and doctest
src/
  main.cpp         command-line options and the window
  game.cpp         controls, mining/building, survival rules, cows and item drops
  world.cpp        chunk storage, terrain generation, meshing (with ambient occlusion), ray casting
  streamer.cpp     background threads that generate and mesh chunks
  renderer.cpp     OpenGL shaders and drawing chunks, cows, items and the block outline
  hud.cpp          crosshair, hotbar, hearts, hunger, text and overlays
  physics.cpp      box-vs-voxel collision used by the player, cows and items
  survival.cpp     inventory, health, hunger and saturation
  mobs.cpp         cow AI and model, dropped items
  textures.cpp     procedurally generated pixel-art textures
  font.cpp         the 5x7 pixel font
  blocks.cpp       block and item definitions
  noise.cpp        Perlin noise
  png.cpp          PNG writer for autotest screenshots
  gl.cpp           loads the OpenGL functions the game uses
  autotest.cpp     the scripted smoke test
tests/             doctest logic tests (no window needed)
```

## Known limits

- The world is not saved when you quit.
- There are no tools, furnaces, day/night cycle, caves or hostile mobs yet.
- The world is 64 blocks tall with the sea at height 20.
- The same seed makes a different world than the Python version did, because the random numbers come from a different generator. The terrain rules are the same.
