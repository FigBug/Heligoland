#pragma once

#include "FileSystemWatcher.h"
#include <raylib.h>
#include <memory>
#include <string>

// =============================================================================
// Game Configuration
// All tweakable game constants in one place
// =============================================================================

class Config : public FileSystemWatcher::Listener
{
public:
    Config();
    ~Config();

    bool load();
    bool save() const;
    void startWatching();

    // FileSystemWatcher::Listener
    void fileChanged (const std::string& file, FileSystemWatcher::Event event) override;

    // -------------------------------------------------------------------------
    // Ship Physics
    // -------------------------------------------------------------------------
    float shipLength                  = 50.0f;
    float shipWidth                   = 15.0f;
    float shipMaxSpeed                = 7.0f;
    float shipFullSpeedKnots          = 20.0f;     // Display speed at max velocity
    float shipAccelTime               = 20.0f;     // Seconds to reach full speed or stop from full speed with throttle
    float shipCoastStopTime           = 30.0f;     // Seconds to stop when coasting (no throttle)
    float shipThrottleRate            = 0.5f;      // How fast throttle changes per second
    float shipRudderRate              = 2.0f;      // How fast rudder moves
    float shipRudderReturn            = 3.0f;      // How fast rudder returns to center
    float shipMinTurnRadiusMultiplier = 2.0f;      // Min turn radius = length * this
    float shipDamagePenaltyMax        = 0.2f;      // Max speed/turn reduction at 0% health
    float shipSinkDuration            = 30.0f;     // Seconds to fully sink
    float shipSinkVelocityDecay       = 0.98f;
    float shipSinkAngularDecay        = 0.95f;
    float shipReverseSpeedMultiplier  = 0.4f;      // Max reverse speed as fraction of forward

    // -------------------------------------------------------------------------
    // Ship Health
    // -------------------------------------------------------------------------
    float shipMaxHealth               = 1000.0f;
    float shellDamage                 = 100.0f;

    // -------------------------------------------------------------------------
    // Turrets
    // -------------------------------------------------------------------------
    float turretRotationSpeed         = 0.5f;      // Radians per second
    float turretArcSize               = 0.75f;     // Arc = PI * this (0.75 = 135 degrees)
    float turretOnTargetTolerance     = 0.09f;     // Radians (~5 degrees)

    // -------------------------------------------------------------------------
    // Shells / Firing
    // -------------------------------------------------------------------------
    float fireInterval                = 15.0f;     // Seconds between shots
    float shellSpeedMultiplier        = 5.0f;      // Shell speed = ship max speed * this
    float shellShipVelocityFactor     = 0.25f;    // How much ship velocity affects shell (0 = none, 1 = full)
    float shellSpread                 = 0.03f;    // Random angle deviation in radians (~1.7 degrees)
    float shellRangeVariation         = 0.05f;    // Random range variation (0.05 = ±5%)
    float shellRadius                 = 2.0f;
    float shellSplashRadius           = 4.0f;      // Hit detection radius when shell lands
    float minShellRange               = 50.0f;     // Minimum range shells can travel
    float maxShellRange               = 300.0f;    // Maximum range shells can travel

    // -------------------------------------------------------------------------
    // Crosshair / Aiming
    // -------------------------------------------------------------------------
    float crosshairSpeed              = 150.0f;    // How fast crosshair moves with stick
    float crosshairStartDistance      = 150.0f;    // Initial crosshair distance

    // -------------------------------------------------------------------------
    // Bubbles (Wake Trail)
    // -------------------------------------------------------------------------
    float bubbleMinSpeed              = 0.5f;      // Min ship speed to spawn bubbles
    float bubbleSpawnInterval         = 0.02f;
    float bubbleFadeTime              = 10.0f;     // Seconds to fade out
    float bubbleMinRadius             = 1.5f;
    float bubbleRadiusVariation       = 2.0f;      // Random addition to min radius

    // -------------------------------------------------------------------------
    // Smoke
    // -------------------------------------------------------------------------
    float smokeFadeTimeMin            = 10.0f;     // Min seconds to fade out
    float smokeFadeTimeMax            = 14.0f;     // Max seconds to fade out
    float smokeWindStrength           = 30.0f;     // How much wind affects smoke
    float smokeBaseSpawnInterval      = 0.0433f;   // ~30 per second when undamaged
    float smokeDamageMultiplier       = 4.0f;      // Spawn rate increases with damage
    float smokeBaseRadius             = 1.5f;
    float smokeBaseAlpha              = 0.4f;
    float smokeWindAngleVariation     = 0.2f;      // +/- radians (~6 degrees)
    unsigned char smokeGreyStart      = 80;        // Grey value when not sinking
    unsigned char smokeGreyEnd        = 140;       // Grey value when fully sunk

    // -------------------------------------------------------------------------
    // Explosions
    // -------------------------------------------------------------------------
    float explosionDuration           = 0.5f;
    float explosionMaxRadius          = 30.0f;
    float sinkExplosionDuration       = 1.0f;
    float sinkExplosionMaxRadius      = 80.0f;

    // -------------------------------------------------------------------------
    // Wind
    // -------------------------------------------------------------------------
    float windChangeInterval          = 60.0f;     // Seconds between wind target changes
    float windLerpSpeed               = 0.05f;     // How fast wind changes
    float windMaxDrift                = 0.02f;     // Max shell drift (1%)
    float windMinStrength             = 0.25f;     // Minimum wind strength
    float windAngleChangeMax          = 0.524f;    // Max angle change (~30 degrees)
    float windStrengthChangeMax       = 0.4f;      // Max strength change

