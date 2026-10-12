// The sound effects that came with M16: blocks, the player's things, the world around, animals,
// monsters and the menus. All made here from noise, filters and a buzzing "voice"; no recordings.
#include <cmath>

#include "sound_synth.h"

namespace synth {
namespace {

// --- blocks ------------------------------------------------------------------------------------
// One sound per material, short enough for footsteps. Breaking and placing play it lower and louder.

// Stone: a hard, gritty scrape with a dull knock under it.
std::vector<float> step_stone() {
    std::vector<float> s = grains(0.25, 30, 0.07, 0.004, 101);
    filter(s, Biquad(Biquad::Band, 2300, 0.9));
    std::vector<float> knock = grains(0.25, 4, 0.01, 0.02, 102);
    filter(knock, Biquad(Biquad::Low, 350, 0.8));
    mix_into(s, knock, 0, 1.5);
    for (size_t i = 0; i < s.size(); ++i) s[i] *= static_cast<float>(std::exp(-seconds_at(i) * 18));
    normalize(s, 0.7f);
    return s;
}

// Wood: a hollow knock, ringing a moment at a few low pitches.
std::vector<float> step_wood() {
    std::vector<float> s = modes(0.3, {{190, 28, 1.0}, {430, 38, 0.6}, {780, 55, 0.35}, {1270, 75, 0.2}});
    std::vector<float> tap = grains(0.3, 3, 0.005, 0.006, 103);
    filter(tap, Biquad(Biquad::Band, 1500, 1.0));
    mix_into(s, tap, 0, 0.8);
    normalize(s, 0.7f);
    return s;
}

// Gravel and dirt: lots of little stones crunching.
std::vector<float> step_gravel() {
    std::vector<float> s = grains(0.35, 140, 0.22, 0.003, 104);
    filter(s, Biquad(Biquad::Band, 1300, 0.7));
    filter(s, Biquad(Biquad::Low, 4500, 0.7));
    for (size_t i = 0; i < s.size(); ++i) s[i] *= static_cast<float>(std::exp(-seconds_at(i) * 7));
    normalize(s, 0.7f);
    return s;
}

// Grass and leaves: a soft, high rustle.
std::vector<float> step_grass() {
    std::vector<float> s = grains(0.3, 90, 0.2, 0.007, 105);
    filter(s, Biquad(Biquad::Band, 4200, 0.6));
    for (size_t i = 0; i < s.size(); ++i) s[i] *= static_cast<float>(std::exp(-seconds_at(i) * 8));
    normalize(s, 0.55f);
    return s;
}

// Sand: a smooth shush.
std::vector<float> step_sand() {
    std::vector<float> s = buffer(0.3);
    Rng rng(106);
    Biquad low(Biquad::Low, 2600, 0.7), high(Biquad::High, 500, 0.7);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        s[i] = static_cast<float>(high(low(noise(rng))) * std::min(1.0, t / 0.03) * std::exp(-t * 11));
    }
    std::vector<float> grit = grains(0.3, 25, 0.15, 0.003, 107);
    filter(grit, Biquad(Biquad::Band, 2000, 1.0));
    mix_into(s, grit, 0, 0.3);
    normalize(s, 0.55f);
    return s;
}

// Wool and snow: a muffled pat.
std::vector<float> step_cloth() {
    std::vector<float> s = buffer(0.2);
    Rng rng(108);
    Biquad low(Biquad::Low, 700, 0.7);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        s[i] = static_cast<float>(low(noise(rng)) * std::min(1.0, t / 0.015) * std::exp(-t * 20));
    }
    normalize(s, 0.6f);
    return s;
}

// Glass breaking: a crash, then shards tinkling down.
std::vector<float> glass_break() {
    std::vector<float> s = buffer(0.9);
    Rng rng(109);
    Biquad high(Biquad::High, 2200, 0.7);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(high(noise(rng)) * std::exp(-seconds_at(i) * 14) * 0.8);
    for (int k = 0; k < 45; ++k) {
        const double at = 0.5 * std::pow(rng.random(), 1.8);
        const double f = rng.uniform(2500, 7500);
        std::vector<float> shard = modes(0.15, {{f, rng.uniform(30, 70), 1.0}, {f * 1.47, 80, 0.5}});
        mix_into(s, shard, at, (0.25 + rng.random() * 0.3) * std::exp(-at * 3));
    }
    normalize(s, 0.6f);
    return s;
}

