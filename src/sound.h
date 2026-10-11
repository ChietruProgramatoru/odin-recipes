// Sound effects: made by the code (no sound files), and how loud each ear hears one.
// Playing them is audio.cpp's job; this part needs no audio device, so the tests can use it.
#pragma once
#include <vector>

#include <glm/vec3.hpp>

// Note block instruments (M11) come in the order of NoteInstrument (building's note blocks), then
// the two music discs.
enum class Sound { Fuse, Explosion, NoteHarp, NoteBassDrum, NoteSnare, NoteClick, NoteBass, Record13, RecordCat, Click, PistonOut, PistonIn };
constexpr int SOUND_COUNT = 12;
constexpr int SOUND_RATE = 44100;  // samples per second of the made sounds

// A sound the game wants played at a spot. `pitch` 1 plays it as made; 1.2 plays it faster and higher.
// A sound with a `key` (not 0) can be stopped again (a jukebox's disc): starting another with the
// same key replaces it, and `stop` ends it.
struct SoundEvent {
    Sound sound;
    glm::dvec3 pos;
    float pitch = 1.0f;
    int key = 0;
    bool stop = false;
};

// The sound's samples (mono, -1..1, SOUND_RATE a second). The same every time.
//   Fuse: a creeper's or lit TNT's hiss, 1.5 seconds of noise that swells a little.
//   Explosion: a low boom with a rumble that dies away over 2 seconds.
//   Note*: one note of a note block at F#4 (the bass an octave lower), about a second long.
//   Record13, RecordCat: the music on the discs, this game's own tunes (about 50 seconds each):
//   "13" is slow and eerie, "cat" a bright little song with bass and drums.
//   Click: a lever, button or pressure plate moving (and a dispenser with nothing in it).
//   PistonOut, PistonIn: a piston pushing out (a thump with a hiss of air) and pulling back.
std::vector<float> make_sound(Sound sound);

// A note block's pitch (0-24) as a playback speed: 0.5 (F#3) to 2 (F#5), like Beta.
float note_pitch(int note);

// How far away a sound can be heard (Beta: 16 blocks times its volume, explosions are 4 times louder).
double sound_range(Sound sound);

struct StereoGain {
    float left = 0, right = 0;
};
// How loud each ear hears a sound at `source` for a listener at `listener` facing `yaw_degrees`
// (0 faces +z, 90 faces +x). It fades to nothing at the sound's range, and is panned towards the
// side it comes from (never fully to one ear).
StereoGain sound_gain(Sound sound, glm::dvec3 listener, double yaw_degrees, glm::dvec3 source);
