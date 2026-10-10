#include "noise.h"

#include <cmath>
#include <utility>

#include "rng.h"

namespace {
double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }

void shuffled_perm(uint64_t seed, int* perm) {
    Rng rng(seed);
    int p[256];
    for (int i = 0; i < 256; ++i) p[i] = i;
    for (int i = 255; i > 0; --i) std::swap(p[i], p[rng.below(i + 1)]);  // Fisher-Yates
    for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
}

// Dot product of the offset with one of Perlin's 12 edge gradients.
double grad3(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

int wrap(double f) { return static_cast<int>(static_cast<long long>(f) & 255); }
}  // namespace

Perlin2D::Perlin2D(uint64_t seed) {
    shuffled_perm(seed, perm_);
    for (int i = 0; i < 16; ++i) {
        double a = 2 * 3.14159265358979323846 * i / 16;
        gx_[i] = std::cos(a);
        gy_[i] = std::sin(a);
    }
}

double Perlin2D::noise(double x, double y) const {
    double xf = std::floor(x), yf = std::floor(y);
    double dx = x - xf, dy = y - yf;
    int xi = static_cast<int>(static_cast<long long>(xf) & 255);
    int yi = static_cast<int>(static_cast<long long>(yf) & 255);
    const int* p = perm_;
    int aa = p[p[xi] + yi] & 15, ab = p[p[xi] + yi + 1] & 15;
    int ba = p[p[xi + 1] + yi] & 15, bb = p[p[xi + 1] + yi + 1] & 15;
    double u = fade(dx), v = fade(dy);
    double n00 = gx_[aa] * dx + gy_[aa] * dy;
    double n10 = gx_[ba] * (dx - 1) + gy_[ba] * dy;
    double n01 = gx_[ab] * dx + gy_[ab] * (dy - 1);
    double n11 = gx_[bb] * (dx - 1) + gy_[bb] * (dy - 1);
    double x1 = n00 + (n10 - n00) * u;
    double x2 = n01 + (n11 - n01) * u;
    return (x1 + (x2 - x1) * v) * 1.41;
}

double Perlin2D::fbm(double x, double y, int octaves, double lacunarity, double gain) const {
    double total = 0, amp = 1, freq = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        total += noise(x * freq, y * freq) * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return total / norm;
}

Perlin3D::Perlin3D(uint64_t seed) { shuffled_perm(seed, perm_); }

double Perlin3D::noise(double x, double y, double z) const {
    double xf = std::floor(x), yf = std::floor(y), zf = std::floor(z);
    int X = wrap(xf), Y = wrap(yf), Z = wrap(zf);
    x -= xf;
    y -= yf;
    z -= zf;
    double u = fade(x), v = fade(y), w = fade(z);
    const int* p = perm_;
    int a = p[X] + Y, aa = p[a] + Z, ab = p[a + 1] + Z;
    int b = p[X + 1] + Y, ba = p[b] + Z, bb = p[b + 1] + Z;
    auto lerp = [](double t, double a, double b) { return a + t * (b - a); };
    return lerp(w,
                lerp(v, lerp(u, grad3(p[aa], x, y, z), grad3(p[ba], x - 1, y, z)),
                     lerp(u, grad3(p[ab], x, y - 1, z), grad3(p[bb], x - 1, y - 1, z))),
                lerp(v, lerp(u, grad3(p[aa + 1], x, y, z - 1), grad3(p[ba + 1], x - 1, y, z - 1)),
                     lerp(u, grad3(p[ab + 1], x, y - 1, z - 1), grad3(p[bb + 1], x - 1, y - 1, z - 1))));
}

double Perlin3D::fbm(double x, double y, double z, int octaves, double lacunarity, double gain) const {
    double total = 0, amp = 1, freq = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        total += noise(x * freq, y * freq, z * freq) * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return total / norm;
}