// --- the player and the things they use ----------------------------------------------------------

// Hurt: a short "oof".
std::vector<float> hurt() {
    VoiceSpec v;
    v.seconds = 0.28;
    v.f0[0] = 230, v.f0[1] = 205, v.f0[2] = 140;
    v.formants[0] = 650, v.formants[1] = 1100, v.formants[2] = 2500;
    v.breath = 0.2, v.attack = 0.01, v.release = 0.1, v.seed = 201;
    std::vector<float> s = voice(v);
    normalize(s, 0.7f);
    return s;
}

// Pop: picking something up.
std::vector<float> pop() {
    std::vector<float> s = buffer(0.12);
    Osc osc;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        s[i] = static_cast<float>(osc(420 + 900 * std::min(1.0, t / 0.05)) * std::min(1.0, t / 0.002) * std::exp(-t * 45));
    }
    normalize(s, 0.5f);
    return s;
}

// A plucked string (Karplus-Strong), for the bow.
std::vector<float> string_pluck(double freq, double seconds, uint64_t seed) {
    std::vector<float> s = buffer(seconds);
    const int period = std::max(2, static_cast<int>(SOUND_RATE / freq));
    std::vector<double> buf(period);
    Rng rng(seed);
    for (double& v : buf) v = noise(rng);
    for (size_t i = 0; i < s.size(); ++i) {
        const size_t k = i % period, k2 = (i + 1) % period;
        s[i] = static_cast<float>(buf[k]);
        buf[k] = (buf[k] + buf[k2]) * 0.5 * 0.995;
    }
    return s;
}

// Bow: the string's twang and the arrow's whoosh.
std::vector<float> bow() {
    std::vector<float> s = string_pluck(130, 0.45, 202);
    for (size_t i = 0; i < s.size(); ++i) s[i] *= static_cast<float>(std::exp(-seconds_at(i) * 7));
    std::vector<float> whoosh = buffer(0.35);
    Rng rng(203);
    Biquad band(Biquad::Band, 900, 1.2);
    for (size_t i = 0; i < whoosh.size(); ++i) {
        const double t = seconds_at(i);
        band.set(900 + 2500 * t, 1.2);
        whoosh[i] = static_cast<float>(band(noise(rng)) * std::sin(PI * t / 0.35));
    }
    mix_into(s, whoosh, 0.02, 1.2);
    normalize(s, 0.6f);
    return s;
}

// An arrow thudding into a block and quivering.
std::vector<float> arrow_hit() {
    std::vector<float> s = modes(0.4, {{240, 40, 1.0}, {520, 60, 0.5}});
    Osc buzz, wobble;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        const double q = buzz(310) * (0.5 + 0.5 * wobble(38)) * std::exp(-t * 9) * 0.6;
        s[i] += static_cast<float>(q);
    }
    normalize(s, 0.6f);
    return s;
}

// A creak: wood rubbing on wood, a run of little catches that speed up from `rate0` to `rate1`
// a second, each ringing the board's resonances.
std::vector<float> creak(double seconds, double rate0, double rate1, double f1, double f2, uint64_t seed) {
    std::vector<float> s = buffer(seconds);
    Rng rng(seed);
    double phase = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / seconds;
        phase += (rate0 + (rate1 - rate0) * u) * (1 + 0.15 * noise(rng)) / SOUND_RATE;
        if (phase >= 1) {
            phase -= 1;
            s[i] = static_cast<float>(1.0 + 0.3 * rng.random());
        }
    }
    std::vector<float> a = s, b = s;
    filter(a, Biquad(Biquad::Band, f1, 6));
    filter(b, Biquad(Biquad::Band, f2, 8));
    for (size_t i = 0; i < s.size(); ++i) {
        const double u = seconds_at(i) / seconds;
        s[i] = static_cast<float>((a[i] + b[i] * 0.6) * std::sin(PI * std::min(1.0, u * 1.1)));
    }
    return s;
}

std::vector<float> door_open() {
    std::vector<float> s = creak(0.45, 45, 110, 620, 1450, 204);
    normalize(s, 0.55f);
    return s;
}

// A door shutting: a wooden thump and the latch's click.
std::vector<float> door_close() {
    std::vector<float> s = modes(0.35, {{115, 22, 1.0}, {255, 32, 0.6}, {540, 50, 0.3}});
    std::vector<float> latch = modes(0.05, {{3100, 120, 1.0}, {4700, 160, 0.5}});
    mix_into(s, latch, 0.025, 0.4);
    normalize(s, 0.65f);
    return s;
}

