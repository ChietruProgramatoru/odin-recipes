// Small seeded random number generator (SplitMix64).
// The standard library's distributions differ between compilers, so the game uses this
// everywhere to get the same worlds and textures on Windows and Linux.
#pragma once
#include <cstdint>
#include <initializer_list>

class Rng {
public:
    explicit Rng(uint64_t seed = 0) : state_(seed) {}

    uint64_t next() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // Uniform in [0, 1).
    double random() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    // Uniform in [a, b).
    double uniform(double a, double b) { return a + (b - a) * random(); }
    // Integer in [0, n).
    int below(int n) { return static_cast<int>(random() * n); }
    // Integer in [a, b], both ends included.
    int randint(int a, int b) { return a + below(b - a + 1); }
    template <typename T>
    T choice(std::initializer_list<T> items) { return items.begin()[below(static_cast<int>(items.size()))]; }

private:
    uint64_t state_;
};
