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
    std::uniform_real_distribution<float> pitchDist { 1.0f - Config::audioPitchVariation, 1.0f + Config::audioPitchVariation };
    std::uniform_real_distribution<float> gainDist { 1.0f - Config::audioGainVariation, 1.0f + Config::audioGainVariation };

    float engineVolume = 0.0f;
    float currentEngineVolume = 0.0f;
};
