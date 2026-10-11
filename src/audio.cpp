#include "audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <vector>

// Only the low-level device part of miniaudio is needed: the game makes and mixes its own sounds.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace {
constexpr size_t MAX_VOICES = 24;  // sounds playing at once; the oldest makes room for a new one
}

struct Audio::Impl {
    ma_device device{};
    bool open = false;
    std::array<std::vector<float>, SOUND_COUNT> sounds;

    struct Voice {
        const std::vector<float>* samples;
        double pos, step;
        float left, right;
        Sound sound;
        glm::dvec3 source;
        int key;
    };
    std::mutex mutex;
    std::vector<Voice> voices;

    // Called on miniaudio's thread: mix every voice into the stereo output.
    void mix(float* out, ma_uint32 frames) {
        std::fill(out, out + frames * 2, 0.0f);
        std::lock_guard lock(mutex);
        for (Voice& v : voices) {
            const auto& s = *v.samples;
            for (ma_uint32 f = 0; f < frames; ++f) {
                const size_t i = static_cast<size_t>(v.pos);
                if (i + 1 >= s.size()) {
                    v.pos = static_cast<double>(s.size());
                    break;
                }
                const double frac = v.pos - static_cast<double>(i);
                const float x = static_cast<float>(s[i] + (s[i + 1] - s[i]) * frac);
                out[f * 2] += x * v.left;
                out[f * 2 + 1] += x * v.right;
                v.pos += v.step;
            }
        }
        std::erase_if(voices, [](const Voice& v) { return v.pos + 1 >= static_cast<double>(v.samples->size()); });
        for (ma_uint32 i = 0; i < frames * 2; ++i) out[i] = std::clamp(out[i], -1.0f, 1.0f);
    }

    static void callback(ma_device* device, void* output, const void*, ma_uint32 frames) {
        static_cast<Impl*>(device->pUserData)->mix(static_cast<float*>(output), frames);
    }
};

Audio::Audio(bool enabled) : impl_(std::make_unique<Impl>()) {
    if (!enabled) return;
    for (int i = 0; i < SOUND_COUNT; ++i) impl_->sounds[i] = make_sound(static_cast<Sound>(i));
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = SOUND_RATE;
    config.dataCallback = &Impl::callback;
    config.pUserData = impl_.get();
    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) return;
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        return;
    }
    impl_->open = true;
}

Audio::~Audio() {
    if (impl_->open) ma_device_uninit(&impl_->device);
}

bool Audio::ok() const { return impl_->open; }

void Audio::play(const SoundEvent& event, glm::dvec3 listener, double yaw_degrees) {
    if (!impl_->open) return;
    std::lock_guard lock(impl_->mutex);
    auto& voices = impl_->voices;
    if (event.key) std::erase_if(voices, [&](const Impl::Voice& v) { return v.key == event.key; });
    if (event.stop) return;
    const StereoGain g = sound_gain(event.sound, listener, yaw_degrees, event.pos);
    if (g.left <= 0 && g.right <= 0 && !event.key) return;  // a disc keeps playing out of earshot
    if (voices.size() >= MAX_VOICES) voices.erase(voices.begin());
    voices.push_back({&impl_->sounds[static_cast<int>(event.sound)], 0.0, std::max(0.1f, event.pitch) * 1.0, g.left, g.right,
                      event.sound, event.pos, event.key});
}

void Audio::stop_all() {
    if (!impl_->open) return;
    std::lock_guard lock(impl_->mutex);
    impl_->voices.clear();
}

void Audio::move_listener(glm::dvec3 listener, double yaw_degrees) {
    if (!impl_->open) return;
    std::lock_guard lock(impl_->mutex);
    for (auto& v : impl_->voices) {
        const StereoGain g = sound_gain(v.sound, listener, yaw_degrees, v.source);
        v.left = g.left;
        v.right = g.right;
    }
}
