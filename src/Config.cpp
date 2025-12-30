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
        return json::array ({ c.r, c.g, c.b, c.a });
    }

    Color jsonToColor (const json& j, Color defaultColor)
    {
        if (! j.is_array() || j.size() != 4)
            return defaultColor;

        return {
            static_cast<unsigned char> (j[0].get<int>()),
            static_cast<unsigned char> (j[1].get<int>()),
            static_cast<unsigned char> (j[2].get<int>()),
            static_cast<unsigned char> (j[3].get<int>())
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

Config::Config() = default;

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

    // Ship Physics
    loadValue (j, "shipLength", shipLength);
    loadValue (j, "shipWidth", shipWidth);
    loadValue (j, "shipMaxSpeed", shipMaxSpeed);
    loadValue (j, "shipFullSpeedKnots", shipFullSpeedKnots);
    loadValue (j, "shipAccelTime", shipAccelTime);
    loadValue (j, "shipCoastStopTime", shipCoastStopTime);
    loadValue (j, "shipThrottleRate", shipThrottleRate);
    loadValue (j, "shipRudderRate", shipRudderRate);
    loadValue (j, "shipRudderReturn", shipRudderReturn);
    loadValue (j, "shipDragCoefficient", shipDragCoefficient);
    loadValue (j, "shipMinTurnRadiusMultiplier", shipMinTurnRadiusMultiplier);
    loadValue (j, "shipDamagePenaltyMax", shipDamagePenaltyMax);
    loadValue (j, "shipSinkDuration", shipSinkDuration);
    loadValue (j, "shipSinkVelocityDecay", shipSinkVelocityDecay);
    loadValue (j, "shipSinkAngularDecay", shipSinkAngularDecay);

    // Ship Health
    loadValue (j, "shipMaxHealth", shipMaxHealth);
    loadValue (j, "shellDamage", shellDamage);

    // Turrets
    loadValue (j, "turretRotationSpeed", turretRotationSpeed);
    loadValue (j, "turretRadius", turretRadius);
    loadValue (j, "turretBarrelLength", turretBarrelLength);
    loadValue (j, "turretArcSize", turretArcSize);
    loadValue (j, "turretOnTargetTolerance", turretOnTargetTolerance);

    // Shells / Firing
    loadValue (j, "fireInterval", fireInterval);
    loadValue (j, "shellSpeedMultiplier", shellSpeedMultiplier);
    loadValue (j, "shellShipVelocityFactor", shellShipVelocityFactor);
    loadValue (j, "shellSpread", shellSpread);
    loadValue (j, "shellRangeVariation", shellRangeVariation);
    loadValue (j, "shellRadius", shellRadius);
    loadValue (j, "shellSplashRadius", shellSplashRadius);
    loadValue (j, "minShellRange", minShellRange);

    // Crosshair / Aiming
    loadValue (j, "crosshairSpeed", crosshairSpeed);
    loadValue (j, "maxCrosshairDistance", maxCrosshairDistance);
    loadValue (j, "crosshairStartDistance", crosshairStartDistance);

    // Bubbles
    loadValue (j, "bubbleMinSpeed", bubbleMinSpeed);
    loadValue (j, "bubbleSpawnInterval", bubbleSpawnInterval);
    loadValue (j, "bubbleFadeTime", bubbleFadeTime);
    loadValue (j, "bubbleMinRadius", bubbleMinRadius);
    loadValue (j, "bubbleRadiusVariation", bubbleRadiusVariation);

    // Smoke
    loadValue (j, "smokeFadeTime", smokeFadeTime);
    loadValue (j, "smokeWindStrength", smokeWindStrength);
    loadValue (j, "smokeBaseSpawnInterval", smokeBaseSpawnInterval);
    loadValue (j, "smokeDamageMultiplier", smokeDamageMultiplier);
    loadValue (j, "smokeBaseRadius", smokeBaseRadius);
    loadValue (j, "smokeBaseAlpha", smokeBaseAlpha);
    loadValue (j, "smokeWindAngleVariation", smokeWindAngleVariation);
    loadValue (j, "smokeGreyStart", smokeGreyStart);
    loadValue (j, "smokeGreyEnd", smokeGreyEnd);

    // Explosions
    loadValue (j, "explosionDuration", explosionDuration);
    loadValue (j, "explosionMaxRadius", explosionMaxRadius);
    loadValue (j, "sinkExplosionDuration", sinkExplosionDuration);
    loadValue (j, "sinkExplosionMaxRadius", sinkExplosionMaxRadius);

    // Wind
    loadValue (j, "windChangeInterval", windChangeInterval);
    loadValue (j, "windLerpSpeed", windLerpSpeed);
    loadValue (j, "windMaxDrift", windMaxDrift);
    loadValue (j, "windMinStrength", windMinStrength);
    loadValue (j, "windAngleChangeMax", windAngleChangeMax);
    loadValue (j, "windStrengthChangeMax", windStrengthChangeMax);

    // Collision
    loadValue (j, "collisionRestitution", collisionRestitution);
    loadValue (j, "collisionAngularFactor", collisionAngularFactor);
    loadValue (j, "collisionDamageScale", collisionDamageScale);
    loadValue (j, "wallBounceMultiplier", wallBounceMultiplier);

    // AI
    loadValue (j, "aiWanderInterval", aiWanderInterval);
    loadValue (j, "aiWanderMargin", aiWanderMargin);
    loadValue (j, "aiLookAheadTime", aiLookAheadTime);
    loadValue (j, "aiFireDistance", aiFireDistance);
    loadValue (j, "aiCrosshairTolerance", aiCrosshairTolerance);
    loadValue (j, "aiMinImpactForSound", aiMinImpactForSound);

    // Audio
    loadValue (j, "audioGunSilenceDuration", audioGunSilenceDuration);
    loadValue (j, "audioPitchVariation", audioPitchVariation);
    loadValue (j, "audioGainVariation", audioGainVariation);
    loadValue (j, "audioEngineBaseVolume", audioEngineBaseVolume);
    loadValue (j, "audioEngineThrottleBoost", audioEngineThrottleBoost);

    // Game Flow
    loadValue (j, "gameStartDelay", gameStartDelay);
    loadValue (j, "gameOverTextDelay", gameOverTextDelay);
    loadValue (j, "gameOverReturnDelay", gameOverReturnDelay);

    // Colors - Environment
    loadColor (j, "colorOcean", colorOcean);
    loadColor (j, "colorWaterHighlight1", colorWaterHighlight1);
    loadColor (j, "colorWaterHighlight2", colorWaterHighlight2);
    loadColor (j, "colorWaterHighlight3", colorWaterHighlight3);

    // Colors - Ships (FFA mode)
    loadColor (j, "colorShipRed", colorShipRed);
    loadColor (j, "colorShipBlue", colorShipBlue);
    loadColor (j, "colorShipGreen", colorShipGreen);
    loadColor (j, "colorShipYellow", colorShipYellow);

    // Colors - Ships (Team mode)
    loadColor (j, "colorTeam1", colorTeam1);
    loadColor (j, "colorTeam2", colorTeam2);

    // Colors - UI
    loadColor (j, "colorWhite", colorWhite);
    loadColor (j, "colorBlack", colorBlack);
    loadColor (j, "colorGrey", colorGrey);
    loadColor (j, "colorGreyDark", colorGreyDark);
    loadColor (j, "colorGreyMid", colorGreyMid);
    loadColor (j, "colorGreyLight", colorGreyLight);
    loadColor (j, "colorGreySubtle", colorGreySubtle);
    loadColor (j, "colorBarBackground", colorBarBackground);
    loadColor (j, "colorHudBackground", colorHudBackground);

    // Colors - Title Screen
    loadColor (j, "colorTitle", colorTitle);
    loadColor (j, "colorSubtitle", colorSubtitle);
    loadColor (j, "colorModeText", colorModeText);
    loadColor (j, "colorInstruction", colorInstruction);

    // Colors - Gameplay
    loadColor (j, "colorShell", colorShell);
    loadValue (j, "shellTrailLength", shellTrailLength);
    loadValue (j, "shellTrailSegments", shellTrailSegments);
    loadColor (j, "colorBubble", colorBubble);
    loadColor (j, "colorBarrel", colorBarrel);
    loadColor (j, "colorReloadReady", colorReloadReady);
    loadColor (j, "colorReloadNotReady", colorReloadNotReady);
    loadColor (j, "colorFiringRange", colorFiringRange);
    loadColor (j, "colorThrottleBar", colorThrottleBar);
    loadColor (j, "colorRudderBar", colorRudderBar);

    // Colors - Explosions
    loadColor (j, "colorExplosionOuter", colorExplosionOuter);
    loadColor (j, "colorExplosionMid", colorExplosionMid);
    loadColor (j, "colorExplosionCore", colorExplosionCore);
    loadColor (j, "colorSplashOuter", colorSplashOuter);
    loadColor (j, "colorSplashMid", colorSplashMid);
    loadColor (j, "colorSplashCore", colorSplashCore);

    // Colors - Wind Indicator
    loadColor (j, "colorWindBackground", colorWindBackground);
    loadColor (j, "colorWindBorder", colorWindBorder);
    loadColor (j, "colorWindArrow", colorWindArrow);

    return true;
}

bool Config::save() const
{
    std::string path = getConfigPath();
    if (path.empty())
        return false;

    json j;

    // Ship Physics
    j["shipLength"] = shipLength;
    j["shipWidth"] = shipWidth;
    j["shipMaxSpeed"] = shipMaxSpeed;
    j["shipFullSpeedKnots"] = shipFullSpeedKnots;
    j["shipAccelTime"] = shipAccelTime;
    j["shipCoastStopTime"] = shipCoastStopTime;
    j["shipThrottleRate"] = shipThrottleRate;
    j["shipRudderRate"] = shipRudderRate;
    j["shipRudderReturn"] = shipRudderReturn;
    j["shipDragCoefficient"] = shipDragCoefficient;
    j["shipMinTurnRadiusMultiplier"] = shipMinTurnRadiusMultiplier;
    j["shipDamagePenaltyMax"] = shipDamagePenaltyMax;
    j["shipSinkDuration"] = shipSinkDuration;
    j["shipSinkVelocityDecay"] = shipSinkVelocityDecay;
    j["shipSinkAngularDecay"] = shipSinkAngularDecay;

    // Ship Health
    j["shipMaxHealth"] = shipMaxHealth;
    j["shellDamage"] = shellDamage;

    // Turrets
    j["turretRotationSpeed"] = turretRotationSpeed;
    j["turretRadius"] = turretRadius;
    j["turretBarrelLength"] = turretBarrelLength;
    j["turretArcSize"] = turretArcSize;
    j["turretOnTargetTolerance"] = turretOnTargetTolerance;

    // Shells / Firing
    j["fireInterval"] = fireInterval;
    j["shellSpeedMultiplier"] = shellSpeedMultiplier;
    j["shellShipVelocityFactor"] = shellShipVelocityFactor;
    j["shellSpread"] = shellSpread;
    j["shellRangeVariation"] = shellRangeVariation;
    j["shellRadius"] = shellRadius;
    j["shellSplashRadius"] = shellSplashRadius;
    j["minShellRange"] = minShellRange;

    // Crosshair / Aiming
    j["crosshairSpeed"] = crosshairSpeed;
    j["maxCrosshairDistance"] = maxCrosshairDistance;
    j["crosshairStartDistance"] = crosshairStartDistance;

    // Bubbles
    j["bubbleMinSpeed"] = bubbleMinSpeed;
    j["bubbleSpawnInterval"] = bubbleSpawnInterval;
    j["bubbleFadeTime"] = bubbleFadeTime;
    j["bubbleMinRadius"] = bubbleMinRadius;
    j["bubbleRadiusVariation"] = bubbleRadiusVariation;

    // Smoke
    j["smokeFadeTime"] = smokeFadeTime;
    j["smokeWindStrength"] = smokeWindStrength;
    j["smokeBaseSpawnInterval"] = smokeBaseSpawnInterval;
    j["smokeDamageMultiplier"] = smokeDamageMultiplier;
    j["smokeBaseRadius"] = smokeBaseRadius;
    j["smokeBaseAlpha"] = smokeBaseAlpha;
    j["smokeWindAngleVariation"] = smokeWindAngleVariation;
    j["smokeGreyStart"] = smokeGreyStart;
    j["smokeGreyEnd"] = smokeGreyEnd;

    // Explosions
    j["explosionDuration"] = explosionDuration;
    j["explosionMaxRadius"] = explosionMaxRadius;
    j["sinkExplosionDuration"] = sinkExplosionDuration;
    j["sinkExplosionMaxRadius"] = sinkExplosionMaxRadius;

    // Wind
    j["windChangeInterval"] = windChangeInterval;
    j["windLerpSpeed"] = windLerpSpeed;
    j["windMaxDrift"] = windMaxDrift;
    j["windMinStrength"] = windMinStrength;
    j["windAngleChangeMax"] = windAngleChangeMax;
    j["windStrengthChangeMax"] = windStrengthChangeMax;

    // Collision
    j["collisionRestitution"] = collisionRestitution;
    j["collisionAngularFactor"] = collisionAngularFactor;
    j["collisionDamageScale"] = collisionDamageScale;
    j["wallBounceMultiplier"] = wallBounceMultiplier;

    // AI
    j["aiWanderInterval"] = aiWanderInterval;
    j["aiWanderMargin"] = aiWanderMargin;
    j["aiLookAheadTime"] = aiLookAheadTime;
    j["aiFireDistance"] = aiFireDistance;
    j["aiCrosshairTolerance"] = aiCrosshairTolerance;
    j["aiMinImpactForSound"] = aiMinImpactForSound;

    // Audio
    j["audioGunSilenceDuration"] = audioGunSilenceDuration;
    j["audioPitchVariation"] = audioPitchVariation;
    j["audioGainVariation"] = audioGainVariation;
    j["audioEngineBaseVolume"] = audioEngineBaseVolume;
    j["audioEngineThrottleBoost"] = audioEngineThrottleBoost;

    // Game Flow
    j["gameStartDelay"] = gameStartDelay;
    j["gameOverTextDelay"] = gameOverTextDelay;
    j["gameOverReturnDelay"] = gameOverReturnDelay;

    // Colors - Environment
    j["colorOcean"] = colorToJson (colorOcean);
    j["colorWaterHighlight1"] = colorToJson (colorWaterHighlight1);
    j["colorWaterHighlight2"] = colorToJson (colorWaterHighlight2);
    j["colorWaterHighlight3"] = colorToJson (colorWaterHighlight3);

    // Colors - Ships (FFA mode)
    j["colorShipRed"] = colorToJson (colorShipRed);
    j["colorShipBlue"] = colorToJson (colorShipBlue);
    j["colorShipGreen"] = colorToJson (colorShipGreen);
    j["colorShipYellow"] = colorToJson (colorShipYellow);

    // Colors - Ships (Team mode)
    j["colorTeam1"] = colorToJson (colorTeam1);
    j["colorTeam2"] = colorToJson (colorTeam2);

    // Colors - UI
    j["colorWhite"] = colorToJson (colorWhite);
    j["colorBlack"] = colorToJson (colorBlack);
    j["colorGrey"] = colorToJson (colorGrey);
    j["colorGreyDark"] = colorToJson (colorGreyDark);
    j["colorGreyMid"] = colorToJson (colorGreyMid);
    j["colorGreyLight"] = colorToJson (colorGreyLight);
    j["colorGreySubtle"] = colorToJson (colorGreySubtle);
    j["colorBarBackground"] = colorToJson (colorBarBackground);
    j["colorHudBackground"] = colorToJson (colorHudBackground);

    // Colors - Title Screen
    j["colorTitle"] = colorToJson (colorTitle);
    j["colorSubtitle"] = colorToJson (colorSubtitle);
    j["colorModeText"] = colorToJson (colorModeText);
    j["colorInstruction"] = colorToJson (colorInstruction);

    // Colors - Gameplay
    j["colorShell"] = colorToJson (colorShell);
    j["shellTrailLength"] = shellTrailLength;
    j["shellTrailSegments"] = shellTrailSegments;
    j["colorBubble"] = colorToJson (colorBubble);
    j["colorBarrel"] = colorToJson (colorBarrel);
    j["colorReloadReady"] = colorToJson (colorReloadReady);
    j["colorReloadNotReady"] = colorToJson (colorReloadNotReady);
    j["colorFiringRange"] = colorToJson (colorFiringRange);
    j["colorThrottleBar"] = colorToJson (colorThrottleBar);
    j["colorRudderBar"] = colorToJson (colorRudderBar);

    // Colors - Explosions
    j["colorExplosionOuter"] = colorToJson (colorExplosionOuter);
    j["colorExplosionMid"] = colorToJson (colorExplosionMid);
    j["colorExplosionCore"] = colorToJson (colorExplosionCore);
    j["colorSplashOuter"] = colorToJson (colorSplashOuter);
    j["colorSplashMid"] = colorToJson (colorSplashMid);
    j["colorSplashCore"] = colorToJson (colorSplashCore);

    // Colors - Wind Indicator
    j["colorWindBackground"] = colorToJson (colorWindBackground);
    j["colorWindBorder"] = colorToJson (colorWindBorder);
    j["colorWindArrow"] = colorToJson (colorWindArrow);

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