std::vector<float> chest_open() {
    std::vector<float> s = creak(0.65, 25, 60, 420, 950, 205);
    normalize(s, 0.55f);
    return s;
}

// A chest's lid dropping shut.
std::vector<float> chest_close() {
    std::vector<float> s = modes(0.4, {{90, 18, 1.0}, {210, 28, 0.5}, {430, 45, 0.25}});
    std::vector<float> thud = grains(0.4, 3, 0.005, 0.03, 206);
    filter(thud, Biquad(Biquad::Low, 400, 0.7));
    mix_into(s, thud, 0, 0.8);
    normalize(s, 0.65f);
    return s;
}

// Little rising blips: bubbles in water (and lava).
void bubbles(std::vector<float>& s, int count, double f_lo, double f_hi, double spread, double gain, Rng& rng) {
    for (int k = 0; k < count; ++k) {
        const double at = rng.uniform(0, spread), f = rng.uniform(f_lo, f_hi), len = rng.uniform(0.02, 0.06);
        std::vector<float> b = buffer(len);
        Osc osc;
        for (size_t i = 0; i < b.size(); ++i) {
            const double t = seconds_at(i);
            b[i] = static_cast<float>(osc(f * (1 + 1.5 * t / len)) * std::sin(PI * t / len));
        }
        mix_into(s, b, at, gain * (0.4 + 0.6 * rng.random()));
    }
}

// Splash: water thrown up, then bubbles.
std::vector<float> splash() {
    std::vector<float> s = buffer(0.8);
    Rng rng(207);
    Biquad low(Biquad::Low, 4000, 0.7);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        if (i % 64 == 0) low.set(4000 * std::exp(-t * 3) + 500, 0.7);
        s[i] = static_cast<float>(low(noise(rng)) * std::min(1.0, t / 0.01) * std::exp(-t * 5));
    }
    bubbles(s, 22, 450, 1300, 0.6, 0.25, rng);
    normalize(s, 0.6f);
    return s;
}

// Fizz: something hot meets water (or lava meets water).
std::vector<float> fizz() {
    std::vector<float> s = buffer(0.6);
    Rng rng(208);
    Biquad high(Biquad::High, 3000, 0.7);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        double v = high(noise(rng));
        if (rng.below(400) == 0) v += noise(rng) * 3;
        s[i] = static_cast<float>(v * std::min(1.0, t / 0.01) * std::exp(-t * 5));
    }
    normalize(s, 0.45f);
    return s;
}

// Flint and steel: a scrape, and the flame catching.
std::vector<float> ignite() {
    std::vector<float> s = buffer(0.5);
    Rng rng(209);
    Biquad scrape(Biquad::Band, 3200, 2.0), flame(Biquad::Low, 300, 0.8);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        const double a = t < 0.12 ? scrape(noise(rng)) * (0.6 + 0.4 * rng.random()) * std::sin(PI * t / 0.12) : 0;
        if (i % 64 == 0) flame.set(300 + 1800 * std::min(1.0, t / 0.4), 0.8);
        const double b = t > 0.06 ? flame(noise(rng)) * std::sin(PI * std::min(1.0, (t - 0.06) / 0.44)) : 0;
        s[i] = static_cast<float>(a * 0.8 + b);
    }
    normalize(s, 0.55f);
    return s;
}

// Eating: one crunchy bite.
std::vector<float> eat() {
    std::vector<float> s = grains(0.22, 35, 0.09, 0.004, 210);
    filter(s, Biquad(Biquad::Band, 1700, 1.0));
    std::vector<float> chomp = grains(0.22, 2, 0.003, 0.025, 211);
    filter(chomp, Biquad(Biquad::Low, 300, 0.7));
    mix_into(s, chomp, 0, 1.2);
    for (size_t i = 0; i < s.size(); ++i) s[i] *= static_cast<float>(std::exp(-seconds_at(i) * 12));
    normalize(s, 0.55f);
    return s;
}

// A tool wearing out: a snap and the bits clattering.
std::vector<float> tool_break() {
    std::vector<float> s = buffer(0.5);
    Rng rng(212);
    Biquad high(Biquad::High, 1500, 0.7);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(high(noise(rng)) * std::exp(-seconds_at(i) * 90));
    for (double at : {0.0, 0.09, 0.16, 0.22}) {
        const double f = rng.uniform(1600, 2400);
        mix_into(s, modes(0.2, {{f, 25, 1.0}, {f * 1.63, 35, 0.6}, {f * 2.61, 50, 0.4}}), at, 0.5 * std::exp(-at * 4));
    }
    normalize(s, 0.6f);
    return s;
}

