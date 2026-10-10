#include "sound.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "rng.h"

namespace {

constexpr double PI = 3.14159265358979323846;

// Scale so the loudest sample is `peak`.
void normalize(std::vector<float>& s, float peak) {
    float top = 0;
    for (float v : s) top = std::max(top, std::abs(v));
    if (top > 0)
        for (float& v : s) v *= peak / top;
}

std::vector<float> fuse() {
    const int n = static_cast<int>(SOUND_RATE * 1.5);
    std::vector<float> s(n);
    Rng rng(41);
    double prev = 0, low = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        const double white = rng.random() * 2 - 1;
        // High-passed noise is a hiss; a little of the low part keeps it from sounding thin.
        const double hiss = white - prev;
        prev = white;
        low += (white - low) * 0.2;
        double v = hiss * 0.8 + low * 0.3;
        // Now and then a crackle, like a burning fuse.
        if (rng.below(2500) == 0) v += (rng.random() < 0.5 ? -1 : 1) * 2.5;
        const double attack = std::min(1.0, t / 0.05);
        const double swell = 0.6 + 0.4 * t / 1.5;
        const double release = std::min(1.0, (1.5 - t) / 0.15);
        s[i] = static_cast<float>(v * attack * swell * release);
    }
    normalize(s, 0.5f);
    return s;
}

std::vector<float> explosion() {
    const int n = SOUND_RATE * 2;
    std::vector<float> s(n);
    Rng rng(42);
    double low = 0, lower = 0, phase = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        const double white = rng.random() * 2 - 1;
        // A rumble: noise through two low-pass filters that close as it dies away.
        const double cut = 0.25 * std::exp(-t * 1.5) + 0.02;
        low += (white - low) * cut;
        lower += (low - lower) * cut;
        const double rumble = (lower * 3.0 + low) * std::exp(-t * 2.2);
        // The thump: a deep tone sliding down from 70 Hz.
        phase += 2 * PI * (35 + 35 * std::exp(-t * 8)) / SOUND_RATE;
        const double thump = std::sin(phase) * std::exp(-t * 5);
        // The crack at the start.
        const double crack = white * std::exp(-t * 40);
        const double attack = std::min(1.0, t / 0.004);
        s[i] = static_cast<float>((rumble * 1.0 + thump * 0.8 + crack * 0.5) * attack);
    }
    normalize(s, 0.9f);
    return s;
}

}  // namespace

std::vector<float> make_sound(Sound sound) {
    switch (sound) {
        case Sound::Fuse: return fuse();
        case Sound::Explosion: return explosion();
    }
    return {};
}

double sound_range(Sound sound) { return sound == Sound::Explosion ? 64.0 : 16.0; }

StereoGain sound_gain(Sound sound, glm::dvec3 listener, double yaw_degrees, glm::dvec3 source) {
    const glm::dvec3 d = source - listener;
    const double dist = glm::length(d);
    const double gain = std::clamp(1.0 - dist / sound_range(sound), 0.0, 1.0);
    if (gain <= 0) return {};
    // -1 all left, +1 all right. Right of the way you face (yaw 0 faces +z) is -x.
    const double yaw = yaw_degrees * PI / 180.0;
    const glm::dvec3 right(-std::cos(yaw), 0, std::sin(yaw));
    const double flat = std::hypot(d.x, d.z);
    const double pan = flat > 1e-6 ? glm::dot(glm::dvec3(d.x, 0, d.z) / flat, right) * 0.7 : 0.0;
    // Constant power: the sound is as loud overall wherever it is panned.
    const double angle = (pan + 1) * PI / 4;
    return {static_cast<float>(gain * std::cos(angle) * std::sqrt(2.0)),
            static_cast<float>(gain * std::sin(angle) * std::sqrt(2.0))};
}
