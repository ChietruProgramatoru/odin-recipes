"""Game-logic tests that run without opening a window.

Run with:  python -m pytest tests      (or simply: python tests/test_logic.py)
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from pycraft.blocks import AIR, BEEF, COBBLE, DIRT, LOG, PLANKS, STONE  # noqa: E402
from pycraft.physics import Body  # noqa: E402
from pycraft.survival import Inventory, Vitals, fall_damage  # noqa: E402
from pycraft.world import HEIGHT, LAYER, World  # noqa: E402


def test_padded_chunk_matches_world():
    w = World(3)
    c = w.chunk(0, 0)
    P = w._padded(c)
    assert len(P) == LAYER * (HEIGHT + 2)
    for y in range(c.max_y + 1):
        for z in range(-1, 17):
            for x in range(-1, 17):
                assert P[(y + 1) * LAYER + (z + 1) * 18 + (x + 1)] == w.get_block(x, y, z)


def test_terrain_and_mesh():
    w = World(5)
    solid, _ = w.build_mesh_data(w.chunk(0, 0))
    assert len(solid.positions) == solid.count * 3 and len(solid.uvs) == solid.count * 2
    assert len(solid.indices) % 6 == 0 and max(solid.indices) < solid.count
    assert w.surface_y(0, 0) > 1


def test_raycast_and_edit():
    w = World(5)
    y = w.surface_y(4, 4)
    hit = w.raycast((4.5, y + 1.5, 4.5), (0, -1, 0), 5)
    assert hit and hit[0] == (4, y - 1, 4) and hit[1] == (4, y, 4)
    w.set_block(4, y - 1, 4, AIR)
    assert w.get_block(4, y - 1, 4) == AIR


def test_body_lands_on_ground():
    w = World(5)
    y = w.surface_y(8, 8)
    b = Body(w, 8.5, y + 5, 8.5, 0.3, 1.8)
    for _ in range(200):
        b.apply_gravity(1 / 60)
        b.move(0, b.vy / 60, 0)
    assert b.on_ground and abs(b.y - y) < 1e-6


def test_inventory():
    inv = Inventory()
    assert inv.add(DIRT, 70) == 0
    assert inv.slots[0] == [DIRT, 64] and inv.slots[1] == [DIRT, 6]
    inv.add(LOG, 1)
    assert inv.craft_planks() and inv.count(PLANKS) == 4 and inv.count(LOG) == 0
    inv.selected = 0
    assert inv.take_selected() == DIRT and inv.count(DIRT) == 69
    assert inv.add(STONE, 64 * 9) > 0   # overflow is reported
    assert inv.count(COBBLE) == 0


def test_hunger_and_health():
    v = Vitals()
    v.saturation = 0
    v.exhaust(8.0)
    assert v.hunger == 18
    v.health = 10
    for _ in range(int(4.0 / 0.05) + 1):
        v.tick(0.05)
    assert v.health == 11            # regenerates when well fed
    v.hunger = 0
    v.health = 2
    for _ in range(400):
        v.tick(0.05)
    assert v.health == 1             # starving hurts but never kills
    v.hunger = 10
    assert v.eat(BEEF) and v.hunger == 16
    assert fall_damage(3.0) == 0 and fall_damage(10.0) == 7
    assert v.damage(20, ignore_cooldown=True) and v.dead


if __name__ == '__main__':
    for name, fn in list(globals().items()):
        if name.startswith('test_'):
            fn()
            print('ok', name)
