#include "sound.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "rng.h"
#include "sound_synth.h"

namespace {

using namespace synth;

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

// --- Note blocks and music (M11) ---------------------------------------------------------

// A plucked string (Karplus-Strong): bright at first, mellowing as it dies away.
std::vector<float> pluck(double freq, double seconds, uint64_t seed, double damping = 0.996) {
    const int n = static_cast<int>(SOUND_RATE * seconds);
    const int period = std::max(2, static_cast<int>(SOUND_RATE / freq));
    std::vector<double> buf(period);
    Rng rng(seed);
    for (double& v : buf) v = rng.random() * 2 - 1;
    std::vector<float> s(n);
    for (int i = 0; i < n; ++i) {
        const int k = i % period, k2 = (i + 1) % period;
        const double v = buf[k];
        buf[k] = (buf[k] + buf[k2]) * 0.5 * damping;
        const double fade = std::min(1.0, (seconds - static_cast<double>(i) / SOUND_RATE) / 0.05);
        s[i] = static_cast<float>(v * fade);
    }
    normalize(s, 0.6f);
    return s;
}

std::vector<float> bass_drum() {
    const int n = SOUND_RATE / 2;
    std::vector<float> s(n);
    double phase = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        phase += 2 * PI * (50 + 120 * std::exp(-t * 30)) / SOUND_RATE;
        s[i] = static_cast<float>(std::sin(phase) * std::exp(-t * 9));
    }
    normalize(s, 0.8f);
    return s;
}

std::vector<float> snare(double seconds = 0.3, uint64_t seed = 77) {
    const int n = static_cast<int>(SOUND_RATE * seconds);
    std::vector<float> s(n);
    Rng rng(seed);
    double prev = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        const double white = rng.random() * 2 - 1;
        const double v = (white - prev * 0.5);
        prev = white;
        s[i] = static_cast<float>((v * 0.8 + std::sin(2 * PI * 190 * t) * 0.4) * std::exp(-t * 18));
    }
    normalize(s, 0.6f);
    return s;
}

std::vector<float> click() {
    const int n = SOUND_RATE / 10;
    std::vector<float> s(n);
    Rng rng(78);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        s[i] = static_cast<float>((rng.random() * 2 - 1) * std::exp(-t * 90) + std::sin(2 * PI * 2400 * t) * std::exp(-t * 60) * 0.5);
    }
    normalize(s, 0.5f);
    return s;
}

// A lever's or button's click: a short, dry tick of wood on stone.
std::vector<float> switch_click() {
    const int n = SOUND_RATE / 20;
    std::vector<float> s(n);
    Rng rng(79);
    double low = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        low += (rng.random() * 2 - 1 - low) * 0.35;
        s[i] = static_cast<float>(low * std::exp(-t * 140) + std::sin(2 * PI * 900 * t) * std::exp(-t * 120) * 0.6);
    }
    normalize(s, 0.45f);
    return s;
}

// A piston: a low thump sliding down (out) or up (in), with a puff of noise.
std::vector<float> piston(bool out) {
    const int n = static_cast<int>(SOUND_RATE * 0.35);
    std::vector<float> s(n);
    Rng rng(out ? 80 : 81);
    double phase = 0, low = 0;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / SOUND_RATE;
        const double f = out ? 160 - 90 * t / 0.35 : 90 + 70 * t / 0.35;
        phase += 2 * PI * f / SOUND_RATE;
        low += (rng.random() * 2 - 1 - low) * 0.15;
        const double env = std::min(1.0, t / 0.01) * std::exp(-t * 9);
        s[i] = static_cast<float>((std::sin(phase) * 0.8 + low * 0.9) * env);
    }
    normalize(s, 0.5f);
    return s;
}

// "13": slow, dark and a little uneasy. A drone in A minor, a low pulse, and bell notes that drift
// in and out of the scale, with a gust of filtered wind now and then.
std::vector<float> record_13() {
    constexpr double LEN = 52;
    Song song(LEN);
    Rng rng(1313);
    const int drone[4] = {-24, -17, -12, -19};  // A2, E3, A3, D3
    for (int bar = 0; bar < 13; ++bar) song.note(bar * 4.0, 4.2, drone[bar % 4], 0.25, 4);
    for (double t = 0; t < LEN - 2; t += 2.0) song.note(t, 0.6, -36 + (static_cast<int>(t) % 8 == 0 ? 0 : -2), 0.35, 3);
    const int scale[7] = {0, 2, 3, 5, 7, 8, 11};  // A harmonic minor
    double t = 3;
    int degree = 4;
    while (t < LEN - 4) {
        degree = std::clamp(degree + rng.randint(-2, 2), 0, 13);
        const int semi = scale[degree % 7] + 12 * (degree / 7) - (rng.below(6) == 0 ? 1 : 0);  // now and then a wrong note
        song.note(t, 1.2, semi, 0.3, 1);
        t += rng.choice({1.5, 2.0, 2.5, 3.0});
    }
    // Wind: noise through a slow low-pass, swelling twice.
    double low = 0;
    for (size_t i = 0; i < song.s.size(); ++i) {
        const double sec = static_cast<double>(i) / SOUND_RATE;
        low += ((rng.random() * 2 - 1) - low) * 0.01;
        const double swell = std::pow(std::max(0.0, std::sin(sec / LEN * 2 * PI * 2 - 1)), 3);
        song.s[i] += static_cast<float>(low * 1.6 * swell);
    }
    return song.done(4);
}