// --- the world around -------------------------------------------------------------------------

// Rain: a patter of drops over a soft wash, faded at both ends so it can be laid over itself.
std::vector<float> rain() {
    std::vector<float> s = buffer(1.2);
    Rng rng(301);
    Biquad wash(Biquad::Low, 1200, 0.7);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(wash(noise(rng)) * 0.25);
    for (int k = 0; k < 260; ++k) {
        const double at = rng.uniform(0, 1.18), f = rng.uniform(2500, 6500);
        std::vector<float> drop = modes(0.02, {{f, 250, 1.0}});
        mix_into(s, drop, at, 0.05 + rng.random() * 0.12);
    }
    std::vector<float> hiss = grains(1.2, 400, 1.2, 0.002, 302);
    filter(hiss, Biquad(Biquad::High, 3000, 0.7));
    mix_into(s, hiss, 0, 0.25);
    fade_ends(s, 0.25, 0.35);
    normalize(s, 0.45f);
    return s;
}

// Thunder: a crack, then a long rolling rumble.
std::vector<float> thunder() {
    std::vector<float> s = buffer(5.0);
    Rng rng(303);
    Biquad low(Biquad::Low, 140, 0.8), lower(Biquad::Low, 70, 0.7), crack(Biquad::High, 1200, 0.7);
    // The rumble swells a few times as it rolls away.
    const double swells[4][2] = {{0.15, 1.0}, {0.9, 0.8}, {1.7, 0.6}, {2.8, 0.4}};
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        double env = 0;
        for (const auto& sw : swells)
            if (t > sw[0]) env += sw[1] * std::exp(-(t - sw[0]) * 1.4) * std::min(1.0, (t - sw[0]) / 0.15);
        const double w = noise(rng);
        const double rumble = (lower(low(w)) * 4 + low(w) * 0.6) * env;
        const double c = crack(w) * std::exp(-t * 9) * 0.9;
        s[i] = static_cast<float>(rumble + c);
    }
    fade_ends(s, 0.0, 0.8);
    normalize(s, 0.9f);
    return s;
}

// Flowing water: a babble of bubbles over a low gurgle.
std::vector<float> water() {
    std::vector<float> s = buffer(2.0);
    Rng rng(304);
    Biquad low(Biquad::Low, 500, 0.7);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(low(noise(rng)) * 0.5);
    bubbles(s, 70, 280, 900, 1.95, 0.35, rng);
    fade_ends(s, 0.3, 0.5);
    normalize(s, 0.45f);
    return s;
}

// Lava: a deep, slow bubbling.
std::vector<float> lava() {
    std::vector<float> s = buffer(2.0);
    Rng rng(305);
    Biquad low(Biquad::Low, 180, 0.9);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(low(noise(rng)) * 2.0);
    bubbles(s, 18, 70, 200, 1.9, 0.6, rng);
    fade_ends(s, 0.3, 0.5);
    normalize(s, 0.5f);
    return s;
}

// A bubble of lava bursting.
std::vector<float> lava_pop() {
    std::vector<float> s = buffer(0.25);
    Osc osc;
    Rng rng(306);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        s[i] = static_cast<float>(osc(140 + 260 * t / 0.25) * std::min(1.0, t / 0.005) * std::exp(-t * 18) +
                                  noise(rng) * std::exp(-t * 120) * 0.5);
    }
    normalize(s, 0.5f);
    return s;
}

// Fire: a soft roar full of crackles.
std::vector<float> fire() {
    std::vector<float> s = buffer(1.5);
    Rng rng(307);
    Biquad low(Biquad::Low, 400, 0.7), high(Biquad::High, 2000, 0.7);
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<float>(low(noise(rng)) * 0.6);
    for (int k = 0; k < 70; ++k) {
        const double at = rng.uniform(0, 1.45);
        const size_t i0 = static_cast<size_t>(at * SOUND_RATE);
        const double amp = rng.random() < 0.15 ? 1.5 : 0.4 + rng.random() * 0.4;
        for (size_t j = 0; j < 220 && i0 + j < s.size(); ++j)
            s[i0 + j] += static_cast<float>(high(noise(rng)) * amp * std::exp(-static_cast<double>(j) / 40));
    }
    fade_ends(s, 0.2, 0.4);
    normalize(s, 0.45f);
    return s;
}

