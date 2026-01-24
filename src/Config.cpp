#include "Config.h"
#include "Platform.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// Global config instance
Config config;

namespace
{
    json colorToJson (Color c)
    {
        char hex[10];
        snprintf (hex, sizeof (hex), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
        return std::string (hex);
    }

    Color jsonToColor (const json& j, Color defaultColor)
    {
        if (! j.is_string())
            return defaultColor;

        std::string s = j.get<std::string>();
        if (s.empty() || s[0] != '#' || (s.length() != 7 && s.length() != 9))
            return defaultColor;

        unsigned int r, g, b, a = 255;
        if (s.length() == 7)
        {
            if (sscanf (s.c_str(), "#%02X%02X%02X", &r, &g, &b) != 3)
                return defaultColor;
        }
        else
        {
            if (sscanf (s.c_str(), "#%02X%02X%02X%02X", &r, &g, &b, &a) != 4)
                return defaultColor;
        }

        return {
            static_cast<unsigned char> (r),
            static_cast<unsigned char> (g),
            static_cast<unsigned char> (b),
            static_cast<unsigned char> (a)
        };
    }

    template <typename T>
    void loadValue (const json& j, const char* key, T& value)
    {
        if (j.contains (key))
            value = j[key].get<T>();
    }

    void loadColor (const json& j, const char* key, Color& value)
    {
        if (j.contains (key))
            value = jsonToColor (j[key], value);
    }
}

Config::Config()
{
    initDefaultShipTypes();
}

void Config::initDefaultShipTypes()
{
    // Ship Type 1: Scout - 1 turret, fast but fragile, short range, low damage
    shipTypes[0].name = "Scout";
    shipTypes[0].numTurrets = 1;
    shipTypes[0].turrets[0] = { 0.24f, true };  // Single front turret
    shipTypes[0].numSmokeStacks = 1;
    shipTypes[0].smokeStackOffsets = { -0.12f, 0.0f };  // Single stack behind center
    shipTypes[0].healthMultiplier = 0.6f;
    shipTypes[0].speedMultiplier = 1.4f;
    shipTypes[0].turnMultiplier = 1.25f;
    shipTypes[0].reloadMultiplier = 0.5f;       // Faster reload
    shipTypes[0].rangeMultiplier = 0.65f;        // Short range
    shipTypes[0].damageMultiplier = 0.6f;       // Low damage
    shipTypes[0].turretSpeedMultiplier = 1.4f;  // Fast turret

    // Ship Type 2: Frigate - 2 turrets, balanced
    shipTypes[1].name = "Frigate";
    shipTypes[1].numTurrets = 2;
    shipTypes[1].turrets[0] = { 0.19f, true };   // Front turret
    shipTypes[1].turrets[1] = { -0.41f, false }; // Rear turret
    shipTypes[1].numSmokeStacks = 1;
    shipTypes[1].smokeStackOffsets = { -0.13f, 0.0f };  // Single stack behind center
    shipTypes[1].healthMultiplier = 0.8f;
    shipTypes[1].speedMultiplier = 1.15f;
    shipTypes[1].turnMultiplier = 1.15f;
    shipTypes[1].reloadMultiplier = 0.85f;
    shipTypes[1].rangeMultiplier = 0.85f;       // Medium-short range
    shipTypes[1].damageMultiplier = 0.8f;       // Medium-low damage
    shipTypes[1].turretSpeedMultiplier = 1.15f; // Slightly fast turret

    // Ship Type 3: Cruiser - 3 turrets, standard
    shipTypes[2].name = "Cruiser";
    shipTypes[2].numTurrets = 3;
    shipTypes[2].turrets[0] = { 0.28f, true };   // Front
    shipTypes[2].turrets[1] = { 0.17f, true };   // Mid (front-facing)
    shipTypes[2].turrets[2] = { -0.350f, false }; // Rear
    shipTypes[2].numSmokeStacks = 2;
    shipTypes[2].smokeStackOffsets = { -0.066f, -0.109f };  // Two stacks near center
    shipTypes[2].healthMultiplier = 1.0f;
    shipTypes[2].speedMultiplier = 1.0f;
    shipTypes[2].turnMultiplier = 1.0f;
    shipTypes[2].reloadMultiplier = 1.0f;
    shipTypes[2].rangeMultiplier = 1.0f;        // Standard range
    shipTypes[2].damageMultiplier = 1.0f;       // Standard damage
    shipTypes[2].turretSpeedMultiplier = 1.0f;  // Standard turret

    // Ship Type 4: Battleship - 4 turrets, slow but powerful, long range, high damage
    shipTypes[3].name = "Battleship";
    shipTypes[3].numTurrets = 4;
    shipTypes[3].turrets[0] = { 0.29f, true };   // Front
    shipTypes[3].turrets[1] = { 0.185f, true };   // Front-mid
    shipTypes[3].turrets[2] = { -0.28f, false }; // Rear-mid
    shipTypes[3].turrets[3] = { -0.41f, false }; // Rear
    shipTypes[3].numSmokeStacks = 2;
    shipTypes[3].smokeStackOffsets = { -0.027f, -0.0645f };  // Two stacks near center
    shipTypes[3].healthMultiplier = 1.25f;
    shipTypes[3].speedMultiplier = 0.8f;
    shipTypes[3].turnMultiplier = 0.8f;
    shipTypes[3].reloadMultiplier = 1.15f;      // Slower reload
    shipTypes[3].rangeMultiplier = 1.15f;       // Long range
    shipTypes[3].damageMultiplier = 1.2f;       // High damage
    shipTypes[3].turretSpeedMultiplier = 0.8f;  // Slow turret
}

Config::~Config()
{
    if (watcher)
    {
        watcher->removeListener (this);
        watcher.reset();
    }
}

std::string Config::getConfigDirectory() const
{
    return Platform::getUserDataDirectory();
}

std::string Config::getConfigPath() const
{
    std::string dir = Platform::getUserDataDirectory();
    if (dir.empty())
        return "";
    return dir + "/config.json";
}

bool Config::load()
{
    std::string path = getConfigPath();
    if (path.empty())
        return false;

    std::ifstream file (path);
    if (! file.is_open())
        return false;

    json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        return false;
    }

