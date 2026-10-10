// 2D and 3D Perlin noise.
#pragma once
#include <cstdint>

class Perlin2D {
public:
    explicit Perlin2D(uint64_t seed);
    double noise(double x, double y) const;  // roughly [-1, 1]
    double fbm(double x, double y, int octaves = 4, double lacunarity = 2.0, double gain = 0.5) const;

private:
    int perm_[512];
    double gx_[16], gy_[16];
};

class Perlin3D {
public:
    explicit Perlin3D(uint64_t seed);
    double noise(double x, double y, double z) const;  // roughly [-1, 1]
    double fbm(double x, double y, double z, int octaves = 4, double lacunarity = 2.0, double gain = 0.5) const;

private:
    int perm_[512];
};
