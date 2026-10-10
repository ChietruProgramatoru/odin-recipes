"""Scripted smoke test: `python main.py --autotest OUT_DIR --seed 1`.

Drives the real game for a few hundred frames, exercises mining, building, cows, eating and
fall damage, saves screenshots to OUT_DIR and writes OUT_DIR/report.txt. Used for development.
"""
import math
import os
import random

import pyglet

from .blocks import AIR, BEEF


class AutoTest:
    def __init__(self, out_dir):
        self.out = out_dir
        os.makedirs(out_dir, exist_ok=True)
        self.lines = []
        self.state = {}
        random.seed(0)

    def log(self, msg):
        print('[autotest]', msg, flush=True)
        self.lines.append(msg)

    def check(self, name, ok):
        self.log(('PASS ' if ok else 'FAIL ') + name)

    def shot(self, g, name):
        g.switch_to()
        g.on_draw()
        pyglet.image.get_buffer_manager().get_color_buffer().save(os.path.join(self.out, name))
        self.log(f'screenshot {name}')

    def _face(self, g, x, z, pitch):
        p = g.player
        g.yaw = math.degrees(math.atan2(x - p.x, z - p.z))
        g.pitch = pitch

    def __call__(self, g):
        f = g.frame
        p = g.player
        s = self.state
        if f == 2:
            g.hud.show_help = False
        if f == 120:
            self.log(f'seed {g.seed}, chunks meshed {len(g.renderer.meshes)}, cows {len(g.cows)}')
            self.shot(g, '1_spawn.png')
            cow = min(g.cows, key=lambda c: math.hypot(c.body.x - p.x, c.body.z - p.z))
            s['cow'] = cow
            cow.state, cow.state_timer, cow.graze = 'idle', 1000, False
            # Walk the player up to the cow and look at it.
            d = math.hypot(cow.body.x - p.x, cow.body.z - p.z)
            p.x += (cow.body.x - p.x) * (d - 3) / d
            p.z += (cow.body.z - p.z) * (d - 3) / d
            p.y = float(g.world.surface_y(math.floor(p.x), math.floor(p.z)))
            s['cow_start'] = (cow.body.x, cow.body.z)
        if 120 < f < 200 and 'cow' in s:
            cow = s['cow']
            d = math.hypot(cow.body.x - p.x, cow.body.z - p.z)
            self._face(g, cow.body.x, cow.body.z,
                       math.degrees(math.atan2(p.y + 1.62 - (cow.body.y + 0.8), d)))
        if f == 160:
            self.shot(g, '2_cow.png')
            self.check('cow is targetable by the crosshair', g.target_cow is s['cow'])
        if f == 200:
            cow = s['cow']
            moved = math.hypot(cow.body.x - s['cow_start'][0], cow.body.z - s['cow_start'][1])
            self.log(f'cow wandered {moved:.2f} blocks; on ground={cow.body.on_ground}')
            for _ in range(5):
                cow.hurt(2, p.x, p.z)
            self.check('cow dies after 5 punches', not cow.alive)
        if f == 260:
            self.check('dead cow removed', s['cow'] not in g.cows)
            beef = [d for d in g.drops if d.item == BEEF]
            self.check('cow dropped beef', len(beef) > 0 or g.inventory.count(BEEF) > 0)
            for d in beef:
                d.body.x, d.body.y, d.body.z = p.x, p.y + 0.5, p.z
        if f == 290:
            self.check('beef picked up', g.inventory.count(BEEF) > 0)
            g.vitals.hunger = 8
            g.inventory.selected = next(i for i, sl in enumerate(g.inventory.slots) if sl and sl[0] == BEEF)
            g._use_item()
            self.check('eating beef restores hunger (8 -> %d)' % g.vitals.hunger, g.vitals.hunger > 8)
            g.pitch = 70
        if f == 300:
            t = g.target
            self.check('looking at a block', t is not None)
            if t:
                s['broken'] = t[0]
                s['broken_block'] = g.world.get_block(*t[0])
                g._break_block(t[0], s['broken_block'])
                self.check('block removed from world', g.world.get_block(*t[0]) == AIR)
        if f == 340 and 'broken' in s:
            from .blocks import BLOCKS
            drop = BLOCKS[s['broken_block']].drop
            self.check('broken block collected into inventory', drop is None or g.inventory.count(drop) > 0)
            if drop is not None and g.inventory.count(drop):
                g.inventory.selected = next(i for i, sl in enumerate(g.inventory.slots) if sl and sl[0] == drop)
                g._update_target()
                place = g.target[1] if g.target else None
                g._use_item()
                self.check('block placed back', place is not None and g.world.get_block(*place) == drop)
            g.pitch = 10
        if f == 360:
            s['hp'] = g.vitals.health
            p.y += 12
            p.vy = 0
            g.fall_peak = p.y
        if f == 460:
            self.check('fall damage (health %d -> %d)' % (s['hp'], g.vitals.health), g.vitals.health < s['hp'])
            self.shot(g, '3_after_fall.png')
            g.creative = True
            g.flying = True
            p.y += 25
            g.pitch = 35
        if f == 520:
            self.shot(g, '4_overview.png')
            with open(os.path.join(self.out, 'report.txt'), 'w') as fh:
                fh.write('\n'.join(self.lines) + '\n')
            g.close()
            pyglet.app.exit()