// The portal: a low, wavering, otherworldly hum.
std::vector<float> portal_hum() {
    std::vector<float> s = buffer(3.0);
    Rng rng(308);
    Osc a, b, c, lfo, trem;
    Biquad whoosh(Biquad::Band, 800, 5);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        const double wob = 1 + 0.03 * lfo(0.35);
        if (i % 64 == 0) whoosh.set(700 + 500 * std::sin(t * 1.7), 5);
        const double tone = a(110 * wob) + 0.6 * b(166 * wob) + 0.4 * c(221.5 / wob);
        s[i] = static_cast<float>((tone * 0.4 * (0.75 + 0.25 * trem(6)) + whoosh(noise(rng)) * 1.5));
    }
    fade_ends(s, 0.6, 0.9);
    normalize(s, 0.45f);
    return s;
}

// Stepping into a portal: everything rises and swells.
std::vector<float> portal_trigger() {
    std::vector<float> s = buffer(3.5);
    Rng rng(309);
    Osc a, b, trem;
    Biquad band(Biquad::Band, 400, 3);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / 3.5;
        const double f = 120 * std::pow(2.0, u * 1.5);
        if (i % 64 == 0) band.set(400 + 2500 * u, 3);
        const double v = (a(f) + 0.5 * b(f * 1.5)) * 0.35 * (0.8 + 0.2 * trem(5 + 6 * u)) + band(noise(rng)) * 1.2;
        s[i] = static_cast<float>(v * u * u);
    }
    fade_ends(s, 0.0, 0.3);
    normalize(s, 0.55f);
    return s;
}

// Going through: a rush that falls away into the other side.
std::vector<float> portal_travel() {
    std::vector<float> s = buffer(4.0);
    Rng rng(310);
    Osc a, b;
    Biquad band(Biquad::Band, 3000, 2);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / 4.0;
        const double f = 500 * std::pow(0.25, u);
        if (i % 64 == 0) band.set(3000 * std::pow(0.1, u) + 200, 2);
        const double v = (a(f) + 0.4 * b(f * 1.33)) * 0.4 + band(noise(rng)) * 1.4;
        s[i] = static_cast<float>(v * std::min(1.0, t / 0.08) * std::exp(-t * 0.9));
    }
    fade_ends(s, 0.0, 0.6);
    normalize(s, 0.6f);
    return s;
}

// A menu button: a crisp little click.
std::vector<float> ui_click() {
    std::vector<float> s = buffer(0.06);
    Rng rng(311);
    Osc osc;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        s[i] = static_cast<float>(noise(rng) * std::exp(-t * 300) * 0.6 + osc(1900) * std::exp(-t * 140));
    }
    normalize(s, 0.4f);
    return s;
}

// --- animals ------------------------------------------------------------------------------------

VoiceSpec spec(double seconds, double f_start, double f_mid, double f_end, double fa, double fb, double fc, uint64_t seed) {
    VoiceSpec v;
    v.seconds = seconds;
    v.f0[0] = f_start, v.f0[1] = f_mid, v.f0[2] = f_end;
    v.formants[0] = fa, v.formants[1] = fb, v.formants[2] = fc;
    v.seed = seed;
    return v;
}

// Say a few syllables of a voice, `gap` seconds apart (each start).
std::vector<float> syllables(const VoiceSpec& v, std::initializer_list<double> starts, double total) {
    std::vector<float> s = buffer(total);
    uint64_t seed = v.seed;
    for (double at : starts) {
        VoiceSpec one = v;
        one.seed = seed++;
        mix_into(s, voice(one), at, 1.0);
    }
    return s;
}

std::vector<float> done(std::vector<float> s, float peak = 0.7f) {
    normalize(s, peak);
    return s;
}

