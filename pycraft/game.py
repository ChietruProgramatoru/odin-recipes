"""The game window: player input, mining/building, survival, cows and drawing."""
import math
import random
import time as pytime

import pyglet
from pyglet.window import key, mouse

from .blocks import (AIR, APPLE, BEEF, BEDROCK, BLOCKS, CREATIVE_ITEMS, FOODS, GRASS, LEAVES, WATER,
                     is_block)
from .hud import Hud
from .mobs import Cow, ItemDrop, drop_beef_count
from .physics import Body
from .render import WorldRenderer, pixel_texture
from .survival import (EXHAUST_ATTACK, EXHAUST_BREAK, EXHAUST_JUMP, EXHAUST_SPRINT, EXHAUST_SPRINT_JUMP,
                       EXHAUST_WALK, Inventory, Vitals, fall_damage)
from .textures import make_atlas, make_hud_icons, make_item_icons, make_tiles
from .world import CHUNK, SEA_LEVEL, World

SKY = (0.62, 0.8, 1.0, 1.0)
EYE_HEIGHT = 1.62
REACH = 5.0
ATTACK_REACH = 3.5
WALK_SPEED = 4.3
SPRINT_SPEED = 5.8
FLY_SPEED = 11.0
JUMP_SPEED = 8.4
MOUSE_SENSITIVITY = 0.15     # degrees per pixel
MAX_COWS = 24


