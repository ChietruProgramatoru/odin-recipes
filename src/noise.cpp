#include "noise.h"

#include <cmath>
#include <utility>

#include "rng.h"

namespace {
double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
}  // namespace

Perlin2D::Perlin2D(uint64_t seed) {
    Rng rng(seed);
    int p[256];
    for (int i = 0; i < 256; ++i) p[i] = i;
    for (int i = 255; i > 0; --i) std::swap(p[i], p[rng.below(i + 1)]);  // Fisher-Yates
    for (int i = 0; i < 512; ++i) perm_[i] = p[i & 255];
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