std::vector<float> cow_say() {  // a long, low "mooo"
    VoiceSpec v = spec(1.1, 120, 132, 92, 330, 680, 2400, 401);
    v.formants_end[0] = 560, v.formants_end[1] = 900;
    v.rough = 0.05, v.breath = 0.06, v.attack = 0.1, v.release = 0.3;
    return done(voice(v));
}
std::vector<float> cow_hurt() {
    VoiceSpec v = spec(0.45, 165, 185, 120, 520, 920, 2400, 402);
    v.rough = 0.07, v.attack = 0.02, v.release = 0.15;
    return done(voice(v));
}
std::vector<float> pig_say() {  // two snorty oinks
    VoiceSpec v = spec(0.15, 150, 175, 120, 380, 1150, 2600, 403);
    v.q[0] = 4, v.q[1] = 4, v.breath = 0.35, v.rough = 0.15, v.attack = 0.01, v.release = 0.05;
    return done(syllables(v, {0.0, 0.2}, 0.4));
}
std::vector<float> pig_death() {  // a squeal
    VoiceSpec v = spec(0.65, 480, 760, 420, 900, 1650, 3000, 404);
    v.rough = 0.05, v.breath = 0.15, v.attack = 0.02, v.release = 0.2;
    return done(voice(v));
}
std::vector<float> sheep_say() {  // a trembling "baa"
    VoiceSpec v = spec(0.85, 300, 325, 260, 720, 1300, 2700, 405);
    v.formants_end[0] = 820, v.formants_end[1] = 1200;
    v.tremolo_hz = 22, v.tremolo = 0.55, v.vibrato_hz = 7, v.vibrato = 0.03, v.breath = 0.15, v.attack = 0.04, v.release = 0.25;
    return done(voice(v));
}
std::vector<float> chicken_say() {  // three quick clucks
    VoiceSpec v = spec(0.07, 700, 900, 620, 1200, 2400, 3500, 406);
    v.breath = 0.2, v.attack = 0.005, v.release = 0.03;
    return done(syllables(v, {0.0, 0.12, 0.3}, 0.42), 0.55f);
}
std::vector<float> chicken_hurt() {  // a squawk
    VoiceSpec v = spec(0.35, 1000, 1300, 780, 1400, 2600, 3800, 407);
    v.rough = 0.1, v.breath = 0.3, v.attack = 0.01, v.release = 0.1;
    return done(voice(v), 0.6f);
}
std::vector<float> wolf_bark() {  // "wuf wuf"
    VoiceSpec v = spec(0.13, 420, 490, 300, 700, 1300, 2600, 408);
    v.breath = 0.4, v.rough = 0.1, v.attack = 0.005, v.release = 0.05;
    return done(syllables(v, {0.0, 0.26}, 0.42));
}
std::vector<float> wolf_growl() {
    VoiceSpec v = spec(1.0, 85, 96, 80, 450, 900, 2200, 409);
    v.rough = 0.35, v.breath = 0.3, v.tremolo_hz = 25, v.tremolo = 0.4, v.attack = 0.1, v.release = 0.25;
    return done(voice(v));
}
std::vector<float> wolf_whine() {
    VoiceSpec v = spec(0.9, 700, 960, 600, 800, 1600, 3000, 410);
    v.q[0] = 8, v.q[1] = 10, v.breath = 0.05, v.vibrato_hz = 5, v.vibrato = 0.03, v.attack = 0.08, v.release = 0.3;
    return done(voice(v), 0.55f);
}
std::vector<float> wolf_pant() {  // breathing out, fast
    VoiceSpec v = spec(0.12, 300, 300, 300, 1200, 2200, 3200, 411);
    v.tone = 0, v.breath = 1.0, v.q[0] = 2, v.q[1] = 3, v.attack = 0.02, v.release = 0.07;
    return done(syllables(v, {0.0, 0.18, 0.36, 0.54}, 0.68), 0.45f);
}
std::vector<float> wolf_hurt() {  // a yelp
    VoiceSpec v = spec(0.25, 900, 1150, 600, 1000, 1800, 3200, 412);
    v.breath = 0.15, v.attack = 0.005, v.release = 0.08;
    return done(voice(v), 0.6f);
}

// --- monsters ---------------------------------------------------------------------------------

std::vector<float> zombie_say() {  // a slow, rotten groan
    VoiceSpec v = spec(1.3, 88, 100, 68, 440, 780, 2300, 501);
    v.formants_end[0] = 560, v.formants_end[1] = 900;
    v.rough = 0.25, v.breath = 0.2, v.tremolo_hz = 7, v.tremolo = 0.3, v.attack = 0.15, v.release = 0.4;
    return done(voice(v));
}
std::vector<float> zombie_hurt() {
    VoiceSpec v = spec(0.45, 125, 140, 90, 560, 950, 2400, 502);
    v.rough = 0.3, v.breath = 0.2, v.attack = 0.01, v.release = 0.15;
    return done(voice(v));
}