class Game(pyglet.window.Window):
    def __init__(self, seed=None, render_distance=6, autotest=None, **window_args):
        config = pyglet.gl.Config(double_buffer=True, depth_size=24, major_version=3, minor_version=3,
                                  forward_compatible=True)
        super().__init__(caption='PyCraft', resizable=True, config=config, **window_args)
        self.keys = key.KeyStateHandler()
        self.push_handlers(self.keys)
        self.mouse_buttons = set()

        self.seed = seed if seed is not None else random.randrange(1 << 30)
        self.world = World(self.seed)
        tiles = make_tiles()
        self.renderer = WorldRenderer(self.world, pixel_texture(make_atlas(tiles)), render_distance, SKY)
        self.renderer.on_chunk_shown = self._maybe_spawn_cows
        self.hud = Hud(self, make_item_icons(tiles), make_hud_icons())

        self.inventory = Inventory()
        self.creative_inventory = Inventory()
        for item in CREATIVE_ITEMS:
            self.creative_inventory.add(item, 1)
        self.vitals = Vitals()
        self.creative = False
        self.flying = False

        self.spawn = self._find_spawn()
        self.player = Body(self.world, *self.spawn, 0.3, 1.8)
        self.yaw, self.pitch = 0.0, 0.0          # pitch > 0 looks down
        self.fall_peak = self.player.y
        self.was_on_ground = True

        self.cows, self.drops = [], []
        self.mining_pos, self.mining_progress = None, 0.0
        self.attack_cooldown = self.break_cooldown = self.use_cooldown = 0.0
        self.target = None          # (block_pos, place_pos, dist) or None
        self.target_cow = None
        self.paused = False
        self._last_space = -1.0

        # Build the area around spawn before the first frame; the rest streams in.
        self.renderer.load_all(self.player.x, self.player.z, radius=2)
        for _ in range(5):
            self._spawn_cow_near(self.player.x, self.player.z, 6, 20)

        self.set_exclusive_mouse(True)
        self.autotest = autotest
        self.frame = 0
        self.hud.show_message('Welcome! Punch trees, find cows, survive.', 5)
        pyglet.clock.schedule_interval(self.update, 1 / 60)

    # --- world setup ----------------------------------------------------------
    def _find_spawn(self):
        for r in range(0, 200, 4):
            for x, z in ((r, 0), (0, r), (-r, 0), (0, -r), (r, r), (-r, -r)):
                y = self.world.surface_y(x, z)
                if y > SEA_LEVEL + 1 and self.world.get_block(x, y - 1, z) == GRASS:
                    return x + 0.5, float(y), z + 0.5
        return 0.5, float(self.world.surface_y(0, 0)), 0.5

    def _spawn_cow_near(self, x, z, rmin, rmax):
        for _ in range(10):
            a = random.uniform(0, math.tau)
            r = random.uniform(rmin, rmax)
            cx, cz = math.floor(x + math.cos(a) * r), math.floor(z + math.sin(a) * r)
            y = self.world.surface_y(cx, cz)
            if self.world.get_block(cx, y - 1, cz) == GRASS and not self.world.is_solid(cx, y + 1, cz):
                self.cows.append(Cow(self.world, cx + 0.5, y, cz + 0.5))
                return True
        return False

    def _maybe_spawn_cows(self, chunk):
        if chunk.mob_spawned:
            return
        chunk.mob_spawned = True
        if len(self.cows) >= MAX_COWS or random.random() > 0.12:
            return
        cx, cz = chunk.cx * CHUNK + 8, chunk.cz * CHUNK + 8
        for _ in range(random.randint(1, 3)):
            self._spawn_cow_near(cx, cz, 0, 6)

    # --- camera -----------------------------------------------------------------
    def eye(self):
        p = self.player
        return (p.x, p.y + EYE_HEIGHT, p.z)

    def forward(self):
        y, p = math.radians(self.yaw), math.radians(self.pitch)
        return (math.sin(y) * math.cos(p), -math.sin(p), math.cos(y) * math.cos(p))

    # --- input events -------------------------------------------------------------
    def on_key_press(self, symbol, modifiers):
        if self.vitals.dead:
            if symbol == key.R:
                self.respawn()
            return
        if symbol == key.ESCAPE:
            self.set_paused(not self.paused)
            return pyglet.event.EVENT_HANDLED      # don't close the window
        if self.paused:
            return
        if key._1 <= symbol <= key._9:
            self.active_inventory.selected = symbol - key._1
            self.hud.show_item_name(self.active_inventory.selected_item)
        elif symbol == key.C:
            self.creative = not self.creative
            self.flying = False
            self.hud.show_message('Creative mode' if self.creative else 'Survival mode')
        elif symbol == key.F and not self.creative:
            ok = self.inventory.craft_planks()
            self.hud.show_message('Crafted 4 planks' if ok else 'Need 1 oak log (and a free slot) to craft planks', 2)
        elif symbol == key.H:
            self.hud.show_help = not self.hud.show_help
        elif symbol == key.SPACE and self.creative:
            now = pytime.perf_counter()
            if now - self._last_space < 0.3:
                self.flying = not self.flying
                self.player.vy = 0
            self._last_space = now

    def on_mouse_motion(self, x, y, dx, dy):
        if self.paused or self.vitals.dead:
            return
        self.yaw -= dx * MOUSE_SENSITIVITY
        self.pitch = max(-89.9, min(89.9, self.pitch - dy * MOUSE_SENSITIVITY))

    def on_mouse_drag(self, x, y, dx, dy, buttons, modifiers):
        self.on_mouse_motion(x, y, dx, dy)

    def on_mouse_press(self, x, y, button, modifiers):
        if self.vitals.dead:
            return
        if self.paused:
            self.set_paused(False)
            return
        self.mouse_buttons.add(button)
        if button == mouse.LEFT:
            self.break_cooldown = 0.0
            self._update_target()
            self._try_attack()
        elif button == mouse.RIGHT:
            self._update_target()
            self._use_item()

    def on_mouse_release(self, x, y, button, modifiers):
        self.mouse_buttons.discard(button)

    def on_mouse_scroll(self, x, y, scroll_x, scroll_y):
        if self.paused or self.vitals.dead or not scroll_y:
            return
        inv = self.active_inventory
        inv.selected = (inv.selected + (-1 if scroll_y > 0 else 1)) % 9
        self.hud.show_item_name(inv.selected_item)

    def on_deactivate(self):
        if not self.vitals.dead and not self.autotest:
            self.set_paused(True)

    def on_resize(self, width, height):
        super().on_resize(width, height)
        if hasattr(self, 'hud'):
            self.hud.resize()

    def set_paused(self, paused):
        self.paused = paused
        self.mouse_buttons.clear()
        self.set_exclusive_mouse(not paused)

    @property
    def active_inventory(self):
        return self.creative_inventory if self.creative else self.inventory

    # --- per-frame ----------------------------------------------------------------
    def update(self, dt):
        dt = min(dt, 0.05)
        self.frame += 1
        p = self.player
        dead = self.vitals.dead

        if not self.paused and not dead:
            self._move(dt)
            self._update_target()
            self._mine(dt)
            if mouse.RIGHT in self.mouse_buttons and self.use_cooldown <= 0:
                self._use_item()
            self.use_cooldown -= dt
            self.break_cooldown -= dt
            self.attack_cooldown -= dt
            if not self.creative and self.vitals.tick(dt):
                self.hud.flash_damage()
        else:
            self.target = None
            self.hud.set_mining(None)

        if not self.paused:
            self._update_cows(dt)
            self._update_drops(dt)
        self.renderer.update(p.x, p.z)

        view = self.renderer.render_distance * CHUNK
        self.renderer.set_instances([part for thing in self.cows + self.drops
                                     if abs(thing.body.x - p.x) < view and abs(thing.body.z - p.z) < view
                                     for part in thing.parts()])
        self.renderer.set_outline(self.target[0] if self.target and not self.target_cow else None)

        ex, ey, ez = self.eye()
        eye_block = self.world.get_block(math.floor(ex), math.floor(ey), math.floor(ez))
        mode = 'Creative' + (' (flying)' if self.flying else '') if self.creative else 'Survival'
        info = f'{mode}   XYZ {p.x:.1f} {p.y:.1f} {p.z:.1f}   cows {len(self.cows)}   FPS {pyglet.clock.get_frequency():.0f}'
        self.hud.update(dt, self.active_inventory, self.vitals, self.creative, info,
                        eye_block == WATER, dead, self.paused)

        if self.autotest:
            self.autotest(self)

    def on_draw(self):
        pyglet.gl.glClearColor(*SKY)
        self.clear()
        self.renderer.draw(self.width, self.height, self.eye(), self.forward())
        self.hud.draw()

    def _move(self, dt):
        p = self.player
        held = self.keys
        yr = math.radians(self.yaw)
        fwd = (math.sin(yr), math.cos(yr))
        right = (-math.cos(yr), math.sin(yr))
        f = held[key.W] - held[key.S]
        s = held[key.D] - held[key.A]
        wx, wz = fwd[0] * f + right[0] * s, fwd[1] * f + right[1] * s
        length = math.hypot(wx, wz)
        if length > 0:
            wx, wz = wx / length, wz / length
        p.check_water()

        shift = held[key.LSHIFT] or held[key.RSHIFT]
        sprint = shift and held[key.W] and not self.flying and (self.creative or self.vitals.can_sprint)
        speed = FLY_SPEED if self.flying else (SPRINT_SPEED if sprint else WALK_SPEED)
        if p.in_water and not self.flying:
            speed *= 0.55
        accel = 14 if (p.on_ground or self.flying) else 5
        k = min(1.0, accel * dt)
        p.vx += (wx * speed - p.vx) * k
        p.vz += (wz * speed - p.vz) * k

        jumped = False
        if self.flying:
            p.vy = (held[key.SPACE] - shift) * 8.0
        else:
            p.apply_gravity(dt)
            if held[key.SPACE]:
                if p.in_water:
                    p.vy = min(p.vy + 30 * dt, 4.0)
                elif p.on_ground:
                    p.vy = JUMP_SPEED
                    jumped = True

        ox, oz = p.x, p.z
        p.move(p.vx * dt, p.vy * dt, p.vz * dt)
        if self.flying and p.on_ground and shift:
            self.flying = False

        if not self.creative:
            moved = math.hypot(p.x - ox, p.z - oz)
            self.vitals.exhaust(moved * (EXHAUST_SPRINT if sprint else EXHAUST_WALK))
            if jumped:
                self.vitals.exhaust(EXHAUST_SPRINT_JUMP if sprint else EXHAUST_JUMP)

        # Fall damage
        if p.in_water or self.flying or self.creative:
            self.fall_peak = p.y
        elif not p.on_ground:
            self.fall_peak = max(self.fall_peak, p.y)
        else:
            if not self.was_on_ground:
                dmg = fall_damage(self.fall_peak - p.y)
                if dmg and self.vitals.damage(dmg, ignore_cooldown=True):
                    self.hud.flash_damage()
                    if self.vitals.dead:
                        self._on_death()
            self.fall_peak = p.y
        self.was_on_ground = p.on_ground

    def _update_target(self):
        origin, direction = self.eye(), self.forward()
        self.target = self.world.raycast(origin, direction, REACH)
        best, best_d = None, ATTACK_REACH
        for cow in self.cows:
            if cow.alive:
                d = cow.ray_hit(origin, direction, ATTACK_REACH)
                if d is not None and d < best_d:
                    best, best_d = cow, d
        if best and self.target and self.target[2] < best_d:
            best = None
        self.target_cow = best

    # --- actions ---------------------------------------------------------------------
    def _try_attack(self):
        cow = self.target_cow
        if not cow or self.attack_cooldown > 0:
            return
        self.attack_cooldown = 0.35
        cow.hurt(10 if self.creative else 2, self.player.x, self.player.z)
        if not self.creative:
            self.vitals.exhaust(EXHAUST_ATTACK)

    def _mine(self, dt):
        if mouse.LEFT not in self.mouse_buttons or not self.target or self.target_cow:
            self.mining_pos, self.mining_progress = None, 0.0
            self.hud.set_mining(None)
            return
        pos = self.target[0]
        blk = self.world.get_block(*pos)
        info = BLOCKS.get(blk)
        if info is None or (info.hardness is None and not self.creative) or blk == BEDROCK:
            self.hud.set_mining(None)
            return
        if pos != self.mining_pos:
            self.mining_pos, self.mining_progress = pos, 0.0
        if self.creative:
            self.hud.set_mining(None)
            if self.break_cooldown <= 0:
                self._break_block(pos, blk)
                self.break_cooldown = 0.25
            return
        self.mining_progress += dt / max(0.05, info.hardness)
        self.hud.set_mining(self.mining_progress)
        if self.mining_progress >= 1.0:
            self._break_block(pos, blk)
            self.mining_pos, self.mining_progress = None, 0.0

    def _break_block(self, pos, blk):
        self.renderer.mark_dirty(self.world.set_block(*pos, AIR))
        self.renderer.flush_dirty()
        if self.creative:
            return
        self.vitals.exhaust(EXHAUST_BREAK)
        drop = BLOCKS[blk].drop
        if blk == LEAVES and random.random() < 0.1:
            drop = APPLE
        if drop is not None:
            self._spawn_drop(drop, pos[0] + 0.5, pos[1] + 0.3, pos[2] + 0.5)

    def _spawn_drop(self, item, x, y, z):
        self.drops.append(ItemDrop(self.world, item, x, y, z))

    def _use_item(self):
        self.use_cooldown = 0.25
        inv = self.active_inventory
        item = inv.selected_item
        if item is None:
            return
        if item in FOODS:
            if not self.creative and self.vitals.eat(item):
                inv.take_selected()
                self.hud.show_message('Yum!', 1)
            return
        if not is_block(item) or not self.target or self.target_cow:
            return
        place = self.target[1]
        if place is None:
            return
        x, y, z = place
        bodies = [self.player] + [cow.body for cow in self.cows]
        for b in bodies:
            if (x + 1 > b.x - b.half and x < b.x + b.half and z + 1 > b.z - b.half and z < b.z + b.half
                    and y + 1 > b.y and y < b.y + b.height):
                return
        if self.world.get_block(x, y, z) not in (AIR, WATER):
            return
        self.renderer.mark_dirty(self.world.set_block(x, y, z, item))
        self.renderer.flush_dirty()
        if not self.creative:
            inv.take_selected()

    def _on_death(self):
        self.hud.set_mining(None)
        self.mouse_buttons.clear()
        self.set_exclusive_mouse(False)

    def respawn(self):
        self.vitals.reset()
        p = self.player
        p.x, p.y, p.z = self.spawn
        p.vx = p.vy = p.vz = 0
        self.fall_peak = p.y
        self.paused = False
        self.set_exclusive_mouse(True)
        self.hud.show_message('Respawned')

    # --- mobs and drops ------------------------------------------------------------
    def _update_cows(self, dt):
        p = self.player
        far = (self.renderer.render_distance + 1) * CHUNK
        for cow in list(self.cows):
            # Skip cows on chunks that aren't generated yet so they don't fall through the world.
            if (math.floor(cow.body.x) >> 4, math.floor(cow.body.z) >> 4) not in self.world.chunks:
                continue
            if cow.tick(dt, p.x, p.z) == 'dead':
                for _ in range(drop_beef_count()):
                    self._spawn_drop(BEEF, cow.body.x, cow.body.y + 0.5, cow.body.z)
                self.cows.remove(cow)
            elif math.hypot(cow.body.x - p.x, cow.body.z - p.z) > far:
                self.cows.remove(cow)
        if len(self.cows) < 6 and random.random() < dt * 0.05:
            self._spawn_cow_near(p.x, p.z, 24, 48)

    def _update_drops(self, dt):
        p = self.player
        for drop in list(self.drops):
            drop.tick(dt)
            b = drop.body
            dx, dz = b.x - p.x, b.z - p.z
            horiz = math.hypot(dx, dz)
            vert_ok = p.y - 1.2 < b.y < p.y + 2.2
            if drop.age > 0.5 and not self.vitals.dead and vert_ok and horiz < 2.5:
                if horiz < 1.2 and self.inventory.add(drop.item, 1) == 0:
                    self.drops.remove(drop)
                    continue
                # Pull nearby items towards the player.
                pull = 6.0 * dt / max(horiz, 0.3)
                b.x -= dx * min(1.0, pull)
                b.z -= dz * min(1.0, pull)
            if drop.age > 300:
                self.drops.remove(drop)
