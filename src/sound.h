// Sound effects: made by the code (no sound files), and how loud each ear hears one.
// Playing them is audio.cpp's job; this part needs no audio device, so the tests can use it.
#pragma once
#include <vector>

#include <glm/vec3.hpp>

// Note block instruments (M11) come in the order of NoteInstrument (building's note blocks), then
// the two music discs. Everything after PistonIn came with M16 (sound and music); the order is only
// used inside the program (nothing is saved), but sounds_started in the autotest counts by it.
enum class Sound {
    Fuse, Explosion, NoteHarp, NoteBassDrum, NoteSnare, NoteClick, NoteBass, Record13, RecordCat, Click, PistonOut, PistonIn,
    // Blocks: one sound per material (Beta's step sounds), used for walking, digging, breaking and
    // placing; glass shatters when it breaks.
    StepStone, StepWood, StepGravel, StepGrass, StepSand, StepCloth, GlassBreak,
    // The player and the things they use.
    Hurt, Pop, Bow, ArrowHit, DoorOpen, DoorClose, ChestOpen, ChestClose, Splash, Fizz, Ignite, Eat, ToolBreak,
    // The world around: weather, fluids, fire and portals.
    Rain, Thunder, Water, Lava, LavaPop, Fire, PortalHum, PortalTrigger, PortalTravel,
    // Animals.
    CowSay, CowHurt, PigSay, PigDeath, SheepSay, ChickenSay, ChickenHurt, WolfBark, WolfGrowl, WolfWhine, WolfPant, WolfHurt,
    // Monsters.
    ZombieSay, ZombieHurt, SkeletonSay, SkeletonHurt, SpiderSay, SpiderDeath, CreeperHurt, Slime, GhastMoan, GhastScream,
    GhastCharge, GhastFireball, PigmanSay, PigmanAngry, PigmanHurt,
    // The menus' buttons.
    UiClick,
    // Background music: calm pieces, this game's own, that play now and then (see MusicTimer).
    MusicDawn, MusicMeadow, MusicLanterns, MusicDrift,
    Count
};
constexpr int SOUND_COUNT = static_cast<int>(Sound::Count);
constexpr int SOUND_RATE = 44100;  // samples per second of the made sounds
constexpr Sound FIRST_MUSIC = Sound::MusicDawn;
constexpr int MUSIC_COUNT = 4;
bool is_music(Sound sound);

// Who hears a sound. World sounds come from a spot and fade with distance; Ui and Music sounds are
// heard the same wherever the listener is. The Options screen sets Music's volume and the rest's.
enum class Channel { World, Ui, Music };

// A sound the game wants played at a spot. `pitch` 1 plays it as made; 1.2 plays it faster and higher.
// `volume` above 1 also carries further (Beta: the range is 16 blocks times the volume).
// A sound with a `key` (not 0) can be stopped again (a jukebox's disc): starting another with the
// same key replaces it, and `stop` ends it.
struct SoundEvent {
    Sound sound;
    glm::dvec3 pos;
    float pitch = 1.0f;
    int key = 0;
    bool stop = false;
    float volume = 1.0f;
    Channel channel = Channel::World;
};

// What a mob's voice remembers between ticks (each animal and monster has one): Beta's "living
// sound" countdown, and the health, charge and footing it had, to hear it get hurt, die, charge a
// fireball or land (slimes).
struct MobVoice {
    int talk = 0;
    int health = -1;
    bool charging = false;
    bool on_ground = true;
};

// The sound's samples (mono, -1..1, SOUND_RATE a second). The same every time.
//   Fuse: a creeper's or lit TNT's hiss, 1.5 seconds of noise that swells a little.
//   Explosion: a low boom with a rumble that dies away over 2 seconds.
//   Note*: one note of a note block at F#4 (the bass an octave lower), about a second long.
//   Record13, RecordCat: the music on the discs, this game's own tunes (about 50 seconds each):
//   "13" is slow and eerie, "cat" a bright little song with bass and drums.
//   Click: a lever, button or pressure plate moving (and a dispenser with nothing in it).
//   PistonOut, PistonIn: a piston pushing out (a thump with a hiss of air) and pulling back.
//   The M16 sounds are described where they are made (sound_effects.cpp and music.cpp).
std::vector<float> make_sound(Sound sound);

// A note block's pitch (0-24) as a playback speed: 0.5 (F#3) to 2 (F#5), like Beta.
float note_pitch(int note);

// How far away a sound at volume 1 can be heard (Beta: 16 blocks times its volume, explosions are 4
// times louder, thunder is heard everywhere).
double sound_range(Sound sound);

struct StereoGain {
    float left = 0, right = 0;
};
// How loud each ear hears a sound at `source` for a listener at `listener` facing `yaw_degrees`
// (0 faces +z, 90 faces +x). It fades to nothing at the sound's range, and is panned towards the
// side it comes from (never fully to one ear). `volume` scales it and stretches the range (Beta).
StereoGain sound_gain(Sound sound, glm::dvec3 listener, double yaw_degrees, glm::dvec3 source, float volume = 1.0f);