// Bones knocking together in `bursts`.
std::vector<float> rattle(double seconds, int clicks, std::initializer_list<double> bursts, uint64_t seed) {
    std::vector<float> s = buffer(seconds);
    Rng rng(seed);
    for (double at : bursts)
        for (int k = 0; k < clicks; ++k) {
            const double t = at + rng.uniform(0, 0.12);
            const double f = 1100 * rng.uniform(0.85, 1.15);
            mix_into(s, modes(0.04, {{f, 130, 1.0}, {f * 2.1, 170, 0.6}, {f * 3.4, 220, 0.3}}), t, 0.4 + 0.6 * rng.random());
        }
    return s;
}
std::vector<float> skeleton_say() { return done(rattle(0.6, 9, {0.0, 0.2, 0.38}, 503), 0.55f); }
std::vector<float> skeleton_hurt() {
    std::vector<float> s = rattle(0.45, 16, {0.0, 0.12}, 504);
    mix_into(s, modes(0.2, {{420, 40, 1.0}, {980, 60, 0.5}}), 0, 0.8);
    return done(s, 0.6f);
}
std::vector<float> spider_say() {  // a hissing chitter
    std::vector<float> s = buffer(0.7);
    Rng rng(505);
    Biquad band(Biquad::Band, 3500, 1.5);
    Osc chitter;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        const double c = std::pow(std::abs(chitter(14)), 4);
        s[i] = static_cast<float>(band(noise(rng)) * (0.25 + c) * std::sin(PI * t / 0.7));
    }
    return done(s, 0.5f);
}
std::vector<float> spider_death() {  // a screech
    VoiceSpec v = spec(0.9, 600, 720, 340, 1500, 2800, 4200, 506);
    v.rough = 0.3, v.breath = 0.6, v.attack = 0.02, v.release = 0.3;
    return done(voice(v), 0.6f);
}
std::vector<float> creeper_hurt() {  // a crackling hiss
    std::vector<float> s = buffer(0.45);
    Rng rng(507);
    Biquad high(Biquad::High, 2500, 0.7);
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i);
        double v = high(noise(rng));
        if (rng.below(300) == 0) v += noise(rng) * 4;
        s[i] = static_cast<float>(v * std::min(1.0, t / 0.01) * std::exp(-t * 5));
    }
    return done(s, 0.5f);
}
std::vector<float> slime() {  // a wet squelch
    std::vector<float> s = buffer(0.3);
    Rng rng(508);
    Biquad band(Biquad::Band, 250, 4);
    Osc blip;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / 0.3;
        if (i % 32 == 0) band.set(250 + 500 * std::sin(PI * u), 4);
        s[i] = static_cast<float>((band(noise(rng)) * 2.5 + blip(130 - 50 * u) * 0.5) * std::min(1.0, t / 0.01) * std::exp(-t * 9));
    }
    return done(s, 0.6f);
}
std::vector<float> ghast_moan() {  // a high, sorrowful wail that echoes off the Nether
    VoiceSpec v = spec(2.2, 420, 560, 380, 500, 900, 2600, 509);
    v.formants_end[0] = 350, v.formants_end[1] = 780;
    v.vibrato_hz = 5, v.vibrato = 0.04, v.breath = 0.35, v.attack = 0.3, v.release = 0.6;
    std::vector<float> s = buffer(2.8);
    mix_into(s, voice(v), 0, 1.0);
    echo(s, 0.18, 0.3, 2);
    return done(s, 0.6f);
}
std::vector<float> ghast_scream() {
    VoiceSpec v = spec(0.8, 780, 1000, 680, 900, 1700, 3200, 510);
    v.rough = 0.15, v.breath = 0.4, v.attack = 0.02, v.release = 0.25;
    std::vector<float> s = buffer(1.2);
    mix_into(s, voice(v), 0, 1.0);
    echo(s, 0.16, 0.3, 2);
    return done(s, 0.65f);
}
std::vector<float> ghast_charge() {  // drawing breath, rising
    VoiceSpec v = spec(0.6, 300, 600, 1100, 800, 1500, 3000, 511);
    v.breath = 0.35, v.attack = 0.1, v.release = 0.1;
    return done(voice(v), 0.55f);
}
std::vector<float> ghast_fireball() {  // the fireball bursting out: a whoosh and a thump
    std::vector<float> s = buffer(0.7);
    Rng rng(512);
    Biquad low(Biquad::Low, 300, 0.8);
    Osc thump;
    for (size_t i = 0; i < s.size(); ++i) {
        const double t = seconds_at(i), u = t / 0.7;
        if (i % 64 == 0) low.set(300 + 2200 * std::sin(PI * std::min(1.0, u * 1.4)), 0.8);
        s[i] = static_cast<float>(low(noise(rng)) * std::sin(PI * u) + thump(60 - 20 * u) * std::exp(-t * 10) * 0.8);
    }
    return done(s, 0.65f);
}
std::vector<float> pigman_say() {  // a grumbling, piggish grunt
    VoiceSpec v = spec(0.24, 105, 130, 88, 450, 1000, 2500, 513);
    v.rough = 0.3, v.breath = 0.3, v.attack = 0.02, v.release = 0.08;
    return done(syllables(v, {0.0, 0.3}, 0.6));
}
std::vector<float> pigman_angry() {
    VoiceSpec v = spec(0.8, 200, 265, 170, 600, 1200, 2600, 514);
    v.rough = 0.4, v.breath = 0.3, v.tremolo_hz = 12, v.tremolo = 0.3, v.attack = 0.03, v.release = 0.25;
    return done(voice(v));
}
std::vector<float> pigman_hurt() {
    VoiceSpec v = spec(0.35, 180, 205, 120, 550, 1050, 2500, 515);
    v.rough = 0.3, v.breath = 0.25, v.attack = 0.01, v.release = 0.1;
    return done(voice(v));
}

}  // namespace