    // -------------------------------------------------------------------------
    // Collision
    // -------------------------------------------------------------------------
    float collisionRestitution        = 0.5f;      // Bounciness (0 = inelastic, 1 = elastic)
    float collisionAngularFactor      = 0.01f;     // How much collisions affect rotation
    float collisionDamageScale        = 35.7f;     // Two full-speed ships colliding = 5 shell impacts
    float wallBounceMultiplier        = 0.3f;      // Velocity retained after hitting wall

    // -------------------------------------------------------------------------
    // AI
    // -------------------------------------------------------------------------
    float aiWanderInterval            = 3.0f;      // Base seconds between wander targets
    float aiWanderMargin              = 150.0f;    // Keep this far from edges
    float aiLookAheadTime             = 2.0f;      // Seconds to predict ahead
    float aiFireDistance              = 400.0f;    // Max range AI will try to fire
    float aiCrosshairTolerance        = 30.0f;     // How close crosshair needs to be to fire

    // -------------------------------------------------------------------------
    // Audio
    // -------------------------------------------------------------------------
    float audioGunSilenceDuration     = 0.25f;     // Seconds between gun sounds
    float audioPitchVariation         = 0.1f;      // +/- 10%
    float audioGainVariation          = 0.1f;      // +/- 10%
    float audioEngineBaseVolume       = 0.3f;
    float audioEngineThrottleBoost    = 0.7f;
    float audioMinImpactForSound      = 10.0f;     // Min collision speed for sound

    // -------------------------------------------------------------------------
    // Game Flow
    // -------------------------------------------------------------------------
    float gameStartDelay              = 0.5f;      // Ignore fire input for this long
    float gameOverTextDelay           = 5.0f;      // Delay before showing winner text
    float gameOverReturnDelay         = 8.0f;      // Total delay before returning to title

    // -------------------------------------------------------------------------
    // Colors - Environment
    // -------------------------------------------------------------------------
    Color colorOcean                  = { 30, 60, 90, 255 };
    Color colorWaterHighlight1        = { 255, 255, 255, 30 };
    Color colorWaterHighlight2        = { 220, 220, 255, 20 };
    Color colorWaterHighlight3        = { 180, 200, 220, 12 };

    // -------------------------------------------------------------------------
    // Colors - Ships (FFA mode)
    // -------------------------------------------------------------------------
    Color colorShipRed                = { 255, 100, 100, 255 };
    Color colorShipBlue               = { 100, 100, 255, 255 };
    Color colorShipGreen              = { 100, 255, 100, 255 };
    Color colorShipYellow             = { 255, 255, 100, 255 };

    // -------------------------------------------------------------------------
    // Colors - Ships (Team mode)
    // -------------------------------------------------------------------------
    Color colorTeam1                  = { 255, 100, 100, 255 }; // Red
    Color colorTeam2                  = { 100, 100, 255, 255 }; // Blue

    // -------------------------------------------------------------------------
    // Colors - UI
    // -------------------------------------------------------------------------
    Color colorWhite                  = { 255, 255, 255, 255 };
    Color colorBlack                  = { 0, 0, 0, 255 };
    Color colorGrey                   = { 200, 200, 200, 255 };
    Color colorGreyDark               = { 80, 80, 80, 255 };
    Color colorGreyMid                = { 100, 100, 100, 255 };
    Color colorGreyLight              = { 150, 150, 150, 255 };
    Color colorGreySubtle             = { 120, 120, 120, 255 };
    Color colorBarBackground          = { 60, 60, 60, 255 };
    Color colorHudBackground          = { 30, 30, 30, 200 };

    // -------------------------------------------------------------------------
    // Colors - Title Screen
    // -------------------------------------------------------------------------
    Color colorTitle                  = { 255, 255, 255, 255 };
    Color colorSubtitle               = { 200, 200, 200, 255 };
    Color colorModeText               = { 255, 220, 100, 255 };
    Color colorInstruction            = { 150, 150, 150, 255 };

    // -------------------------------------------------------------------------
    // Colors - Gameplay
    // -------------------------------------------------------------------------
    Color colorShell                  = { 255, 60, 40, 255 };
    float shellTrailLength            = 20.0f;
    int shellTrailSegments            = 5;
    Color colorBubble                 = { 255, 255, 255, 128 };
    Color colorBarrel                 = { 50, 50, 50, 255 };
    Color colorReloadReady            = { 100, 255, 100, 255 };
    Color colorReloadNotReady         = { 255, 100, 100, 255 };
    Color colorFiringRange            = { 255, 255, 255, 5 };
    Color colorThrottleBar            = { 100, 150, 255, 255 };
    Color colorRudderBar              = { 255, 200, 100, 255 };

    // -------------------------------------------------------------------------
    // Colors - Explosions
    // -------------------------------------------------------------------------
    Color colorExplosionOuter         = { 255, 150, 50, 200 };
    Color colorExplosionMid           = { 255, 220, 100, 180 };
    Color colorExplosionCore          = { 255, 255, 200, 150 };
    Color colorSplashOuter            = { 100, 150, 255, 200 };
    Color colorSplashMid              = { 150, 200, 255, 180 };
    Color colorSplashCore             = { 220, 240, 255, 150 };

    // -------------------------------------------------------------------------
    // Colors - Wind Indicator
    // -------------------------------------------------------------------------
    Color colorWindBackground         = { 30, 30, 30, 200 };
    Color colorWindBorder             = { 100, 100, 100, 255 };
    Color colorWindArrow              = { 200, 200, 255, 255 };

private:
    std::string getConfigPath() const;
    std::string getConfigDirectory() const;

    std::unique_ptr<FileSystemWatcher> watcher;
};

// Global config instance
extern Config config;
