# PyCraft

A small Minecraft-style survival game for the desktop, written in Python with
[pyglet](https://pyglet.org/) (OpenGL). It runs on Python 3.10 through 3.15.

![Spawn](screenshots/spawn.png)

## Features

- **Endless terrain** generated from Perlin noise: grassy hills, sandy beaches, lakes, rare stone peaks and oak trees. Chunks stream in around you as you walk.
- **Building**: hold the left mouse button to mine a block. The block drops as an item that you pick up by walking over it. Right click places the selected block.
- **Health and hunger**: you have 10 hearts and 10 drumsticks. Walking, sprinting, jumping, mining and fighting use up hunger. When you are well fed you slowly heal. When you are starving you slowly lose health, down to half a heart. Falls of more than 3 blocks hurt, and a big enough fall kills you.
- **Cows** wander, graze and hop up ledges. Punch one and it panics and runs away. Each cow drops 1 to 3 beef, which you eat with a right click to restore 6 hunger.
- Apples sometimes drop from leaves.
- Crafting: turn 1 log into 4 planks.
- **Creative mode** gives you unlimited blocks, instant mining and flying.
- All textures are drawn by the code itself (no image files), with smooth ambient-occlusion lighting and distance fog.

## Install and run

You need **Python 3.10 or newer**, including 3.15 (from [python.org](https://www.python.org/downloads/)), and a graphics card with OpenGL 3.3, which almost every computer from the last ten years has. Tested with Python 3.13 and 3.15 and pyglet 2.1.

The only libraries it needs are pyglet and Pillow, and both install on Python 3.15.

```bash
cd minecraft-clone
python -m venv .venv
# Windows:      .venv\Scripts\activate
# macOS/Linux:  source .venv/bin/activate
pip install -r requirements.txt
python main.py
```

On macOS and Linux you may need `python3` instead of `python`.

Options:

```bash
python main.py --seed 42              # same world every time
python main.py --render-distance 4    # lower it if the game is slow (default 6, max 12)
python main.py --fullscreen
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
| C | Switch between survival and creative |
| Space twice (creative) | Toggle flying. While flying, Space goes up and Shift goes down |
| H | Show or hide the help text |
| Esc | Pause and free the mouse. Click to resume |
| R | Respawn after dying |

## Survival tips

- Punch a tree (about 1.5 s per log), press **F** to make planks, then build a shelter.
- Cows are found on grass. Five spawn near you at the start, and more appear as you explore.
- Keep at least 9 drumsticks of hunger to regenerate health.

## Project layout

```
main.py              entry point and command-line options
pycraft/
  world.py           chunk storage, terrain generation, meshing (with ambient occlusion), ray casting
  render.py          OpenGL shaders, chunk streaming, drawing cows and items
  game.py            the game window: controls, mining/building, survival rules, cows and item drops
  physics.py         box-vs-voxel collision used by the player, cows and items
  survival.py        inventory, health, hunger and saturation
  mobs.py            cow AI and model, dropped items
  hud.py             crosshair, hotbar, hearts, hunger and overlays
  textures.py        procedurally generated pixel-art textures
  blocks.py          block and item definitions
  noise.py           Perlin noise
  autotest.py        scripted smoke test used during development
tests/test_logic.py  logic tests that need no window: `python tests/test_logic.py`
```

## Known limits

- The world is not saved when you quit.
- There are no tools, furnaces, day/night cycle, caves or hostile mobs yet.
- It has been tested on Linux. Windows and macOS should behave the same, since pyglet supports all three.