// "cat": a cheerful tune in C major over C - Am - F - G, with bass, kick, snare and hats.
std::vector<float> record_cat() {
    constexpr double BEAT = 0.5;  // 120 beats a minute
    constexpr int BARS = 24;
    Song song(BARS * 4 * BEAT + 2);
    Rng rng(4242);
    const int roots[4] = {-9, -12, -16, -14};  // C, A, F, G (below middle C)
    const int chord[4][3] = {{3, 7, 10}, {0, 3, 7}, {-4, 0, 3}, {-2, 2, 5}};
    const auto kick = bass_drum(), snr = snare(0.2, 91), hat = click();
    // Two 4-bar phrases of melody, made once and repeated like a song: A A B A.
    std::vector<std::pair<double, int>> phrase[2];
    for (auto& ph : phrase) {
        int note = 3;
        for (int step = 0; step < 32; ++step) {
            if (rng.below(5) == 0) continue;  // a rest
            const int bar = step / 8;
            const int target = chord[bar][rng.below(3)] + 12;
            note = rng.below(3) == 0 ? target : std::clamp(note + rng.randint(-2, 2), 0, 22);
            ph.push_back({step * BEAT / 2, note});
        }
    }
    const int order[6] = {0, 0, 1, 0, 1, 0};
    for (int bar = 0; bar < BARS; ++bar) {
        const double t0 = bar * 4 * BEAT;
        const int c = bar % 4;
        for (int beat = 0; beat < 4; ++beat) {
            const double t = t0 + beat * BEAT;
            song.note(t, BEAT * 0.8, roots[c] - 12 + (beat == 3 ? 7 : 0), 0.5, 3);
            if (beat % 2 == 0) song.mix(kick, t, 0.7);
            else song.mix(snr, t, 0.45);
            song.mix(hat, t + BEAT / 2, 0.25);
            for (int k : chord[c]) song.note(t, BEAT * 0.4, k, 0.07, 0);
        }
        if (bar >= 2 && bar < BARS - 1) {
            const auto& ph = phrase[order[(bar / 4) % 6]];
            for (const auto& [at, n] : ph)
                if (at >= (bar % 4) * 4 * BEAT && at < ((bar % 4) + 1) * 4 * BEAT) song.note(t0 + at - (bar % 4) * 4 * BEAT, BEAT * 0.45, n, 0.32, 2);
        }
    }
    song.note(BARS * 4 * BEAT, 1.5, -9, 0.5, 3);
    return song.done(1.5);
}

}  // namespace

std::vector<float> make_sound(Sound sound) {
    const double base = synth::Song::freq(-3);  // F#4
    switch (sound) {
        case Sound::Fuse: return fuse();
        case Sound::Explosion: return explosion();
        case Sound::NoteHarp: return pluck(base, 1.2, 11);
        case Sound::NoteBass: return pluck(base / 4, 1.0, 12, 0.993);
        case Sound::NoteBassDrum: return bass_drum();
        case Sound::NoteSnare: return snare();
        case Sound::NoteClick: return click();
        case Sound::Record13: return record_13();
        case Sound::RecordCat: return record_cat();
        case Sound::Click: return switch_click();
        case Sound::PistonOut: return piston(true);
        case Sound::PistonIn: return piston(false);
        default: break;
    }
    return is_music(sound) ? synth::make_music(sound) : synth::make_effect(sound);
}

bool is_music(Sound sound) {
    const int i = static_cast<int>(sound) - static_cast<int>(FIRST_MUSIC);
    return i >= 0 && i < MUSIC_COUNT;
}

float note_pitch(int note) { return static_cast<float>(std::pow(2.0, (note - 12) / 12.0)); }

double sound_range(Sound sound) {
    switch (sound) {
        case Sound::Explosion: case Sound::Record13: case Sound::RecordCat: return 64.0;  // Beta: 4 times louder
        case Sound::NoteHarp: case Sound::NoteBass: case Sound::NoteBassDrum: case Sound::NoteSnare: case Sound::NoteClick:
            return 48.0;
        case Sound::Thunder: return 1000.0;  // Beta: heard all over the world
        default: return 16.0;
    }
}

StereoGain sound_gain(Sound sound, glm::dvec3 listener, double yaw_degrees, glm::dvec3 source, float volume) {
    const glm::dvec3 d = source - listener;
    const double dist = glm::length(d);
    // Beta: a louder sound carries further but is never louder than full.
    const double range = sound_range(sound) * std::max(1.0f, volume);
    const double gain = std::clamp(1.0 - dist / range, 0.0, 1.0) * std::min(1.0f, volume);
    if (gain <= 0) return {};
    // -1 all left, +1 all right. Right of the way you face (yaw 0 faces +z) is -x.
    const double yaw = yaw_degrees * synth::PI / 180.0;
    const glm::dvec3 right(-std::cos(yaw), 0, std::sin(yaw));
    const double flat = std::hypot(d.x, d.z);
    const double pan = flat > 1e-6 ? glm::dot(glm::dvec3(d.x, 0, d.z) / flat, right) * 0.7 : 0.0;
    // Constant power: the sound is as loud overall wherever it is panned.
    const double angle = (pan + 1) * synth::PI / 4;
    return {static_cast<float>(gain * std::cos(angle) * std::sqrt(2.0)),
            static_cast<float>(gain * std::sin(angle) * std::sqrt(2.0))};
}