std::vector<float> make_effect(Sound sound) {
    switch (sound) {
        case Sound::StepStone: return step_stone();
        case Sound::StepWood: return step_wood();
        case Sound::StepGravel: return step_gravel();
        case Sound::StepGrass: return step_grass();
        case Sound::StepSand: return step_sand();
        case Sound::StepCloth: return step_cloth();
        case Sound::GlassBreak: return glass_break();
        case Sound::Hurt: return hurt();
        case Sound::Pop: return pop();
        case Sound::Bow: return bow();
        case Sound::ArrowHit: return arrow_hit();
        case Sound::DoorOpen: return door_open();
        case Sound::DoorClose: return door_close();
        case Sound::ChestOpen: return chest_open();
        case Sound::ChestClose: return chest_close();
        case Sound::Splash: return splash();
        case Sound::Fizz: return fizz();
        case Sound::Ignite: return ignite();
        case Sound::Eat: return eat();
        case Sound::ToolBreak: return tool_break();
        case Sound::Rain: return rain();
        case Sound::Thunder: return thunder();
        case Sound::Water: return water();
        case Sound::Lava: return lava();
        case Sound::LavaPop: return lava_pop();
        case Sound::Fire: return fire();
        case Sound::PortalHum: return portal_hum();
        case Sound::PortalTrigger: return portal_trigger();
        case Sound::PortalTravel: return portal_travel();
        case Sound::CowSay: return cow_say();
        case Sound::CowHurt: return cow_hurt();
        case Sound::PigSay: return pig_say();
        case Sound::PigDeath: return pig_death();
        case Sound::SheepSay: return sheep_say();
        case Sound::ChickenSay: return chicken_say();
        case Sound::ChickenHurt: return chicken_hurt();
        case Sound::WolfBark: return wolf_bark();
        case Sound::WolfGrowl: return wolf_growl();
        case Sound::WolfWhine: return wolf_whine();
        case Sound::WolfPant: return wolf_pant();
        case Sound::WolfHurt: return wolf_hurt();
        case Sound::ZombieSay: return zombie_say();
        case Sound::ZombieHurt: return zombie_hurt();
        case Sound::SkeletonSay: return skeleton_say();
        case Sound::SkeletonHurt: return skeleton_hurt();
        case Sound::SpiderSay: return spider_say();
        case Sound::SpiderDeath: return spider_death();
        case Sound::CreeperHurt: return creeper_hurt();
        case Sound::Slime: return slime();
        case Sound::GhastMoan: return ghast_moan();
        case Sound::GhastScream: return ghast_scream();
        case Sound::GhastCharge: return ghast_charge();
        case Sound::GhastFireball: return ghast_fireball();
        case Sound::PigmanSay: return pigman_say();
        case Sound::PigmanAngry: return pigman_angry();
        case Sound::PigmanHurt: return pigman_hurt();
        case Sound::UiClick: return ui_click();
        default: return {};
    }
}

}  // namespace synth
