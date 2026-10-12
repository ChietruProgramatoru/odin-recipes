#include "soundboard.h"

#include <cmath>

Sound block_sound(BlockId block) {
    switch (block) {
        case PLANKS: case LOG: case BIRCH_LOG: case SPRUCE_LOG: case CRAFTING_TABLE: case CHEST: case BOOKSHELF:
        case NOTE_BLOCK: case JUKEBOX: case WOOD_STAIRS: case FENCE: case LADDER: case WOOD_DOOR_BLOCK: case TRAPDOOR:
        case SIGN_POST: case WALL_SIGN: case PAINTING_BLOCK: case WOOD_PLATE_BLOCK: case TORCH: case TORCH_WALL_WEST:
        case TORCH_WALL_EAST: case TORCH_WALL_NORTH: case TORCH_WALL_SOUTH: case REDSTONE_TORCH_ON: case REDSTONE_TORCH_OFF:
        case LEVER_BLOCK: case REPEATER_OFF: case REPEATER_ON: case FIRE:
        case BED_FOOT_SOUTH: case BED_FOOT_WEST: case BED_FOOT_NORTH: case BED_FOOT_EAST:
        case BED_HEAD_SOUTH: case BED_HEAD_WEST: case BED_HEAD_NORTH: case BED_HEAD_EAST:
            return Sound::StepWood;
        case DIRT: case GRAVEL: case FARMLAND: case CLAY_BLOCK:
            return Sound::StepGravel;
        case GRASS: case LEAVES: case BIRCH_LEAVES: case SPRUCE_LEAVES: case TALL_GRASS: case FERN: case YELLOW_FLOWER:
        case RED_ROSE: case BROWN_MUSHROOM: case RED_MUSHROOM: case WHEAT_CROP: case SUGAR_CANE_PLANT: case SAPLING:
        case SPRUCE_SAPLING: case BIRCH_SAPLING: case TNT:
            return Sound::StepGrass;
        case SAND: case SOUL_SAND:
            return Sound::StepSand;
        case WOOL: case SNOW_BLOCK: case SNOW_LAYER: case CACTUS: case CAKE_BLOCK: case COBWEB:
            return Sound::StepCloth;
        default:
            return Sound::StepStone;
    }
}

Sound break_sound(BlockId block) {
    switch (block) {
        case GLASS: case ICE: case GLOWSTONE: case PORTAL: return Sound::GlassBreak;
        default: return block_sound(block);
    }
}

float block_pitch(BlockId block) {
    switch (block) {
        case IRON_BLOCK: case GOLD_BLOCK: case DIAMOND_BLOCK: case IRON_DOOR_BLOCK: case RAIL: case POWERED_RAIL:
        case DETECTOR_RAIL:
            return 1.5f;
        default: return 1.0f;
    }
}

bool Footsteps::walk(double distance) {
    walked_ += distance / STEP_DISTANCE;
    if (walked_ <= next_) return false;
    next_ = std::floor(walked_) + 1;
    return true;
}

MobSounds animal_sounds(AnimalKind kind, bool angry, bool tamed, int health, int roll) {
    MobSounds m;
    switch (kind) {
        case AnimalKind::Cow:
            m.say = Sound::CowSay, m.hurt = Sound::CowHurt, m.death = Sound::CowHurt;
            m.volume = 0.4f, m.death_pitch = 0.85f;
            break;
        case AnimalKind::Pig:
            m.say = Sound::PigSay, m.hurt = Sound::PigSay, m.death = Sound::PigDeath;
            break;
        case AnimalKind::Sheep:
            m.say = m.hurt = m.death = Sound::SheepSay;
            break;
        case AnimalKind::Chicken:
            m.say = Sound::ChickenSay, m.hurt = m.death = Sound::ChickenHurt;
            break;
        case AnimalKind::Squid:
            break;  // squid make no sound (Beta)
        case AnimalKind::Wolf:
            if (angry) m.say = Sound::WolfGrowl;
            else if (roll == 0) m.say = tamed && health < 10 ? Sound::WolfWhine : Sound::WolfPant;
            else m.say = Sound::WolfBark;
            m.hurt = m.death = Sound::WolfHurt;
            m.volume = 0.4f, m.death_pitch = 0.8f;
            break;
    }
    return m;
}

MobSounds monster_sounds(MobKind kind, int size, bool angry) {
    MobSounds m;
    m.death_pitch = 0.8f;
    switch (kind) {
        case MobKind::Zombie: m.say = Sound::ZombieSay, m.hurt = m.death = Sound::ZombieHurt; break;
        case MobKind::Skeleton: m.say = Sound::SkeletonSay, m.hurt = m.death = Sound::SkeletonHurt; break;
        case MobKind::Spider:
            m.say = m.hurt = Sound::SpiderSay, m.death = Sound::SpiderDeath;
            m.death_pitch = 1.0f;
            break;
        case MobKind::Creeper: m.hurt = m.death = Sound::CreeperHurt; break;  // creepers only hiss when lit
        case MobKind::Slime:
            // Big slimes are louder and lower (the jump's squelch is the same sound, see the game).
            m.hurt = m.death = Sound::Slime;
            m.volume = 0.4f * static_cast<float>(size);
            m.pitch = size >= 4 ? 0.7f : size == 2 ? 1.0f : 1.4f;
            m.death_pitch = 1.0f;
            break;
        case MobKind::Ghast:
            m.say = Sound::GhastMoan, m.hurt = m.death = Sound::GhastScream;
            m.volume = 10.0f, m.death_pitch = 0.7f;
            break;
        case MobKind::PigZombie:
            m.say = angry ? Sound::PigmanAngry : Sound::PigmanSay, m.hurt = m.death = Sound::PigmanHurt;
            break;
    }
    return m;
}

bool mob_talks(int& talk, Rng& rng) {
    if (rng.below(1000) >= talk++) return false;
    talk = -80;
    return true;
}

float vary_pitch(Rng& rng, float spread) {
    return static_cast<float>((rng.random() - rng.random()) * spread + 1.0);
}

MusicTimer::MusicTimer(uint64_t seed) : rng_(seed), wait_(rng_.uniform(FIRST_MIN, FIRST_MAX)) {}

void MusicTimer::rest() { wait_ = rng_.uniform(QUIET_MIN, QUIET_MAX); }

std::optional<Sound> MusicTimer::update(double dt, bool busy) {
    if (busy) return std::nullopt;
    wait_ -= dt;
    if (wait_ > 0) return std::nullopt;
    int piece = rng_.below(MUSIC_COUNT);
    if (piece == last_) piece = (piece + 1 + rng_.below(MUSIC_COUNT - 1)) % MUSIC_COUNT;
    last_ = piece;
    rest();
    return static_cast<Sound>(static_cast<int>(FIRST_MUSIC) + piece);
}
