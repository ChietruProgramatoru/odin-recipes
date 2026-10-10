"""Cows and dropped items: behaviour plus the box geometry used to draw them."""
import math
import random

from .blocks import (APPLE, BEEF, BLOCKS, T_BLACK, T_COW_HEAD, T_COW_HIDE, T_HOOF, T_HORN, T_SNOUT, icon_tile,
                     is_block)
from .physics import Body
from .textures import tile_uv
from .world import _FACE_DEFS

COW_HALF_WIDTH = 0.45
COW_HEIGHT = 1.3
COW_MAX_HEALTH = 10


# --- tiny 3x4 affine matrices ---------------------------------------------------

def translate(x, y, z):
    return ((1, 0, 0, x), (0, 1, 0, y), (0, 0, 1, z))


def rot_y(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return ((c, 0, s, 0), (0, 1, 0, 0), (-s, 0, c, 0))     # local +z turns towards (sin, 0, cos)


def rot_x(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return ((1, 0, 0, 0), (0, c, -s, 0), (0, s, c, 0))     # positive tips +z downwards


def rot_z(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return ((c, -s, 0, 0), (s, c, 0, 0), (0, 0, 1, 0))


def mul(a, b):
    (a00, a01, a02, a03), (a10, a11, a12, a13), (a20, a21, a22, a23) = a
    (b00, b01, b02, b03), (b10, b11, b12, b13), (b20, b21, b22, b23) = b
    return ((a00 * b00 + a01 * b10 + a02 * b20, a00 * b01 + a01 * b11 + a02 * b21,
             a00 * b02 + a01 * b12 + a02 * b22, a00 * b03 + a01 * b13 + a02 * b23 + a03),
            (a10 * b00 + a11 * b10 + a12 * b20, a10 * b01 + a11 * b11 + a12 * b21,
             a10 * b02 + a11 * b12 + a12 * b22, a10 * b03 + a11 * b13 + a12 * b23 + a13),
            (a20 * b00 + a21 * b10 + a22 * b20, a20 * b01 + a21 * b11 + a22 * b21,
             a20 * b02 + a21 * b12 + a22 * b22, a20 * b03 + a21 * b13 + a22 * b23 + a23))


def apply(m, p):
    x, y, z = p
    return (m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
            m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
            m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3])


def add_box(mesh, m, size, center, tile, brightness=1.0):
    """Add a textured box of `size` centred at `center` (in the frame of matrix m)."""
    sx, sy, sz = size
    cx, cy, cz = center
    uv = tile_uv(tile)
    for _normal, corners, light, _slot in _FACE_DEFS:
        pts = [apply(m, (cx + (c[0] - 0.5) * sx, cy + (c[1] - 0.5) * sy, cz + (c[2] - 0.5) * sz)) for c in corners]
        mesh.quad_points(pts, uv, light * brightness)


def _angle_diff(a, b):
    return (b - a + 180) % 360 - 180


class Cow:
    LEGS = ((-0.22, 0.4), (0.22, 0.4), (-0.22, -0.4), (0.22, -0.4))

    def __init__(self, world, x, y, z):
        self.body = Body(world, x, y, z, COW_HALF_WIDTH, COW_HEIGHT)
        self.yaw = random.uniform(0, 360)
        self.target_yaw = self.yaw
        self.health = COW_MAX_HEALTH
        self.state = 'idle'
        self.state_timer = random.uniform(1, 4)
        self.speed = 0.0
        self.walk_phase = 0.0
        self.knock_x = self.knock_z = 0.0
        self.flash = 0.0
        self.dying = 0.0
        self.blocked = 0
        self.graze = False
        self.head_pitch = 0.0
        self.roll = 0.0

    @property
    def alive(self):
        return self.health > 0

    def hurt(self, amount, from_x, from_z):
        if not self.alive:
            return
        self.health -= amount
        dx, dz = self.body.x - from_x, self.body.z - from_z
        d = math.hypot(dx, dz) or 1.0
        self.knock_x, self.knock_z = dx / d * 7, dz / d * 7
        if self.body.on_ground:
            self.body.vy = 5.5
        self.flash = 0.3
        self.state = 'panic'
        self.state_timer = 5.0
        self.target_yaw = math.degrees(math.atan2(dx, dz))
        if not self.alive:
            self.dying = 0.0001

    def ray_hit(self, origin, direction, max_dist):
        """Distance along a ray to this cow's hit box, or None."""
        b = self.body
        h = 0.6
        lo = (b.x - h, b.y, b.z - h)
        hi = (b.x + h, b.y + 1.5, b.z + h)
        t0, t1 = 0.0, max_dist
        for o, d, l, u in zip(origin, direction, lo, hi):
            if abs(d) < 1e-9:
                if o < l or o > u:
                    return None
                continue
            a, c = (l - o) / d, (u - o) / d
            if a > c:
                a, c = c, a
            t0, t1 = max(t0, a), min(t1, c)
            if t0 > t1:
                return None
        return t0

    def tick(self, dt, player_x, player_z):
        """Returns 'dead' when the cow has finished dying and should drop loot."""
        if self.dying:
            self.dying += dt
            self.roll = min(90.0, self.dying * 200)
            return 'dead' if self.dying > 0.7 else None

        self.state_timer -= dt
        if self.state_timer <= 0:
            if self.state in ('panic', 'walk'):
                self.state, self.state_timer = 'idle', random.uniform(2, 6)
                self.graze = random.random() < 0.5
            else:
                self.state, self.state_timer = 'walk', random.uniform(2, 5)
                self.target_yaw = random.uniform(0, 360)
        if self.state == 'panic' and random.random() < dt * 1.2:
            away = math.degrees(math.atan2(self.body.x - player_x, self.body.z - player_z))
            self.target_yaw = away + random.uniform(-60, 60)

        turn = 360 if self.state == 'panic' else 120
        diff = _angle_diff(self.yaw, self.target_yaw)
        self.yaw += max(-turn * dt, min(turn * dt, diff))

        want = {'idle': 0.0, 'walk': 1.3, 'panic': 4.2}[self.state]
        if abs(diff) > 60:
            want *= 0.3
        self.speed += (want - self.speed) * min(1.0, dt * 6)

        rad = math.radians(self.yaw)
        b = self.body
        b.vx = math.sin(rad) * self.speed + self.knock_x
        b.vz = math.cos(rad) * self.speed + self.knock_z
        decay = max(0.0, 1 - dt * 6)
        self.knock_x *= decay
        self.knock_z *= decay

        b.check_water()
        b.apply_gravity(dt)
        if b.in_water:
            b.vy = max(b.vy, 2.0)
        hx, _, hz = b.move(b.vx * dt, b.vy * dt, b.vz * dt)
        if (hx or hz) and self.speed > 0.3:
            if b.on_ground and self.blocked < 3:
                b.vy = 8.2     # hop up one block
                self.blocked += 1
            elif self.blocked >= 3:
                self.target_yaw = self.yaw + random.choice((-1, 1)) * random.uniform(90, 180)
                self.blocked = 0
        elif b.on_ground:
            self.blocked = 0

        self.walk_phase += self.speed * dt * 5
        target_pitch = 40 if (self.state == 'idle' and self.graze) else 0
        self.head_pitch += (target_pitch - self.head_pitch) * min(1.0, dt * 4)
        self.flash = max(0.0, self.flash - dt)
        return None

    def parts(self):
        """(model name, 3x4 matrix) pairs to draw this cow; see build_models()."""
        b = self.body
        root = mul(mul(translate(b.x, b.y, b.z), rot_y(self.yaw)), rot_z(self.roll))
        suffix = '_hurt' if (self.flash > 0 or self.dying) else ''
        out = [('cow_body' + suffix, root)]
        swing = math.sin(self.walk_phase) * min(1.0, self.speed) * 35
        for i, (lx, lz) in enumerate(self.LEGS):
            out.append(('cow_leg' + suffix,
                        mul(mul(root, translate(lx, 0.75, lz)), rot_x(swing if i in (0, 3) else -swing))))
        out.append(('cow_head' + suffix, mul(mul(root, translate(0, 1.2, 0.58)), rot_x(self.head_pitch))))
        return out


class ItemDrop:
    def __init__(self, world, item, x, y, z):
        self.item = item
        self.body = Body(world, x, y, z, 0.12, 0.25)
        self.body.vx = random.uniform(-1.5, 1.5)
        self.body.vz = random.uniform(-1.5, 1.5)
        self.body.vy = random.uniform(3, 5)
        self.age = 0.0

    def tick(self, dt):
        b = self.body
        self.age += dt
        b.check_water()
        b.apply_gravity(dt)
        if b.in_water:
            b.vy = max(b.vy, 1.0)
        b.move(b.vx * dt, b.vy * dt, b.vz * dt)
        if b.on_ground:
            b.vx *= max(0.0, 1 - dt * 8)
            b.vz *= max(0.0, 1 - dt * 8)

    def parts(self):
        b = self.body
        bob = 0.2 + math.sin(self.age * 3) * 0.06
        return [(f'item_{self.item}', mul(translate(b.x, b.y + bob, b.z), rot_y(self.age * 90)))]


def build_models(make_mesh):
    """Static meshes for cow body parts and dropped items, each in its own local frame."""
    models = {}
    for suffix, hurt in (('', False), ('_hurt', True)):
        hide = T_SNOUT if hurt else T_COW_HIDE
        m = make_mesh()
        ident = translate(0, 0, 0)
        add_box(m, ident, (0.75, 0.62, 1.15), (0, 1.06, 0), hide)                     # body
        add_box(m, ident, (0.08, 0.5, 0.08), (0, 0.95, -0.6), hide)                   # tail
        models['cow_body' + suffix] = m
        m = make_mesh()
        add_box(m, ident, (0.25, 0.63, 0.25), (0, -0.315, 0), hide)                   # leg
        add_box(m, ident, (0.26, 0.12, 0.26), (0, -0.69, 0), T_SNOUT if hurt else T_HOOF)
        models['cow_leg' + suffix] = m
        m = make_mesh()
        add_box(m, ident, (0.5, 0.5, 0.38), (0, 0, 0.19), T_SNOUT if hurt else T_COW_HEAD)
        add_box(m, ident, (0.32, 0.2, 0.08), (0, -0.13, 0.41), T_SNOUT)
        for side in (-1, 1):
            add_box(m, ident, (0.07, 0.07, 0.02), (side * 0.15, 0.08, 0.385), T_BLACK)  # eyes
            add_box(m, ident, (0.08, 0.14, 0.08), (side * 0.29, 0.25, 0.12), T_HORN)    # horns
        models['cow_head' + suffix] = m
    for item in list(BLOCKS) + [APPLE, BEEF]:
        m = make_mesh()
        size = (0.25, 0.25, 0.25) if is_block(item) else (0.4, 0.4, 0.03)
        add_box(m, translate(0, 0, 0), size, (0, 0, 0), icon_tile(item))
        models[f'item_{item}'] = m
    return models


def drop_beef_count():
    return random.randint(1, 3)
