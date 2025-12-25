#pragma once

#include <raylib.h>

// =============================================================================
// Game Configuration
// All tweakable game constants in one place
// =============================================================================

namespace Config
{
    // -------------------------------------------------------------------------
    // Ship Physics
    // -------------------------------------------------------------------------
    static constexpr float shipLength                  = 50.0f;
    static constexpr float shipWidth                   = 15.0f;
    static constexpr float shipMaxSpeed                = 7.0f;
    static constexpr float shipFullSpeedKnots          = 20.0f;     // Display speed at max velocity
    static constexpr float shipAccelTime               = 20.0f;     // Seconds to reach full speed or stop from full speed with throttle
    static constexpr float shipCoastStopTime           = 30.0f;     // Seconds to stop when coasting (no throttle)
    static constexpr float shipThrottleRate            = 0.5f;      // How fast throttle changes per second
    static constexpr float shipRudderRate              = 2.0f;      // How fast rudder moves
    static constexpr float shipRudderReturn            = 3.0f;      // How fast rudder returns to center
    static constexpr float shipDragCoefficient         = 0.995f;    // Velocity multiplier when coasting
    static constexpr float shipMinTurnRadiusMultiplier = 2.0f;      // Min turn radius = length * this
    static constexpr float shipDamagePenaltyMax        = 0.2f;      // Max speed/turn reduction at 0% health
    static constexpr float shipSinkDuration            = 30.0f;     // Seconds to fully sink
    static constexpr float shipSinkVelocityDecay       = 0.98f;
    static constexpr float shipSinkAngularDecay        = 0.95f;

    // -------------------------------------------------------------------------
    // Ship Health
    // -------------------------------------------------------------------------
    static constexpr float shipMaxHealth               = 1000.0f;
    static constexpr float shellDamage                 = 100.0f;

    // -------------------------------------------------------------------------
    // Turrets
    // -------------------------------------------------------------------------
    static constexpr float turretRotationSpeed         = 1.0f;      // Radians per second
    static constexpr float turretRadius                = 5.0f;
    static constexpr float turretBarrelLength          = 8.0f;
    static constexpr float turretArcSize               = 0.75f;     // Arc = PI * this (0.75 = 135 degrees)
    static constexpr float turretOnTargetTolerance     = 0.09f;     // Radians (~5 degrees)

    // -------------------------------------------------------------------------
    // Shells / Firing
    // -------------------------------------------------------------------------
    static constexpr float fireInterval                = 15.0f;     // Seconds between shots
    static constexpr float shellSpeedMultiplier        = 5.0f;      // Shell speed = ship max speed * this
    static constexpr float shellRadius                 = 2.0f;
    static constexpr float shellSplashRadius           = 8.0f;      // Hit detection radius when shell lands
    static constexpr float minShellRange               = 50.0f;     // Minimum range shells can travel

    // -------------------------------------------------------------------------
    // Crosshair / Aiming
    // -------------------------------------------------------------------------
    static constexpr float crosshairSpeed              = 150.0f;    // How fast crosshair moves with stick
    static constexpr float maxCrosshairDistance        = 300.0f;
    static constexpr float crosshairStartDistance      = 150.0f;    // Initial crosshair distance

    // -------------------------------------------------------------------------
    // Bubbles (Wake Trail)
    // -------------------------------------------------------------------------
    static constexpr float bubbleMinSpeed              = 5.0f;      // Min ship speed to spawn bubbles
    static constexpr float bubbleSpawnInterval         = 0.02f;
    static constexpr float bubbleFadeTime              = 10.0f;     // Seconds to fade out
    static constexpr float bubbleMinRadius             = 1.5f;
    static constexpr float bubbleRadiusVariation       = 2.0f;      // Random addition to min radius

