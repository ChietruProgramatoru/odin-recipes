// The background music (M16): four calm pieces of this game's own, written out by the code the
// same way every time. A soft piano plays broken chords and a simple tune over a warm pad, in a
// little reverb. Nothing here is taken from Minecraft's music.
#include <cmath>

#include "sound_synth.h"

namespace synth {
namespace {

// A piano-ish note: a few slightly stretched partials, the higher ones dying away faster, and a
// short release when the key is let go after `length` seconds.
void piano(std::vector<float>& s, double start, double length, double freq, double velocity) {
    const size_t i0 = static_cast<size_t>(start * SOUND_RATE);
    for (int n = 1; n <= 6; ++n) {
        const double f = freq * n * std::sqrt(1 + 0.0004 * n * n);
        if (f > 9000) break;
        const double amp = velocity / std::pow(n, 1.4);
        const double decay = 0.5 + 0.6 * n + freq / 700;
        const double seconds = std::min(length + 1.2, 7.0 / decay);
        const size_t len = static_cast<size_t>(seconds * SOUND_RATE);
        double phase = 0;
        for (size_t k = 0; k < len && i0 + k < s.size(); ++k) {
            const double t = seconds_at(k);
            phase += f / SOUND_RATE;
            const double held = t < length ? 1.0 : std::exp(-(t - length) * 6);
            const double env = std::min(1.0, t / 0.004) * std::exp(-t * decay) * held;
            s[i0 + k] += static_cast<float>(fast_sin(phase) * amp * env);
        }
    }
}

// A soft pad: three slightly detuned voices with a slow swell and a long release.
void pad(std::vector<float>& s, double start, double length, double freq, double volume) {
    const size_t i0 = static_cast<size_t>(start * SOUND_RATE);
    const size_t len = static_cast<size_t>((length + 2.0) * SOUND_RATE);
    double ph[3] = {0, 0.31, 0.67};
    const double detune[3] = {1.0, 1.004, 0.996};
    for (size_t k = 0; k < len && i0 + k < s.size(); ++k) {
        const double t = seconds_at(k);
        const double env = std::min(1.0, t / 1.5) * (t < length ? 1.0 : std::exp(-(t - length) * 2.5));
        double v = 0;
        for (int j = 0; j < 3; ++j) {
            ph[j] += freq * detune[j] / SOUND_RATE;
            v += fast_sin(ph[j]) + 0.3 * fast_sin(ph[j] * 2) + 0.1 * fast_sin(ph[j] * 3);
        }
        s[i0 + k] += static_cast<float>(v / 3 * volume * env);
    }
}

// A small room: four filtered echo loops and two smearing stages (a Schroeder reverb).
void reverb(std::vector<float>& s, double wet) {
    const int combs[4] = {1557, 1617, 1491, 1422};
    const int allpasses[2] = {556, 441};
    std::vector<float> out(s.size(), 0.0f);
    for (int d : combs) {
        std::vector<float> line(d, 0.0f);
        double low = 0;
        size_t p = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            const float y = line[p];
            low = y * 0.6 + low * 0.4;  // the walls soak up the highs
            line[p] = static_cast<float>(s[i] + low * 0.82);
            out[i] += y * 0.25f;
            p = (p + 1) % d;
        }
    }
    for (int d : allpasses) {
        std::vector<float> line(d, 0.0f);
        size_t p = 0;
        for (float& v : out) {
            const float b = line[p];
            line[p] = v + b * 0.5f;
            v = b - v * 0.5f;
            p = (p + 1) % d;
        }
    }
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(s[i] * (1 - wet) + out[i] * wet * 1.6);
}

double note_freq(int semitone) { return 440.0 * std::pow(2.0, semitone / 12.0); }  // semitones from A4

// How a piece goes. Chords are semitones above the key's tonic (root first); the tune walks the
// scale and lands on chord notes.
struct Piece {
    double bpm;
    int beats;              // in a bar
    int tonic;              // semitones from A4
    std::vector<std::array<int, 4>> chords;  // one a bar, repeating
    std::array<int, 7> scale;
    int bars;
    double tune_density;    // how many eighth notes have a tune note (0..1)
    double pad_volume;
    uint64_t seed;
};

std::vector<float> play(const Piece& p) {
    const double beat = 60.0 / p.bpm, bar = beat * p.beats;
    std::vector<float> s = buffer(p.bars * bar + 6);
    Rng rng(p.seed);
    const int n_chords = static_cast<int>(p.chords.size());

    // Two four-bar tunes, made once and repeated like a song: intro, A A B A, then the last chord.
    const int intro = 2;
    std::vector<std::pair<double, int>> tunes[2];
    for (auto& tune : tunes) {
        int degree = 7 + rng.below(3);
        for (int step = 0; step < 4 * p.beats * 2; ++step) {
            if (rng.random() > p.tune_density) continue;
            const int chord = (intro + step / (p.beats * 2)) % n_chords;
            const bool strong = step % (p.beats * 2) == 0;
            if (strong) {
                // Land on a note of the chord near where the tune is.
                int best = degree, best_d = 99;
                for (int d = 4; d <= 13; ++d) {
                    const int semi = p.scale[d % 7] + 12 * (d / 7);
                    for (int c = 0; c < 3; ++c)
                        if ((semi - p.chords[chord][c]) % 12 == 0 && std::abs(d - degree) < best_d) best = d, best_d = std::abs(d - degree);
                }
                degree = best;
            } else {
                degree = std::clamp(degree + rng.choice({-2, -1, -1, 1, 1, 2}), 4, 13);
            }
            tune.push_back({step * beat / 2, degree});
        }
    }
    const int sections = (p.bars - intro - 1) / 4;
    for (int b = 0; b < p.bars; ++b) {
        const double t0 = b * bar;
        const auto& ch = p.chords[b % n_chords];
        const bool last = b == p.bars - 1;
        // The pad holds the chord all bar (and the last one rings on).
        const double hold = last ? bar * 2 : bar;
        for (int c = 0; c < 3; ++c) pad(s, t0, hold, note_freq(p.tonic + ch[c] - 12), p.pad_volume);
        if (last) {
            for (int c = 0; c < 4; ++c) piano(s, t0 + c * 0.08, bar * 1.5, note_freq(p.tonic + ch[c]), 0.28);
            piano(s, t0, bar * 1.5, note_freq(p.tonic + ch[0] - 24), 0.3);
            break;
        }
        // Broken chords in eighth notes: the root low, then the chord rising and falling.
        const int pattern[8] = {ch[0] - 24, ch[2] - 12, ch[1], ch[2], ch[3], ch[2], ch[1], ch[2] - 12};
        for (int e = 0; e < p.beats * 2; ++e) {
            const double vel = (e == 0 ? 0.32 : 0.2) * (0.85 + 0.3 * rng.random());
            piano(s, t0 + e * beat / 2 + rng.uniform(0, 0.012), beat * (e == 0 ? 2.0 : 0.9), note_freq(p.tonic + pattern[e % 8]), vel);
        }
        // The tune, from the third bar on.
        const int section = (b - intro) / 4;
        if (b < intro || section >= sections) continue;
        static const int order[4] = {0, 0, 1, 0};
        const auto& tune = tunes[order[section % 4]];
        const double from = ((b - intro) % 4) * bar;
        for (size_t k = 0; k < tune.size(); ++k) {
            const auto& [at, degree] = tune[k];
            if (at < from || at >= from + bar) continue;
            const double next = k + 1 < tune.size() ? tune[k + 1].first : at + beat * 2;
            const int semi = p.scale[degree % 7] + 12 * (degree / 7);
            piano(s, t0 + at - from, std::min(next - at, beat * 2) * 0.95, note_freq(p.tonic + semi), 0.36 * (0.9 + 0.2 * rng.random()));
        }
    }
    reverb(s, 0.3);
    fade_ends(s, 0.5, 4);
    normalize(s, 0.5f);
    return s;
}

constexpr std::array<int, 7> MAJOR{0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 7> LYDIAN{0, 2, 4, 6, 7, 9, 11};
constexpr std::array<int, 7> MINOR{0, 2, 3, 5, 7, 8, 10};

}  // namespace

std::vector<float> make_music(Sound sound) {
    switch (sound) {
        // "Dawn": D major, unhurried, the sun coming up (I - V - vi - IV).
        case Sound::MusicDawn:
            return play({66, 4, -7, {{0, 4, 7, 12}, {7, 11, 14, 19}, {9, 12, 16, 21}, {5, 9, 12, 17}}, MAJOR, 27, 0.55, 0.05, 1601});
        // "Meadow": G lydian in three, light on its feet.
        case Sound::MusicMeadow:
            return play({84, 3, -2, {{0, 4, 7, 12}, {2, 6, 9, 14}, {-5, -1, 2, 7}, {0, 4, 7, 11}}, LYDIAN, 35, 0.6, 0.04, 1602});
        // "Lanterns": A minor, slow and thoughtful, for evenings underground.
        case Sound::MusicLanterns:
            return play({60, 4, -12, {{0, 3, 7, 12}, {-4, 0, 3, 8}, {3, 7, 10, 15}, {-2, 2, 5, 10}}, MINOR, 24, 0.45, 0.06, 1603});
        // "Drift": E major sevenths, barely moving, mostly pad.
        case Sound::MusicDrift:
            return play({54, 4, -5, {{0, 4, 7, 11}, {-3, 0, 4, 7}, {-7, -3, 0, 4}, {-5, -1, 2, 7}}, MAJOR, 22, 0.3, 0.08, 1604});
        default: return {};
    }
}

}  // namespace synth