    // Helper to get a section if it exists
    auto getSection = [&j] (const char* name) -> const json& {
        static const json empty = json::object();
        return j.contains (name) ? j[name] : empty;
    };

    // Ship Physics
    {
        const auto& s = getSection ("shipPhysics");
        loadValue (s, "maxSpeed", shipMaxSpeed);
        loadValue (s, "fullSpeedKnots", shipFullSpeedKnots);
        loadValue (s, "accelTime", shipAccelTime);
        loadValue (s, "coastStopTime", shipCoastStopTime);
        loadValue (s, "throttleRate", shipThrottleRate);
        loadValue (s, "rudderRate", shipRudderRate);
        loadValue (s, "rudderReturn", shipRudderReturn);
        loadValue (s, "minTurnRadiusMultiplier", shipMinTurnRadiusMultiplier);
        loadValue (s, "damagePenaltyMax", shipDamagePenaltyMax);
        loadValue (s, "sinkDuration", shipSinkDuration);
        loadValue (s, "sinkVelocityDecay", shipSinkVelocityDecay);
        loadValue (s, "sinkAngularDecay", shipSinkAngularDecay);
        loadValue (s, "reverseSpeedMultiplier", shipReverseSpeedMultiplier);
    }

    // Ship Health
    {
        const auto& s = getSection ("shipHealth");
        loadValue (s, "maxHealth", shipMaxHealth);
        loadValue (s, "shellDamage", shellDamage);
    }

    // Turrets
    {
        const auto& s = getSection ("turrets");
        loadValue (s, "rotationSpeed", turretRotationSpeed);
        loadValue (s, "arcSize", turretArcSize);
        loadValue (s, "onTargetTolerance", turretOnTargetTolerance);
    }

