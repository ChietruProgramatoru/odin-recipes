// The small synthesiser every made sound is built with: filters, oscillators, noise grains, a voice
// (for the animals and monsters) and a little song mixer. Only sound*.cpp and music.cpp use it.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "rng.h"
#include "sound.h"

namespace synth {

constexpr double PI = 3.14159265358979323846;

// The M16 sounds (sound_effects.cpp) and the background music (music.cpp).
std::vector<float> make_effect(Sound sound);
std::vector<float> make_music(Sound sound);

inline std::vector<float> buffer(double seconds) { return std::vector<float>(static_cast<size_t>(SOUND_RATE * seconds)); }
inline double seconds_at(size_t i) { return static_cast<double>(i) / SOUND_RATE; }
inline double noise(Rng& rng) { return rng.random() * 2 - 1; }

// Scale so the loudest sample is `peak`.
inline void normalize(std::vector<float>& s, float peak) {
    float top = 0;
    for (float v : s) top = std::max(top, std::abs(v));
    if (top > 0)
        for (float& v : s) v *= peak / top;
}

// Fade the first and last `seconds` in and out (sounds that are played over each other, like rain).
inline void fade_ends(std::vector<float>& s, double in, double out) {
    const size_t n = s.size(), a = static_cast<size_t>(in * SOUND_RATE), b = static_cast<size_t>(out * SOUND_RATE);
    for (size_t i = 0; i < a && i < n; ++i) s[i] *= static_cast<float>(i) / static_cast<float>(a);
    for (size_t i = 0; i < b && i < n; ++i) s[n - 1 - i] *= static_cast<float>(i) / static_cast<float>(b);
}

// Add `src` into `dst` starting `at` seconds in, times `gain`.
inline void mix_into(std::vector<float>& dst, const std::vector<float>& src, double at, double gain) {
    const size_t i0 = static_cast<size_t>(std::max(0.0, at) * SOUND_RATE);
    for (size_t k = 0; k < src.size() && i0 + k < dst.size(); ++k) dst[i0 + k] += static_cast<float>(src[k] * gain);
}

// Echoes: copies `delay` seconds apart, each `feedback` times as loud as the last.
inline void echo(std::vector<float>& s, double delay, double feedback, int repeats) {
    const size_t d = static_cast<size_t>(delay * SOUND_RATE);
    for (int r = 0; r < repeats; ++r)
        for (size_t i = s.size(); i-- > d;) s[i] += s[i - d] * static_cast<float>(feedback);
}

// A sine from a table: much faster than std::sin for music. `phase` in turns (0..1).
inline double fast_sin(double phase) {
    constexpr int N = 4096;
    static const std::array<float, N + 1> table = [] {
        std::array<float, N + 1> t{};
        for (int i = 0; i <= N; ++i) t[i] = static_cast<float>(std::sin(2 * PI * i / N));
        return t;
    }();
    phase -= std::floor(phase);
    const double x = phase * N;
    const int i = static_cast<int>(x);
    return table[i] + (table[i + 1] - table[i]) * (x - i);
}

// A sine oscillator whose frequency may change every sample.
struct Osc {
    double phase = 0;
    double operator()(double freq) {
        phase += freq / SOUND_RATE;
        phase -= std::floor(phase);
        return fast_sin(phase);
    }
};

// A two-pole filter (the usual "biquad" recipes). The frequency can be moved while it runs.
struct Biquad {
    enum Kind { Low, High, Band };
    Kind kind = Low;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    Biquad() = default;
    Biquad(Kind k, double freq, double q) : kind(k) { set(freq, q); }
    void set(double freq, double q) {
        freq = std::clamp(freq, 10.0, SOUND_RATE * 0.45);
        const double w = 2 * PI * freq / SOUND_RATE, c = std::cos(w), alpha = std::sin(w) / (2 * q);
        const double a0 = 1 + alpha;
        switch (kind) {
            case Low: b0 = (1 - c) / 2, b1 = 1 - c, b2 = (1 - c) / 2; break;
            case High: b0 = (1 + c) / 2, b1 = -(1 + c), b2 = (1 + c) / 2; break;
            case Band: b0 = alpha, b1 = 0, b2 = -alpha; break;  // peak gain 1
        }
        b0 /= a0, b1 /= a0, b2 /= a0;
        a1 = -2 * c / a0;
        a2 = (1 - alpha) / a0;
    }
    double operator()(double x) {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = x, y2 = y1, y1 = y;
        return y;
    }
};

// Filter a whole sound.
inline void filter(std::vector<float>& s, Biquad f) {
    for (float& v : s) v = static_cast<float>(f(v));
}

// Short taps of noise, `count` of them in the first `spread` seconds (more near the start), each
// `grain` seconds long: the crunch of gravel, the rustle of grass, the patter of rain.
inline std::vector<float> grains(double seconds, int count, double spread, double grain, uint64_t seed) {
    std::vector<float> s = buffer(seconds);
    Rng rng(seed);
    const size_t len = std::max<size_t>(1, static_cast<size_t>(grain * SOUND_RATE));
    for (int g = 0; g < count; ++g) {
        const double at = spread * std::pow(rng.random(), 1.6);
        const double amp = 0.3 + rng.random() * 0.7;
        const size_t i0 = static_cast<size_t>(at * SOUND_RATE);
        for (size_t k = 0; k < len && i0 + k < s.size(); ++k)
            s[i0 + k] += static_cast<float>(noise(rng) * amp * std::exp(-4.0 * static_cast<double>(k) / static_cast<double>(len)));
    }
    return s;
}

// Ringing partials that die away: a knock on wood, a bone, a bell. Frequencies in Hz, each with how
// fast it dies (per second) and how loud it starts.
struct Mode {
    double freq, decay, gain;
};
inline std::vector<float> modes(double seconds, std::initializer_list<Mode> list) {
    std::vector<float> s = buffer(seconds);
    for (const Mode& m : list) {
        Osc osc;
        for (size_t i = 0; i < s.size(); ++i) s[i] += static_cast<float>(osc(m.freq) * m.gain * std::exp(-seconds_at(i) * m.decay));
    }
    return s;
}

// A voice: a buzzing throat (a sawtooth at the pitch) shaped by three resonances (formants, which
// make the vowel), with breath, wobble and growl. The animals and monsters all speak with it.
struct VoiceSpec {
    double seconds = 0.5;
    double f0[3] = {200, 200, 200};    // pitch at the start, the middle and the end
    double formants[3] = {500, 1000, 2500};
    double formants_end[3] = {0, 0, 0};  // where they glide to (0: stay)
    double q[3] = {5, 6, 8};
    double gains[3] = {1, 0.6, 0.25};
    double breath = 0.1;     // noise mixed into the throat
    double tone = 1.0;       // the buzz itself (0: only breath, like panting)
    double rough = 0;        // random pitch jitter each cycle (a growl)
    double vibrato_hz = 0, vibrato = 0;  // slow pitch wobble (fraction of the pitch)
    double tremolo_hz = 0, tremolo = 0;  // loudness wobble (a sheep's bleat)
    double attack = 0.02, release = 0.1;
    uint64_t seed = 1;
};
inline std::vector<float> voice(const VoiceSpec& v) {
    std::vector<float> s = buffer(v.seconds);
    Rng rng(v.seed);
    Biquad f[3];
    for (int k = 0; k < 3; ++k) f[k] = Biquad(Biquad::Band, v.formants[k], v.q[k]);
    Biquad soften(Biquad::Low, 5000, 0.7);
    double phase = 0, jitter = 0;
    Osc vib, trem;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / v.seconds;
        // The pitch glides start -> middle -> end (in steps of musical interval, not Hz).
        const double a = u < 0.5 ? v.f0[0] : v.f0[1], b = u < 0.5 ? v.f0[1] : v.f0[2];
        const double w = u < 0.5 ? u * 2 : u * 2 - 1;
        double f0 = a * std::pow(b / a, w);
        f0 *= 1 + v.vibrato * vib(v.vibrato_hz) + jitter;
        phase += f0 / SOUND_RATE;
        if (phase >= 1) {
            phase -= 1;
            jitter = v.rough * noise(rng) * 0.5;
        }
        const double buzz = (2 * phase - 1) * v.tone + noise(rng) * v.breath;
        if (i % 64 == 0)
            for (int k = 0; k < 3; ++k) {
                const double to = v.formants_end[k] > 0 ? v.formants_end[k] : v.formants[k];
                f[k].set(v.formants[k] * std::pow(to / v.formants[k], u), v.q[k]);
            }
        double x = 0;
        for (int k = 0; k < 3; ++k) x += f[k](buzz) * v.gains[k];
        x = soften(x);
        const double env = std::min(1.0, t / v.attack) * std::min(1.0, (v.seconds - t) / v.release);
        const double tr = 1 - v.tremolo * (0.5 + 0.5 * trem(v.tremolo_hz));
        s[i] = static_cast<float>(x * env * tr);
    }
    return s;
}

