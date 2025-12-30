#include "Turret.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

Turret::Turret (Vec2 localOffset_, bool isFrontTurret)
    : localOffset (localOffset_), isFront (isFrontTurret)
{
    // Angle is relative to ship: 0 = ship forward, PI = ship backward
    // Front turrets start facing forward, rear turrets start facing backward
    if (isFront)
    {
        angle = 0.0f;
        targetAngle = 0.0f;
    }
    else
    {
        angle = pi;
        targetAngle = pi;
    }
}

float Turret::clampAngleToArc (float desiredAngle) const
{
    // Angles are relative to ship: 0 = ship forward, PI/-PI = ship backward
    // Front turrets: can aim ±135 degrees from forward (0)
    // Rear turrets: can aim ±135 degrees from backward (PI)

    // Normalize to [-PI, PI]
    while (desiredAngle > pi)
        desiredAngle -= 2.0f * pi;
    while (desiredAngle < -pi)
        desiredAngle += 2.0f * pi;

    float arcSize = pi * config.turretArcSize;

    if (isFront)
    {
        // Front turrets: valid range is [-135°, +135°] from ship forward
        if (desiredAngle > arcSize)
        {
            desiredAngle = arcSize;
        }
        else if (desiredAngle < -arcSize)
        {
            desiredAngle = -arcSize;
        }
    }
    else
    {
        // Rear turrets: valid range is around PI (backward)
        // Can aim from 45° to 180° and from -180° to -45°
        float limit = pi - arcSize; // 45 degrees

        // If angle is in the forbidden forward zone
        if (desiredAngle > -limit && desiredAngle < limit)
        {
            // Clamp to nearest allowed edge
            if (desiredAngle >= 0)
            {
                desiredAngle = limit;
            }
            else
            {
                desiredAngle = -limit;
            }
        }
    }

    return desiredAngle;
}

void Turret::update (float dt, float shipAngle, Vec2 targetDir)
{
    if (targetDir.lengthSquared() > 0.01f)
    {
        // Convert world-space target direction to ship-relative angle
        float worldTargetAngle = targetDir.toAngle();
        float relativeAngle = worldTargetAngle - shipAngle;

        // Normalize to [-PI, PI]
        while (relativeAngle > pi)
            relativeAngle -= 2.0f * pi;
        while (relativeAngle < -pi)
            relativeAngle += 2.0f * pi;

        desiredAngle = relativeAngle;
        targetAngle = clampAngleToArc (relativeAngle);
    }

    float maxRotation = config.turretRotationSpeed * dt;
    float arcSize = pi * config.turretArcSize;
    float limit = pi - arcSize; // The forbidden zone boundary

    // Determine which direction to rotate
    // We need to check if the shortest path crosses the forbidden zone

    float cwDist, ccwDist; // clockwise and counter-clockwise distances

    // Calculate distance going clockwise (negative direction)
    if (targetAngle <= angle)
        cwDist = angle - targetAngle;
    else
        cwDist = angle + 2.0f * pi - targetAngle;

    // Calculate distance going counter-clockwise (positive direction)
    ccwDist = 2.0f * pi - cwDist;

    // Check if each path crosses the forbidden zone
    auto pathCrossesForbidden = [&] (float start, float dist, bool clockwise) -> bool
    {
        // Sample points along the path to check if any are in forbidden zone
        int steps = std::max (2, (int) (dist / 0.1f));
        for (int i = 1; i < steps; ++i)
        {
            float t = (float) i / steps;
            float testAngle = start + (clockwise ? -1.0f : 1.0f) * dist * t;

            // Normalize
            while (testAngle > pi)
                testAngle -= 2.0f * pi;
            while (testAngle < -pi)
                testAngle += 2.0f * pi;

            // Check if in forbidden zone
            if (isFront)
            {
                if (std::abs (testAngle) > arcSize)
                    return true;
            }
            else
            {
                if (std::abs (testAngle) < limit)
                    return true;
            }
        }
        return false;
    };

    bool cwBlocked = pathCrossesForbidden (angle, cwDist, true);
    bool ccwBlocked = pathCrossesForbidden (angle, ccwDist, false);

    float angleDiff;
    if (cwBlocked && ! ccwBlocked)
    {
        // Must go counter-clockwise
        angleDiff = ccwDist;
    }
    else if (ccwBlocked && ! cwBlocked)
    {
        // Must go clockwise
        angleDiff = -cwDist;
    }
    else
    {
        // Neither blocked (or both blocked - shouldn't happen), take shortest
        if (cwDist <= ccwDist)
            angleDiff = -cwDist;
        else
            angleDiff = ccwDist;
    }

    // Apply rotation
    if (std::abs (angleDiff) < maxRotation)
    {
        angle = targetAngle;
    }
    else if (angleDiff > 0)
    {
        angle += maxRotation;
    }
    else
    {
        angle -= maxRotation;
    }

    // Keep angle in [-PI, PI]
    while (angle > pi)
        angle -= 2.0f * pi;
    while (angle < -pi)
        angle += 2.0f * pi;
}

void Turret::setTargetAngle (float angle_)
{
    targetAngle = clampAngleToArc (angle_);
}

bool Turret::isAimedAtTarget() const
{
    // Check if desired angle is actually reachable (not clamped)
    float clampedDesired = clampAngleToArc (desiredAngle);
    float desiredDiff = clampedDesired - desiredAngle;
    while (desiredDiff > pi)
        desiredDiff -= 2.0f * pi;
    while (desiredDiff < -pi)
        desiredDiff += 2.0f * pi;

    // If desired angle was clamped, target is not reachable
    if (std::abs (desiredDiff) > 0.01f)
        return false;

    // Check if turret is aimed at the target
    float angleDiff = targetAngle - angle;
    // Normalize to [-PI, PI]
    while (angleDiff > pi)
        angleDiff -= 2.0f * pi;
    while (angleDiff < -pi)
        angleDiff += 2.0f * pi;

    return std::abs (angleDiff) < config.turretOnTargetTolerance;
}

bool Turret::isOnTarget() const
{
    // On target if actually aimed OR at arc limit (can't rotate further)
    return isAimedAtTarget() || isAtArcLimit();
}

bool Turret::isAtArcLimit() const
{
    float arcSize = pi * config.turretArcSize;
    float tolerance = 0.05f;

    if (isFront)
    {
        // Front turrets limited to ±135 degrees from forward
        return std::abs (angle - arcSize) < tolerance || std::abs (angle + arcSize) < tolerance;
    }
    else
    {
        // Rear turrets limited to ±45 degrees from forward (can't go past)
        float minAngle = pi - arcSize; // 45 degrees
        return std::abs (angle - minAngle) < tolerance || std::abs (angle + minAngle) < tolerance;
    }
}
