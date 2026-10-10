# PetrutzCraft

A small Minecraft-style survival game for the desktop, written in C++20 with OpenGL 3.3.
It runs on Windows (the main target) and Linux.

![Spawn](screenshots/spawn.png)

![Title screen](screenshots/title.png)

## Features

- **Title screen and worlds**: Beta-style gray buttons on a dark dirt background, all drawn by the code. Singleplayer lists your worlds (newest first) and lets you create, rename and delete them. A new world takes a name and an optional seed: a number is used as is, any other text is turned into a number, and a blank seed picks a random one.
- **Saving**: worlds are saved every 60 seconds, when you open the pause menu and when you quit. Blocks you changed, where you stand, health, hunger, your hotbar, cows and dropped items all come back when you load the world.
- **Endless terrain** generated from Perlin noise: grassy hills, sandy beaches, lakes, rare stone peaks and oak trees. The world is 128 blocks tall with the sea at height 64. Chunks are generated (or loaded from the save) and meshed on background threads as you walk, so the game doesn't stutter.
- **Building**: hold the left mouse button to mine a block. The block drops as an item that you pick up by walking over it. Right click places the selected block.
- **Health and hunger**: you have 10 hearts and 10 drumsticks. Walking, sprinting, jumping, mining and fighting use up hunger. When you are well fed you slowly heal. When you are starving you slowly lose health, down to half a heart. Falls of more than 3 blocks hurt, and a big enough fall kills you.
- **Cows** wander, graze and hop up ledges. Punch one and it panics and runs away. Each cow drops 1 to 3 beef, which you eat with a right click to restore 6 hunger.
- Apples sometimes drop from leaves.
- Crafting: turn 1 log into 4 planks.
- **Creative mode** (a cheat key) gives you unlimited blocks, instant mining and flying.
- **Sneaking** (Shift) makes you slower and stops you from walking off edges. **Sprinting** is on Left Ctrl.
- **Third-person view** (F5) shows your character: an original blocky figure with a red shirt and jeans.

  ![Third person](screenshots/third_person.png)
- **Debug screen** (F3) with your position, chunk, facing, sky light, seed and frame rate. F1 hides the HUD and F2 saves a screenshot.
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

## Where your worlds and settings are kept