// A tiny synthesiser for the discs: notes mixed into a buffer.
struct Song {
    std::vector<float> s;
    explicit Song(double seconds) : s(static_cast<size_t>(SOUND_RATE * seconds)) {}
    static double freq(int semitone) { return 440.0 * std::pow(2.0, semitone / 12.0); }  // semitones from A4
    // shape: 0 soft sine, 1 bell, 2 square-ish lead, 3 bass, 4 pad
    void note(double start, double length, int semitone, double volume, int shape) {
        const int i0 = static_cast<int>(start * SOUND_RATE), n = static_cast<int>((length + 0.4) * SOUND_RATE);
        const double f = freq(semitone);
        for (int k = 0; k < n && i0 + k < static_cast<int>(s.size()); ++k) {
            const double t = static_cast<double>(k) / SOUND_RATE;
            const double held = t < length ? 1.0 : std::exp(-(t - length) * 12);
            const double attack = std::min(1.0, t / (shape == 4 ? 0.6 : 0.008));
            double v = 0;
            switch (shape) {
                case 0: v = std::sin(2 * PI * f * t) * std::exp(-t * 1.5); break;
                case 1: v = (std::sin(2 * PI * f * t) + 0.5 * std::sin(2 * PI * f * 2.76 * t) * std::exp(-t * 4) +
                             0.25 * std::sin(2 * PI * f * 5.4 * t) * std::exp(-t * 8)) * std::exp(-t * 2.2); break;
                case 2: {
                    const double ph = std::fmod(f * t, 1.0);
                    v = ((ph < 0.3 ? 1.0 : -0.6) * 0.5 + std::sin(2 * PI * f * t) * 0.5) * std::exp(-t * 3.0);
                    break;
                }
                case 3: v = (std::sin(2 * PI * f * t) + 0.3 * std::sin(4 * PI * f * t)) * std::exp(-t * 2.5); break;
                case 4: v = (std::sin(2 * PI * f * t) + std::sin(2 * PI * f * 1.003 * t) + 0.4 * std::sin(2 * PI * f * 2.0 * t)) / 2.4; break;
            }
            s[i0 + k] += static_cast<float>(v * volume * attack * held);
        }
    }
    void mix(const std::vector<float>& sample, double start, double volume) {
        const int i0 = static_cast<int>(start * SOUND_RATE);
        for (size_t k = 0; k < sample.size() && i0 + k < s.size(); ++k) s[i0 + k] += sample[k] * static_cast<float>(volume);
    }
    std::vector<float> done(double fade) {
        const int n = static_cast<int>(s.size()), f = static_cast<int>(fade * SOUND_RATE);
        for (int i = 0; i < f && i < n; ++i) s[n - 1 - i] *= static_cast<float>(i) / f;
        normalize(s, 0.55f);
        return std::move(s);
    }
};

}  // namespace synth
