"""Axis-aligned box physics against the voxel grid (used by the player, cows and item drops)."""
import math

GRAVITY = 28.0
TERMINAL_VELOCITY = 50.0
_E = 1e-7


class Body:
    def __init__(self, world, x, y, z, half_width, height):
        self.world = world
        self.x, self.y, self.z = x, y, z      # feet centre
        self.vx = self.vy = self.vz = 0.0
        self.half = half_width
        self.height = height
        self.on_ground = False
        self.in_water = False

    def collides(self):
        h = self.half
        return self.world.box_collides(self.x - h, self.y, self.z - h,
                                       self.x + h - _E, self.y + self.height - _E, self.z + h - _E)

    def check_water(self):
        h = self.half
        self.in_water = self.world.box_in_water(self.x - h, self.y + 0.1, self.z - h,
                                                self.x + h - _E, self.y + self.height * 0.5, self.z + h - _E)
        return self.in_water

    def _move_axis(self, axis, delta):
        if axis == 0:
            self.x += delta
        elif axis == 1:
            self.y += delta
        else:
            self.z += delta
        if not self.collides():
            return False
        h = self.half
        if axis == 0:
            self.x = (math.floor(self.x + h - _E) - h) if delta > 0 else (math.floor(self.x - h) + 1 + h)
            self.vx = 0.0
        elif axis == 1:
            self.y = (math.floor(self.y + self.height - _E) - self.height) if delta > 0 else (math.floor(self.y) + 1)
            self.vy = 0.0
        else:
            self.z = (math.floor(self.z + h - _E) - h) if delta > 0 else (math.floor(self.z - h) + 1 + h)
            self.vz = 0.0
        return True

    def move(self, dx, dy, dz):
        """Move with collision. Returns (hit_x, hit_y, hit_z)."""
        steps = max(1, math.ceil(max(abs(dx), abs(dy), abs(dz)) / 0.4))
        hx = hy = hz = False
        grounded = False
        for _ in range(steps):
            if dy:
                hit = self._move_axis(1, dy / steps)
                hy |= hit
                if hit and dy < 0:
                    grounded = True
            if dx:
                hx |= self._move_axis(0, dx / steps)
            if dz:
                hz |= self._move_axis(2, dz / steps)
        self.on_ground = grounded or (dy == 0 and self.on_ground)
        return hx, hy, hz

    def apply_gravity(self, dt, gravity=GRAVITY):
        if self.in_water:
            self.vy = max(self.vy - gravity * 0.25 * dt, -3.0)
            self.vy *= (1 - min(1.0, 2.0 * dt))
        else:
            self.vy = max(self.vy - gravity * dt, -TERMINAL_VELOCITY)

    def distance_to(self, x, y, z):
        return math.sqrt((self.x - x) ** 2 + (self.y - y) ** 2 + (self.z - z) ** 2)