    // -------------------------------------------------------------------------
    // Smoke
    // -------------------------------------------------------------------------
    static constexpr float smokeFadeTime               = 10.0f;     // Seconds to fade out
    static constexpr float smokeWindStrength           = 15.0f;     // How much wind affects smoke
    static constexpr float smokeBaseSpawnInterval      = 0.0433f;   // ~30 per second when undamaged
    static constexpr float smokeDamageMultiplier       = 4.0f;      // Spawn rate increases with damage
    static constexpr float smokeBaseRadius             = 1.5f;
    static constexpr float smokeBaseAlpha              = 0.4f;
    static constexpr float smokeWindAngleVariation     = 0.155f;    // +/- radians (~6 degrees)
    static constexpr unsigned char smokeGreyStart      = 80;        // Grey value when not sinking
    static constexpr unsigned char smokeGreyEnd        = 140;       // Grey value when fully sunk

    // -------------------------------------------------------------------------
    // Explosions
    // -------------------------------------------------------------------------
    static constexpr float explosionDuration           = 0.5f;
    static constexpr float explosionMaxRadius          = 30.0f;
    static constexpr float sinkExplosionDuration       = 1.0f;
    static constexpr float sinkExplosionMaxRadius      = 80.0f;

    // -------------------------------------------------------------------------
    // Wind
    // -------------------------------------------------------------------------
    static constexpr float windChangeInterval          = 60.0f;     // Seconds between wind target changes
    static constexpr float windLerpSpeed               = 0.05f;     // How fast wind changes
    static constexpr float windMaxDrift                = 0.01f;     // Max shell drift (1%)
    static constexpr float windMinStrength             = 0.25f;     // Minimum wind strength
    static constexpr float windAngleChangeMax          = 0.524f;    // Max angle change (~30 degrees)
    static constexpr float windStrengthChangeMax       = 0.4f;      // Max strength change

    // -------------------------------------------------------------------------
    // Collision
    // -------------------------------------------------------------------------
    static constexpr float collisionRestitution        = 0.5f;      // Bounciness (0 = inelastic, 1 = elastic)
    static constexpr float collisionAngularFactor      = 0.01f;     // How much collisions affect rotation
    static constexpr float collisionDamageScale        = 35.7f;     // Two full-speed ships colliding = 5 shell impacts
    static constexpr float wallBounceMultiplier        = 0.3f;      // Velocity retained after hitting wall

    // -------------------------------------------------------------------------
    // AI
    // -------------------------------------------------------------------------
    static constexpr float aiWanderInterval            = 3.0f;      // Base seconds between wander targets
    static constexpr float aiWanderMargin              = 150.0f;    // Keep this far from edges
    static constexpr float aiLookAheadTime             = 2.0f;      // Seconds to predict ahead
    static constexpr float aiFireDistance              = 400.0f;    // Max range AI will try to fire
    static constexpr float aiCrosshairTolerance        = 30.0f;     // How close crosshair needs to be to fire
    static constexpr float aiMinImpactForSound         = 10.0f;     // Min collision speed for sound

    // -------------------------------------------------------------------------
    // Audio
    // -------------------------------------------------------------------------
    static constexpr float audioGunSilenceDuration     = 0.25f;     // Seconds between gun sounds
    static constexpr float audioPitchVariation         = 0.1f;      // +/- 10%
    static constexpr float audioGainVariation          = 0.1f;      // +/- 10%
    static constexpr float audioEngineBaseVolume       = 0.3f;
    static constexpr float audioEngineThrottleBoost    = 0.7f;

    // -------------------------------------------------------------------------
    // Game Flow
    // -------------------------------------------------------------------------
    static constexpr float gameStartDelay              = 0.5f;      // Ignore fire input for this long
    static constexpr float gameOverTextDelay           = 5.0f;      // Delay before showing winner text
    static constexpr float gameOverReturnDelay         = 8.0f;      // Total delay before returning to title

    // -------------------------------------------------------------------------
    // Colors - Environment
    // -------------------------------------------------------------------------
    static constexpr Color colorOcean                  = { 30, 60, 90, 255 };
    static constexpr Color colorWaterHighlight1        = { 255, 255, 255, 30 };
    static constexpr Color colorWaterHighlight2        = { 220, 220, 255, 20 };
    static constexpr Color colorWaterHighlight3        = { 180, 200, 220, 12 };