Everything lives in the folder the program is in (for example `build\Release\`):

| Path | What it is |
| --- | --- |
| `saves/<world>/level.json` | The world's name, seed, play time, spawn point and the player (position, health, hunger, hotbar) |
| `saves/<world>/region/r.X.Z.pcr` | The world's chunks, 32 x 32 chunks per file, each one compressed. Cows and dropped items are stored with the chunk they are in |
| `options.txt` | Your settings, one `name:value` per line like Beta's |
| `screenshots/` | Pictures taken with F2 |

Deleting the `build` folder deletes these too, so copy `saves/` somewhere safe first.

## Options

The **Options** button on the title screen (or in the pause menu) has:

- **Music** and **Sound** volume, **Invert Mouse**, mouse **Sensitivity** and **Difficulty** (Peaceful, Easy, Normal, Hard). The game has no sound, monsters or clouds yet, so music, sound, difficulty and clouds are saved but don't change anything until those arrive.
- **Video Settings**: **Graphics** Fancy (see-through leaves) or Fast (solid leaves), **Render Distance** Far (16 chunks), Normal (8, the default), Short (4) or Tiny (2), **Smooth Lighting** on or off, and **Clouds**.
- **Controls**: click a key, then press the new key for it. A key used twice turns red.

Command line:

```bash
PetrutzCraft --seed 42      # fills in the seed when you create a new world
PetrutzCraft --fullscreen
```

## Controls

These are the defaults. Every key except the F keys, Esc, H, R and 1–9 can be changed under Options > Controls.

| Key | Action |
| --- | --- |
| Mouse | Look around |
| W A S D | Move |
| Space | Jump, or swim up in water |
| Left Shift | Sneak: walk slowly without falling off edges |
| Left Ctrl (with W) | Sprint (not possible when hunger is 3 drumsticks or less) |
| Left mouse (hold) | Mine a block, or hit a cow |
| Right mouse | Place the selected block, or eat the selected food |
| 1–9 / mouse wheel | Choose a hotbar slot |
| Q | Drop one of the selected item |
| E | Inventory (the inventory screen comes with crafting in M2) |
| F | Craft 1 oak log into 4 planks (moves into the crafting grid in M2) |
| C | Cheat: switch between survival and creative |
| Space twice (creative) | Toggle flying. While flying, Space goes up and Shift goes down |
| F1 | Hide or show the HUD |
| F2 | Save a screenshot to `screenshots/` |
| F3 | Debug screen |
| F5 | Third-person view |
| H | Show or hide the help text |
| Esc | Pause menu (Back to Game, Options, Save and Quit to Title). The world is saved when it opens |
| R | Respawn after dying |

## Survival tips

- Click **Singleplayer**, then **Create New World**.
- Punch a tree (about 1.5 s per log), press **F** to make planks, then build a shelter.
- Cows are found on grass. Five spawn near you at the start, and more appear as you explore.
- Keep at least 9 drumsticks of hunger to regenerate health.

## Tests

```bash
ctest --test-dir build --output-on-failure        # logic tests (add -C Release on Windows)
xvfb-run -a -s "-screen 0 1280x720x24" ./build/PetrutzCraft --seed 7 --autotest OUT_DIR
```

The second command plays a scripted game and saves screenshots plus `report.txt` to `OUT_DIR`: it opens the title and options screens, creates a world from the Create New World screen, finds a cow, kills it, picks up and eats the beef, mines and places a block, falls, flies, takes F5 and F3 pictures, opens the pause menu, changes a block, saves and quits to the title, and loads the world again to check that everything came back. Its world and `options.txt` go in `OUT_DIR`, so your own saves are never touched. It exits with an error if any check fails. On Windows or a Linux desktop, run it without `xvfb-run`. GitHub runs both on every push (the screenshot test on Linux only, since GitHub's Windows machines have no graphics card).

## Project layout

```
CMakeLists.txt     build script; downloads GLFW, GLM and doctest
src/
  main.cpp         command-line options and the window
  app.cpp          the title screen, menus and options around the game, loading and quitting worlds
  menu.cpp         every menu screen (title, world list, create/rename/delete, options, controls, pause)
  ui.cpp           2D drawing: rectangles, icons and pixel text for the HUD and menus
  game.cpp         controls, mining/building, survival rules, cows, item drops and saving one world
  save.cpp         save files: level.json, region files, world folders, background saving
  options.cpp      options.txt and the key bindings
  json.cpp         a small JSON reader and writer
  zlib.cpp         zlib compression and decompression (for saves and PNG screenshots)
  world.cpp        chunk storage, terrain generation, meshing (with ambient occlusion), ray casting
  streamer.cpp     background threads that generate and mesh chunks
  renderer.cpp     OpenGL shaders and drawing chunks, cows, items and the block outline
  hud.cpp          crosshair, hotbar, hearts, hunger, text and overlays
  physics.cpp      box-vs-voxel collision used by the player, cows and items
  survival.cpp     inventory, health, hunger and saturation
  mobs.cpp         cow AI and model, the player model, dropped items
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

- There are no tools, furnaces, day/night cycle, caves or hostile mobs yet.
- No sound, music, clouds or monsters yet, so those options do nothing for now. The F3 screen shows only whether the sky is open (real light levels come in M5).
- Pressing E shows a message: the inventory screen comes with crafting in M2.
- The menus and text use only plain ASCII letters, numbers and symbols.
- Worlds saved by this version can't be read by the Python version (or by Minecraft).
- The same seed makes a different world than the Python version did, because the random numbers come from a different generator. The terrain rules are the same.
