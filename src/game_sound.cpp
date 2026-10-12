// The game's sounds (M16): footsteps, digging, mobs speaking, rain, and the noises of water, lava,
// fire and portals near the player. Which sound and how loud comes from soundboard.h; the sounds
// themselves are made in sound_effects.cpp. Other sounds start where their action happens.
#include <cmath>

#include "game.h"

namespace {
int ifloor(double v) { return static_cast<int>(std::floor(v)); }
}  // namespace

void Game::play_sound(Sound sound, glm::dvec3 pos, float pitch, float volume) {
    SoundEvent e{sound, pos, pitch};
    e.volume = volume;
    sounds.push_back(e);
    ++sounds_started[static_cast<int>(sound)];
}

void Game::play_ui_sound(Sound sound, float pitch, float volume) {
    SoundEvent e{sound, glm::dvec3(player.x, player.y, player.z), pitch};
    e.volume = volume;
    e.channel = Channel::Ui;
    sounds.push_back(e);
    ++sounds_started[static_cast<int>(sound)];
}

void Game::block_sound_at(glm::ivec3 pos, BlockId block, float volume, float pitch, bool breaking) {
    if (block == AIR || is_fluid(block)) return;
    const Sound s = breaking ? break_sound(block) : block_sound(block);
    play_sound(s, glm::dvec3(pos) + 0.5, pitch * block_pitch(block), volume);
}

void Game::step_sounds(double distance) {
    const Body& p = player;
    // A splash going into water, as loud as the fall was fast (Beta).
    if (p.in_water && !was_in_water_ && !flying) {
        const double v = std::sqrt(p.vx * p.vx * 0.2 + p.vy * p.vy + p.vz * p.vz * 0.2) / 20 * 0.2;
        if (v > 0.02) play_sound(Sound::Splash, {p.x, p.y, p.z}, vary_pitch(sound_rng_, 0.4f), static_cast<float>(std::min(1.0, v * 4)));
    }
    was_in_water_ = p.in_water;
    // Footsteps: on the ground, not sneaking (Beta's sneaking is silent), flying or swimming.
    if (!p.on_ground || flying || sneaking || p.in_liquid() || distance <= 0) return;
    if (!footsteps_.walk(distance)) return;
    glm::ivec3 under(ifloor(p.x), ifloor(p.y - 0.2), ifloor(p.z));
    // Snow lying on the ground is what the foot touches.
    if (world.get_block(under.x, under.y + 1, under.z) == SNOW_LAYER) ++under.y;
    const BlockId b = world.get_block(under.x, under.y, under.z);
    if (b == AIR) return;
    play_sound(block_sound(b), {p.x, p.y, p.z}, block_pitch(b), STEP_VOLUME);
}

void Game::door_sound(glm::ivec3 pos, bool opened) {
    play_sound(opened ? Sound::DoorOpen : Sound::DoorClose, glm::dvec3(pos) + 0.5, static_cast<float>(sound_rng_.random() * 0.1 + 0.9));
}

void Game::pickup_sound() {
    play_sound(Sound::Pop, {player.x, player.y + 1, player.z}, vary_pitch(sound_rng_, 0.7f) * 2, 0.2f);
}

void Game::voice_tick(MobVoice& v, const MobSounds& s, const Body& b, int health) {
    const glm::dvec3 at(b.x, b.y + b.height * 0.8, b.z);
    const float pitch = s.pitch * vary_pitch(sound_rng_);
    if (v.health < 0) v.health = health;  // just appeared: nothing happened to it yet
    if (health < v.health) {
        if (health <= 0 && s.death) play_sound(*s.death, at, pitch * s.death_pitch, s.volume);
        else if (health > 0 && s.hurt) play_sound(*s.hurt, at, pitch, s.volume);
    }
    v.health = health;
    if (health <= 0 || !s.say) return;
    // Only mobs in earshot count down to speaking, so a crowd far away doesn't fill the voices.
    const double reach = sound_range(*s.say) * std::max(1.0f, s.volume);
    if (std::abs(b.x - player.x) > reach || std::abs(b.z - player.z) > reach) return;
    if (mob_talks(v.talk, sound_rng_)) play_sound(*s.say, at, pitch, s.volume);
}

