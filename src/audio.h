// Playing sound effects on the computer's audio device (with miniaudio). Without a device the
// game is simply silent.
#pragma once
#include <memory>

#include <glm/vec3.hpp>

#include "sound.h"

class Audio {
public:
    // `enabled` false never opens a device (the autotest runs silent).
    explicit Audio(bool enabled);
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool ok() const;  // a device is open and playing
    // Start a sound, heard from a listener at `listener` facing `yaw_degrees`.
    void play(const SoundEvent& event, glm::dvec3 listener, double yaw_degrees);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