    // Shells / Firing
    {
        const auto& s = getSection ("shells");
        loadValue (s, "fireInterval", fireInterval);
        loadValue (s, "speedMultiplier", shellSpeedMultiplier);
        loadValue (s, "shipVelocityFactor", shellShipVelocityFactor);
        loadValue (s, "spread", shellSpread);
        loadValue (s, "rangeVariation", shellRangeVariation);
        loadValue (s, "radius", shellRadius);
        loadValue (s, "splashRadius", shellSplashRadius);
        loadValue (s, "minRange", minShellRange);
        loadValue (s, "maxRange", maxShellRange);
    }

    // Crosshair / Aiming
    {
        const auto& s = getSection ("crosshair");
        loadValue (s, "speed", crosshairSpeed);
        loadValue (s, "startDistance", crosshairStartDistance);
    }

    // Bubbles
    {
        const auto& s = getSection ("bubbles");
        loadValue (s, "minSpeed", bubbleMinSpeed);
        loadValue (s, "spawnInterval", bubbleSpawnInterval);
        loadValue (s, "fadeTime", bubbleFadeTime);
        loadValue (s, "minRadius", bubbleMinRadius);
        loadValue (s, "radiusVariation", bubbleRadiusVariation);
    }

    // Smoke
    {
        const auto& s = getSection ("smoke");
        loadValue (s, "fadeTimeMin", smokeFadeTimeMin);
        loadValue (s, "fadeTimeMax", smokeFadeTimeMax);
        loadValue (s, "windStrength", smokeWindStrength);
        loadValue (s, "baseSpawnInterval", smokeBaseSpawnInterval);
        loadValue (s, "damageMultiplier", smokeDamageMultiplier);
        loadValue (s, "baseRadius", smokeBaseRadius);
        loadValue (s, "baseAlpha", smokeBaseAlpha);
        loadValue (s, "windAngleVariation", smokeWindAngleVariation);
        loadValue (s, "greyStart", smokeGreyStart);
        loadValue (s, "greyEnd", smokeGreyEnd);
    }

    // Explosions
    {
        const auto& s = getSection ("explosions");
        loadValue (s, "duration", explosionDuration);
        loadValue (s, "maxRadius", explosionMaxRadius);
        loadValue (s, "sinkDuration", sinkExplosionDuration);
        loadValue (s, "sinkMaxRadius", sinkExplosionMaxRadius);
    }

    // Wind
    {
        const auto& s = getSection ("wind");
        loadValue (s, "changeInterval", windChangeInterval);
        loadValue (s, "lerpSpeed", windLerpSpeed);
        loadValue (s, "maxDrift", windMaxDrift);
        loadValue (s, "minStrength", windMinStrength);
        loadValue (s, "angleChangeMax", windAngleChangeMax);
        loadValue (s, "strengthChangeMax", windStrengthChangeMax);
    }

    // Collision
    {
        const auto& s = getSection ("collision");
        loadValue (s, "restitution", collisionRestitution);
        loadValue (s, "angularFactor", collisionAngularFactor);
        loadValue (s, "damageScale", collisionDamageScale);
        loadValue (s, "wallBounceMultiplier", wallBounceMultiplier);
    }

    // AI
    {
        const auto& s = getSection ("ai");
        loadValue (s, "wanderInterval", aiWanderInterval);
        loadValue (s, "wanderMargin", aiWanderMargin);
        loadValue (s, "lookAheadTime", aiLookAheadTime);
        loadValue (s, "fireDistance", aiFireDistance);
        loadValue (s, "crosshairTolerance", aiCrosshairTolerance);
    }

    // Audio
    {
        const auto& s = getSection ("audio");
        loadValue (s, "gunSilenceDuration", audioGunSilenceDuration);
        loadValue (s, "pitchVariation", audioPitchVariation);
        loadValue (s, "gainVariation", audioGainVariation);
        loadValue (s, "engineBaseVolume", audioEngineBaseVolume);
        loadValue (s, "engineThrottleBoost", audioEngineThrottleBoost);
        loadValue (s, "minImpactForSound", audioMinImpactForSound);
    }

