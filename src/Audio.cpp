#include "Audio.h"
#include <cmath>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <string>

static std::string getResourcePath (const char* filename)
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle)
    {
        CFURLRef resourceURL = CFBundleCopyResourcesDirectoryURL (mainBundle);
        if (resourceURL)
        {
            char path[1024];
            if (CFURLGetFileSystemRepresentation (resourceURL, true, (UInt8*) path, sizeof (path)))
            {
                CFRelease (resourceURL);
                return std::string (path) + "/" + filename;
            }
            CFRelease (resourceURL);
        }
    }
    return filename; // Fallback to relative path
}
#elif defined(__linux__)
#include <string>
#include <unistd.h>
#include <linux/limits.h>

static std::string getResourcePath (const char* filename)
{
    // First try relative path (for development)
    if (access (filename, F_OK) == 0)
    {
        return filename;
    }

    // Try installed location
    std::string installed = "/usr/share/heligoland/" + std::string (filename);
    if (access (installed.c_str(), F_OK) == 0)
    {
        return installed;
    }

    // Try next to executable
    char exePath[PATH_MAX];
    ssize_t len = readlink ("/proc/self/exe", exePath, sizeof (exePath) - 1);
    if (len != -1)
    {
        exePath[len] = '\0';
        std::string dir (exePath);
        size_t lastSlash = dir.rfind ('/');
        if (lastSlash != std::string::npos)
        {
            dir = dir.substr (0, lastSlash + 1);
            std::string nearExe = dir + filename;
            if (access (nearExe.c_str(), F_OK) == 0)
            {
                return nearExe;
            }
        }
    }

    return filename; // Fallback
}
#else
#include <string>

static std::string getResourcePath (const char* filename)
{
    return filename;
}
#endif

Audio::Audio()
    : rng (std::random_device{}())
{
}

Audio::~Audio()
{
    shutdown();
}

bool Audio::init()
{
    InitAudioDevice();

    if (! IsAudioDeviceReady())
    {
        return false;
    }

    cannonSound = LoadSound (getResourcePath ("assets/cannon.wav").c_str());
    splashSound = LoadSound (getResourcePath ("assets/splash.wav").c_str());
    explosionSound = LoadSound (getResourcePath ("assets/explosion.wav").c_str());
    collisionSound = LoadSound (getResourcePath ("assets/collision.wav").c_str());
    engineSound = LoadMusicStream (getResourcePath ("assets/engine.wav").c_str());

    if (cannonSound.frameCount == 0 || splashSound.frameCount == 0 ||
        explosionSound.frameCount == 0 || collisionSound.frameCount == 0 ||
        engineSound.frameCount == 0)
    {
        return false;
    }

    // Engine loops continuously
    engineSound.looping = true;
    PlayMusicStream (engineSound);
    SetMusicVolume (engineSound, 0.0f);

    initialized = true;
    return true;
}

void Audio::shutdown()
{
    if (initialized)
    {
        UnloadSound (cannonSound);
        UnloadSound (splashSound);
        UnloadSound (explosionSound);
        UnloadSound (collisionSound);
        UnloadMusicStream (engineSound);
        CloseAudioDevice();
        initialized = false;
    }
}

void Audio::update (float dt)
{
    if (! initialized)
        return;

    // Update gun silence timer
    if (gunSilenceTimer > 0.0f)
    {
        gunSilenceTimer -= dt;
    }

    // Smoothly adjust engine volume
    float volumeSpeed = 2.0f; // Takes 0.5s to fully change
    if (currentEngineVolume < engineVolume)
    {
        currentEngineVolume += volumeSpeed * dt;
        if (currentEngineVolume > engineVolume)
            currentEngineVolume = engineVolume;
    }
    else if (currentEngineVolume > engineVolume)
    {
        currentEngineVolume -= volumeSpeed * dt;
        if (currentEngineVolume < engineVolume)
            currentEngineVolume = engineVolume;
    }

    SetMusicVolume (engineSound, currentEngineVolume * 0.3f); // Engine is quieter
    UpdateMusicStream (engineSound);
}

void Audio::playCannon (float screenX, float screenWidth)
{
    if (! initialized)
        return;

    // Only play if not silenced
    if (gunSilenceTimer > 0.0f)
        return;

    playWithVariation (cannonSound, screenX, screenWidth);
    gunSilenceTimer = gunSilenceDuration;
}

void Audio::playSplash (float screenX, float screenWidth)
{
    if (! initialized)
        return;

    playWithVariation (splashSound, screenX, screenWidth);
}

void Audio::playExplosion (float screenX, float screenWidth)
{
    if (! initialized)
        return;

    playWithVariation (explosionSound, screenX, screenWidth);
}

void Audio::playCollision (float screenX, float screenWidth)
{
    if (! initialized)
        return;

    playWithVariation (collisionSound, screenX, screenWidth);
}

void Audio::setEngineVolume (float volume)
{
    engineVolume = std::max (0.0f, std::min (1.0f, volume));
}

void Audio::playWithVariation (Sound& sound, float screenX, float screenWidth)
{
    float pitch = randomPitchVariation();
    float gain = randomGainVariation();
    float pan = panFromScreenX (screenX, screenWidth);

    SetSoundPitch (sound, pitch);
    SetSoundVolume (sound, gain);
    SetSoundPan (sound, pan);
    PlaySound (sound);
}

float Audio::randomPitchVariation()
{
    return pitchDist (rng);
}

float Audio::randomGainVariation()
{
    return gainDist (rng);
}

float Audio::panFromScreenX (float screenX, float screenWidth)
{
    // Pan ranges from 0.0 (left) to 1.0 (right), 0.5 is center
    if (screenWidth <= 0)
        return 0.5f;

    float normalized = screenX / screenWidth;
    // Clamp and scale - don't go full left/right, keep some center
    float pan = 0.2f + normalized * 0.6f; // Range: 0.2 to 0.8
    return std::max (0.0f, std::min (1.0f, pan));
}
