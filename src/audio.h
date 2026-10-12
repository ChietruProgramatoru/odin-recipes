// Playing sound effects and music on the computer's audio device (with miniaudio). Without a device
// the game is simply silent. The music takes a few seconds to make, so it is made in the background
// after the effects; a piece asked for before it is ready is skipped.
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
    // Start a sound, heard from a listener at `listener` facing `yaw_degrees` (or stop one, see SoundEvent).
    void play(const SoundEvent& event, glm::dvec3 listener, double yaw_degrees);
    // Silence the world's sounds (leaving a world: a jukebox's disc doesn't follow you to the menu).
    // The background music plays on.
    void stop_all();
    // The listener moved: sounds still playing (a disc goes on for a while) are heard from there.
    void move_listener(glm::dvec3 listener, double yaw_degrees);
    // How loud the Music channel and everything else are (0..1, the Options screen's sliders).
    void set_volumes(float sound, float music);
    // Music, or a jukebox's disc, is playing (the next piece of music waits for it).
    bool music_busy() const;
    void stop_music();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