void Game::tick_sounds() {
    for (const auto& a : animals) {
        const MobSounds s = animal_sounds(a->kind, a->angry, a->tamed, a->health, sound_rng_.below(3));
        voice_tick(a->voice, s, a->body, a->health);
    }
    for (const auto& m : monsters) {
        const MobSounds s = monster_sounds(m->kind, m->size, m->anger > 0);
        voice_tick(m->voice, s, m->body, m->health);
        const glm::dvec3 at(m->body.x, m->body.y + m->body.height * 0.5, m->body.z);
        // A ghast draws breath before it shoots.
        if (m->charging() && !m->voice.charging) play_sound(Sound::GhastCharge, at, vary_pitch(sound_rng_), s.volume);
        m->voice.charging = m->charging();
        // A slime squelches each time it lands from a jump.
        if (m->kind == MobKind::Slime && m->alive() && m->body.on_ground && !m->voice.on_ground)
            play_sound(Sound::Slime, at, s.pitch * vary_pitch(sound_rng_) / 0.8f, s.volume);
        m->voice.on_ground = m->body.on_ground;
    }

    // Rain: now and then a patter from a spot near the player where rain is falling, quieter and
    // lower when it falls on a roof above them (Beta).
    const double rain = world.nether() ? 0.0 : weather.rain_strength();
    if (rain > 0 && sound_rng_.random() < rain * rain) {
        const int x = ifloor(player.x) + sound_rng_.randint(-10, 10), z = ifloor(player.z) + sound_rng_.randint(-10, 10);
        const auto top = world.rain_height(x, z);
        if (top && rain_at({x, *top, z}) && *top <= ifloor(player.y) + 10 && sound_rng_.below(3) < rain_sound_counter_++) {
            rain_sound_counter_ = 0;
            const bool roof = *top > player.y + 1 && std::hypot(x + 0.5 - player.x, z + 0.5 - player.z) < 2;
            play_sound(Sound::Rain, {x + 0.5, *top + 0.1, z + 0.5}, roof ? 0.5f : vary_pitch(sound_rng_, 0.15f), roof ? 0.1f : 0.2f);
        }
    }

    // Beta's random display ticks: 1000 random cells within 16 blocks, some of which make a noise.
    const int px = ifloor(player.x), py = ifloor(player.y), pz = ifloor(player.z);
    for (int i = 0; i < 1000; ++i) {
        const int x = px + sound_rng_.randint(-16, 15), y = py + sound_rng_.randint(-16, 15), z = pz + sound_rng_.randint(-16, 15);
        if (y < 0 || y >= HEIGHT) continue;
        const BlockId b = world.get_block(x, y, z);
        const glm::dvec3 c(x + 0.5, y + 0.5, z + 0.5);
        if (b == WATER) {
            const int level = world.get_data(x, y, z);
            if (level > 0 && level < 8 && sound_rng_.below(64) == 0)
                play_sound(Sound::Water, c, static_cast<float>(sound_rng_.random() + 0.5),
                           static_cast<float>(sound_rng_.random() * 0.25 + 0.75));
        } else if (b == LAVA) {
            if (world.get_block(x, y + 1, z) != AIR) continue;
            if (sound_rng_.below(100) == 0)
                play_sound(Sound::LavaPop, {c.x, y + 1.0, c.z}, static_cast<float>(0.9 + sound_rng_.random() * 0.15),
                           static_cast<float>(0.2 + sound_rng_.random() * 0.2));
            if (sound_rng_.below(200) == 0)
                play_sound(Sound::Lava, c, static_cast<float>(0.9 + sound_rng_.random() * 0.15), static_cast<float>(0.2 + sound_rng_.random() * 0.2));
        } else if (b == FIRE) {
            if (sound_rng_.below(24) == 0)
                play_sound(Sound::Fire, c, static_cast<float>(sound_rng_.random() * 0.7 + 0.3), static_cast<float>(1 + sound_rng_.random()));
        } else if (b == PORTAL) {
            if (sound_rng_.below(100) == 0) play_sound(Sound::PortalHum, c, static_cast<float>(sound_rng_.random() * 0.4 + 0.8), 0.5f);
        }
    }
}