    // Game Flow
    {
        const auto& s = getSection ("gameFlow");
        loadValue (s, "startDelay", gameStartDelay);
        loadValue (s, "overTextDelay", gameOverTextDelay);
        loadValue (s, "overReturnDelay", gameOverReturnDelay);
    }

    // Colors - Environment
    {
        const auto& s = getSection ("colorsEnvironment");
        loadColor (s, "ocean", colorOcean);
        loadColor (s, "waterHighlight1", colorWaterHighlight1);
        loadColor (s, "waterHighlight2", colorWaterHighlight2);
        loadColor (s, "waterHighlight3", colorWaterHighlight3);
    }

    // Colors - Ships (FFA mode)
    {
        const auto& s = getSection ("colorsShipsFFA");
        loadColor (s, "red", colorShipRed);
        loadColor (s, "blue", colorShipBlue);
        loadColor (s, "green", colorShipGreen);
        loadColor (s, "yellow", colorShipYellow);
    }

    // Colors - Ships (Team mode)
    {
        const auto& s = getSection ("colorsShipsTeam");
        loadColor (s, "team1", colorTeam1);
        loadColor (s, "team2", colorTeam2);
    }

    // Colors - UI
    {
        const auto& s = getSection ("colorsUI");
        loadColor (s, "white", colorWhite);
        loadColor (s, "black", colorBlack);
        loadColor (s, "grey", colorGrey);
        loadColor (s, "greyDark", colorGreyDark);
        loadColor (s, "greyMid", colorGreyMid);
        loadColor (s, "greyLight", colorGreyLight);
        loadColor (s, "greySubtle", colorGreySubtle);
        loadColor (s, "barBackground", colorBarBackground);
        loadColor (s, "hudBackground", colorHudBackground);
    }

    // Colors - Title Screen
    {
        const auto& s = getSection ("colorsTitleScreen");
        loadColor (s, "title", colorTitle);
        loadColor (s, "subtitle", colorSubtitle);
        loadColor (s, "modeText", colorModeText);
        loadColor (s, "instruction", colorInstruction);
    }

    // Colors - Gameplay
    {
        const auto& s = getSection ("colorsGameplay");
        loadColor (s, "shell", colorShell);
        loadValue (s, "shellTrailLength", shellTrailLength);
        loadValue (s, "shellTrailSegments", shellTrailSegments);
        loadColor (s, "bubble", colorBubble);
        loadColor (s, "barrel", colorBarrel);
        loadColor (s, "reloadReady", colorReloadReady);
        loadColor (s, "reloadNotReady", colorReloadNotReady);
        loadColor (s, "firingRange", colorFiringRange);
        loadColor (s, "throttleBar", colorThrottleBar);
        loadColor (s, "rudderBar", colorRudderBar);
    }

    // Colors - Explosions
    {
        const auto& s = getSection ("colorsExplosions");
        loadColor (s, "explosionOuter", colorExplosionOuter);
        loadColor (s, "explosionMid", colorExplosionMid);
        loadColor (s, "explosionCore", colorExplosionCore);
        loadColor (s, "splashOuter", colorSplashOuter);
        loadColor (s, "splashMid", colorSplashMid);
        loadColor (s, "splashCore", colorSplashCore);
    }

    // Colors - Wind Indicator
    {
        const auto& s = getSection ("colorsWindIndicator");
        loadColor (s, "background", colorWindBackground);
        loadColor (s, "border", colorWindBorder);
        loadColor (s, "arrow", colorWindArrow);
    }

    return true;
}

