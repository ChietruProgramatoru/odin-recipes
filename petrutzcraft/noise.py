"""Small pure-Python 2D Perlin noise (no numpy needed)."""
import math
import random


def _fade(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


class Perlin2D:
    def __init__(self, seed):
        rng = random.Random(seed)
        perm = list(range(256))
        rng.shuffle(perm)
        self.perm = perm + perm
        self.grads = [(math.cos(a), math.sin(a)) for a in (2 * math.pi * i / 16 for i in range(16))]

    def noise(self, x, y):
        xf, yf = math.floor(x), math.floor(y)
        dx, dy = x - xf, y - yf
        xi, yi = int(xf) & 255, int(yf) & 255
        p, g = self.perm, self.grads
        aa, ab = p[p[xi] + yi] & 15, p[p[xi] + yi + 1] & 15
        ba, bb = p[p[xi + 1] + yi] & 15, p[p[xi + 1] + yi + 1] & 15
        u, v = _fade(dx), _fade(dy)
        n00 = g[aa][0] * dx + g[aa][1] * dy
        n10 = g[ba][0] * (dx - 1) + g[ba][1] * dy
        n01 = g[ab][0] * dx + g[ab][1] * (dy - 1)
        n11 = g[bb][0] * (dx - 1) + g[bb][1] * (dy - 1)
        x1 = n00 + (n10 - n00) * u
        x2 = n01 + (n11 - n01) * u
        return (x1 + (x2 - x1) * v) * 1.41   # roughly [-1, 1]

    def fbm(self, x, y, octaves=4, lacunarity=2.0, gain=0.5):
        total, amp, freq, norm = 0.0, 1.0, 1.0, 0.0
        for _ in range(octaves):
            total += self.noise(x * freq, y * freq) * amp
            norm += amp
            amp *= gain
            freq *= lacunarity
        return total / norm
