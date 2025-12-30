#pragma once

#include "Config.h"
#include <raylib.h>
#include <string>
#include <random>

class Audio
{
public:
    Audio();
    ~Audio();

    bool init();
    void shutdown();
    void update (float dt);

    // Play sounds with position-based panning (screenX from 0 to screenWidth)
    void playCannon (float screenX, float screenWidth);
    void playSplash (float screenX, float screenWidth);
    void playExplosion (float screenX, float screenWidth);
    void playCollision (float screenX, float screenWidth);

    // Engine is played continuously with volume based on average throttle
    void setEngineVolume (float volume); // 0.0 to 1.0

    // Master volume control (0-10 scale, stored as 0.0-1.0)
    void setMasterVolume (int level); // 0-10
    int getMasterVolumeLevel() const { return masterVolumeLevel; }
    float getMasterVolume() const { return masterVolume; }

private:
    void playWithVariation (Sound& sound, float screenX, float screenWidth);
    float randomPitchVariation();
    float randomGainVariation();
    float panFromScreenX (float screenX, float screenWidth);

    Sound cannonSounds[2] = { { 0 }, { 0 } };
    Sound splashSound = { 0 };
    Sound explosionSounds[2] = { { 0 }, { 0 } };
    Sound collisionSound = { 0 };
    Music engineSound = { 0 };

    bool initialized = false;

    // Gun silencing - only one gun sound at a time
    float gunSilenceTimer = 0.0f;

    // Random number generator for variations
    std::mt19937 rng;

    float engineVolume = 0.0f;
    float currentEngineVolume = 0.0f;

    // Master volume (0.0 to 1.0)
    int masterVolumeLevel = 5;  // 0-10 scale
    float masterVolume = 0.5f;  // 0.0-1.0 scale
};
