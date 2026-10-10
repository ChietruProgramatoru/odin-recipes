// Sound effects: made by the code (no sound files), and how loud each ear hears one.
// Playing them is audio.cpp's job; this part needs no audio device, so the tests can use it.
#pragma once
#include <vector>

#include <glm/vec3.hpp>

enum class Sound { Fuse, Explosion };
constexpr int SOUND_COUNT = 2;
constexpr int SOUND_RATE = 44100;  // samples per second of the made sounds

// A sound the game wants played at a spot. `pitch` 1 plays it as made; 1.2 plays it faster and higher.
struct SoundEvent {
    Sound sound;
    glm::dvec3 pos;
    float pitch = 1.0f;
};

// The sound's samples (mono, -1..1, SOUND_RATE a second). The same every time.
//   Fuse: a creeper's or lit TNT's hiss, 1.5 seconds of noise that swells a little.
//   Explosion: a low boom with a rumble that dies away over 2 seconds.
std::vector<float> make_sound(Sound sound);

// How far away a sound can be heard (Beta: 16 blocks times its volume, explosions are 4 times louder).
double sound_range(Sound sound);

struct StereoGain {
    float left = 0, right = 0;
};
// How loud each ear hears a sound at `source` for a listener at `listener` facing `yaw_degrees`
// (0 faces +z, 90 faces +x). It fades to nothing at the sound's range, and is panned towards the
// side it comes from (never fully to one ear).
StereoGain sound_gain(Sound sound, glm::dvec3 listener, double yaw_degrees, glm::dvec3 source);