    // -------------------------------------------------------------------------
    // Colors - Ships (FFA mode)
    // -------------------------------------------------------------------------
    static constexpr Color colorShipRed                = { 255, 100, 100, 255 };
    static constexpr Color colorShipBlue               = { 100, 100, 255, 255 };
    static constexpr Color colorShipGreen              = { 100, 255, 100, 255 };
    static constexpr Color colorShipYellow             = { 255, 255, 100, 255 };

    // -------------------------------------------------------------------------
    // Colors - Ships (Team mode)
    // -------------------------------------------------------------------------
    static constexpr Color colorTeam1Dark              = { 200, 60, 60, 255 };   // Dark red
    static constexpr Color colorTeam1Light             = { 255, 130, 130, 255 }; // Light red
    static constexpr Color colorTeam2Dark              = { 60, 60, 200, 255 };   // Dark blue
    static constexpr Color colorTeam2Light             = { 130, 130, 255, 255 }; // Light blue

    // -------------------------------------------------------------------------
    // Colors - UI
    // -------------------------------------------------------------------------
    static constexpr Color colorWhite                  = { 255, 255, 255, 255 };
    static constexpr Color colorBlack                  = { 0, 0, 0, 255 };
    static constexpr Color colorGrey                   = { 200, 200, 200, 255 };
    static constexpr Color colorGreyDark               = { 80, 80, 80, 255 };
    static constexpr Color colorGreyMid                = { 100, 100, 100, 255 };
    static constexpr Color colorGreyLight              = { 150, 150, 150, 255 };
    static constexpr Color colorGreySubtle             = { 120, 120, 120, 255 };
    static constexpr Color colorBarBackground          = { 60, 60, 60, 255 };
    static constexpr Color colorHudBackground          = { 30, 30, 30, 200 };

    // -------------------------------------------------------------------------
    // Colors - Title Screen
    // -------------------------------------------------------------------------
    static constexpr Color colorTitle                  = { 255, 255, 255, 255 };
    static constexpr Color colorSubtitle               = { 200, 200, 200, 255 };
    static constexpr Color colorModeText               = { 255, 220, 100, 255 };
    static constexpr Color colorInstruction            = { 150, 150, 150, 255 };

    // -------------------------------------------------------------------------
    // Colors - Gameplay
    // -------------------------------------------------------------------------
    static constexpr Color colorShell                  = { 255, 60, 40, 255 };
    static constexpr float shellTrailLength            = 20.0f;
    static constexpr int shellTrailSegments            = 5;
    static constexpr Color colorBubble                 = { 255, 255, 255, 128 };
    static constexpr Color colorBarrel                 = { 50, 50, 50, 255 };
    static constexpr Color colorReloadReady            = { 100, 255, 100, 255 };
    static constexpr Color colorReloadNotReady         = { 255, 100, 100, 255 };
    static constexpr Color colorFiringRange            = { 255, 255, 255, 5 }; 
    static constexpr Color colorThrottleBar            = { 100, 150, 255, 255 };
    static constexpr Color colorRudderBar              = { 255, 200, 100, 255 };

    // -------------------------------------------------------------------------
    // Colors - Explosions
    // -------------------------------------------------------------------------
    static constexpr Color colorExplosionOuter         = { 255, 150, 50, 200 };
    static constexpr Color colorExplosionMid           = { 255, 220, 100, 180 };
    static constexpr Color colorExplosionCore          = { 255, 255, 200, 150 };
    static constexpr Color colorSplashOuter            = { 100, 150, 255, 200 };
    static constexpr Color colorSplashMid              = { 150, 200, 255, 180 };
    static constexpr Color colorSplashCore             = { 220, 240, 255, 150 };

    // -------------------------------------------------------------------------
    // Colors - Wind Indicator
    // -------------------------------------------------------------------------
    static constexpr Color colorWindBackground         = { 30, 30, 30, 200 };
    static constexpr Color colorWindBorder             = { 100, 100, 100, 255 };
    static constexpr Color colorWindArrow              = { 200, 200, 255, 255 };
}