bool Config::save() const
{
    std::string path = getConfigPath();
    if (path.empty())
        return false;

    json j;

    // Version
    j["version"] = HELIGOLAND_VERSION;

    // Ship Physics
    j["shipPhysics"] = {
        { "maxSpeed", shipMaxSpeed },
        { "fullSpeedKnots", shipFullSpeedKnots },
        { "accelTime", shipAccelTime },
        { "coastStopTime", shipCoastStopTime },
        { "throttleRate", shipThrottleRate },
        { "rudderRate", shipRudderRate },
        { "rudderReturn", shipRudderReturn },
        { "minTurnRadiusMultiplier", shipMinTurnRadiusMultiplier },
        { "damagePenaltyMax", shipDamagePenaltyMax },
        { "sinkDuration", shipSinkDuration },
        { "sinkVelocityDecay", shipSinkVelocityDecay },
        { "sinkAngularDecay", shipSinkAngularDecay },
        { "reverseSpeedMultiplier", shipReverseSpeedMultiplier }
    };

    // Ship Health
    j["shipHealth"] = {
        { "maxHealth", shipMaxHealth },
        { "shellDamage", shellDamage }
    };

    // Turrets
    j["turrets"] = {
        { "rotationSpeed", turretRotationSpeed },
        { "arcSize", turretArcSize },
        { "onTargetTolerance", turretOnTargetTolerance }
    };

    // Shells / Firing
    j["shells"] = {
        { "fireInterval", fireInterval },
        { "speedMultiplier", shellSpeedMultiplier },
        { "shipVelocityFactor", shellShipVelocityFactor },
        { "spread", shellSpread },
        { "rangeVariation", shellRangeVariation },
        { "radius", shellRadius },
        { "splashRadius", shellSplashRadius },
        { "minRange", minShellRange },
        { "maxRange", maxShellRange }
    };

    // Crosshair / Aiming
    j["crosshair"] = {
        { "speed", crosshairSpeed },
        { "startDistance", crosshairStartDistance }
    };

    // Bubbles
    j["bubbles"] = {
        { "minSpeed", bubbleMinSpeed },
        { "spawnInterval", bubbleSpawnInterval },
        { "fadeTime", bubbleFadeTime },
        { "minRadius", bubbleMinRadius },
        { "radiusVariation", bubbleRadiusVariation }
    };

    // Smoke
    j["smoke"] = {
        { "fadeTimeMin", smokeFadeTimeMin },
        { "fadeTimeMax", smokeFadeTimeMax },
        { "windStrength", smokeWindStrength },
        { "baseSpawnInterval", smokeBaseSpawnInterval },
        { "damageMultiplier", smokeDamageMultiplier },
        { "baseRadius", smokeBaseRadius },
        { "baseAlpha", smokeBaseAlpha },
        { "windAngleVariation", smokeWindAngleVariation },
        { "greyStart", smokeGreyStart },
        { "greyEnd", smokeGreyEnd }
    };

    // Explosions
    j["explosions"] = {
        { "duration", explosionDuration },
        { "maxRadius", explosionMaxRadius },
        { "sinkDuration", sinkExplosionDuration },
        { "sinkMaxRadius", sinkExplosionMaxRadius }
    };

    // Wind
    j["wind"] = {
        { "changeInterval", windChangeInterval },
        { "lerpSpeed", windLerpSpeed },
        { "maxDrift", windMaxDrift },
        { "minStrength", windMinStrength },
        { "angleChangeMax", windAngleChangeMax },
        { "strengthChangeMax", windStrengthChangeMax }
    };

    // Collision
    j["collision"] = {
        { "restitution", collisionRestitution },
        { "angularFactor", collisionAngularFactor },
        { "damageScale", collisionDamageScale },
        { "wallBounceMultiplier", wallBounceMultiplier }
    };

    // AI
    j["ai"] = {
        { "wanderInterval", aiWanderInterval },
        { "wanderMargin", aiWanderMargin },
        { "lookAheadTime", aiLookAheadTime },
        { "fireDistance", aiFireDistance },
        { "crosshairTolerance", aiCrosshairTolerance }
    };

    // Audio
    j["audio"] = {
        { "gunSilenceDuration", audioGunSilenceDuration },
        { "pitchVariation", audioPitchVariation },
        { "gainVariation", audioGainVariation },
        { "engineBaseVolume", audioEngineBaseVolume },
        { "engineThrottleBoost", audioEngineThrottleBoost },
        { "minImpactForSound", audioMinImpactForSound }
    };

    // Game Flow
    j["gameFlow"] = {
        { "startDelay", gameStartDelay },
        { "overTextDelay", gameOverTextDelay },
        { "overReturnDelay", gameOverReturnDelay }
    };

    // Colors - Environment
    j["colorsEnvironment"] = {
        { "ocean", colorToJson (colorOcean) },
        { "waterHighlight1", colorToJson (colorWaterHighlight1) },
        { "waterHighlight2", colorToJson (colorWaterHighlight2) },
        { "waterHighlight3", colorToJson (colorWaterHighlight3) }
    };

    // Colors - Ships (FFA mode)
    j["colorsShipsFFA"] = {
        { "red", colorToJson (colorShipRed) },
        { "blue", colorToJson (colorShipBlue) },
        { "green", colorToJson (colorShipGreen) },
        { "yellow", colorToJson (colorShipYellow) }
    };

    // Colors - Ships (Team mode)
    j["colorsShipsTeam"] = {
        { "team1", colorToJson (colorTeam1) },
        { "team2", colorToJson (colorTeam2) }
    };

    // Colors - UI
    j["colorsUI"] = {
        { "white", colorToJson (colorWhite) },
        { "black", colorToJson (colorBlack) },
        { "grey", colorToJson (colorGrey) },
        { "greyDark", colorToJson (colorGreyDark) },
        { "greyMid", colorToJson (colorGreyMid) },
        { "greyLight", colorToJson (colorGreyLight) },
        { "greySubtle", colorToJson (colorGreySubtle) },
        { "barBackground", colorToJson (colorBarBackground) },
        { "hudBackground", colorToJson (colorHudBackground) }
    };

    // Colors - Title Screen
    j["colorsTitleScreen"] = {
        { "title", colorToJson (colorTitle) },
        { "subtitle", colorToJson (colorSubtitle) },
        { "modeText", colorToJson (colorModeText) },
        { "instruction", colorToJson (colorInstruction) }
    };

    // Colors - Gameplay
    j["colorsGameplay"] = {
        { "shell", colorToJson (colorShell) },
        { "shellTrailLength", shellTrailLength },
        { "shellTrailSegments", shellTrailSegments },
        { "bubble", colorToJson (colorBubble) },
        { "barrel", colorToJson (colorBarrel) },
        { "reloadReady", colorToJson (colorReloadReady) },
        { "reloadNotReady", colorToJson (colorReloadNotReady) },
        { "firingRange", colorToJson (colorFiringRange) },
        { "throttleBar", colorToJson (colorThrottleBar) },
        { "rudderBar", colorToJson (colorRudderBar) }
    };

    // Colors - Explosions
    j["colorsExplosions"] = {
        { "explosionOuter", colorToJson (colorExplosionOuter) },
        { "explosionMid", colorToJson (colorExplosionMid) },
        { "explosionCore", colorToJson (colorExplosionCore) },
        { "splashOuter", colorToJson (colorSplashOuter) },
        { "splashMid", colorToJson (colorSplashMid) },
        { "splashCore", colorToJson (colorSplashCore) }
    };

    // Colors - Wind Indicator
    j["colorsWindIndicator"] = {
        { "background", colorToJson (colorWindBackground) },
        { "border", colorToJson (colorWindBorder) },
        { "arrow", colorToJson (colorWindArrow) }
    };

    std::ofstream file (path);
    if (! file.is_open())
        return false;

    file << j.dump (4);
    return true;
}

void Config::startWatching()
{
    std::string dir = getConfigDirectory();
    if (dir.empty())
        return;

    watcher = std::make_unique<FileSystemWatcher>();
    watcher->addListener (this);
    watcher->addFolder (dir);
}

void Config::fileChanged (const std::string& file, FileSystemWatcher::Event event)
{
    // Check if it's the config file that changed
    std::string configPath = getConfigPath();
    if (file == configPath && event == FileSystemWatcher::Event::fileModified)
    {
        load();
    }
}
