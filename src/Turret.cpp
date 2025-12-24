#include "Turret.h"
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
        angle = M_PI;
        targetAngle = M_PI;
    }
}

float Turret::clampAngleToArc (float desiredAngle) const
{
    // Angles are relative to ship: 0 = ship forward, PI/-PI = ship backward
    // Front turrets: can aim ±135 degrees from forward (0)
    // Rear turrets: can aim ±135 degrees from backward (PI)

    // Normalize to [-PI, PI]
    while (desiredAngle > M_PI)
        desiredAngle -= 2.0f * M_PI;
    while (desiredAngle < -M_PI)
        desiredAngle += 2.0f * M_PI;

    float arcSize = M_PI * 0.75f; // 135 degrees each direction

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
        float limit = M_PI - arcSize; // 45 degrees

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
        while (relativeAngle > M_PI)
            relativeAngle -= 2.0f * M_PI;
        while (relativeAngle < -M_PI)
            relativeAngle += 2.0f * M_PI;

        targetAngle = clampAngleToArc (relativeAngle);
    }

    // Smoothly rotate toward target angle
    float angleDiff = targetAngle - angle;

    // Normalize angle difference to [-PI, PI]
    while (angleDiff > M_PI)
        angleDiff -= 2.0f * M_PI;
    while (angleDiff < -M_PI)
        angleDiff += 2.0f * M_PI;

    float maxRotation = rotationSpeed * dt;

    // Check if the short path crosses the forbidden zone
    // If so, force rotation the long way around
    float arcSize = M_PI * 0.75f; // 135 degrees

    // Test if rotating in the short direction would hit the forbidden zone
    float testStep = maxRotation * (angleDiff > 0 ? 1.0f : -1.0f);
    float testAngle = angle + testStep;

    // Normalize test angle
    while (testAngle > M_PI)
        testAngle -= 2.0f * M_PI;
    while (testAngle < -M_PI)
        testAngle += 2.0f * M_PI;

    bool wouldHitLimit = false;

    if (isFront)
    {
        // Front turrets: forbidden zone is beyond ±135°
        wouldHitLimit = (std::abs (testAngle) > arcSize) && (std::abs (angle) <= arcSize);
    }
    else
    {
        // Rear turrets: forbidden zone is near 0 (within ±45°)
        float limit = M_PI - arcSize; // 45 degrees
        wouldHitLimit = (std::abs (testAngle) < limit) && (std::abs (angle) >= limit);
    }

    // If short path is blocked, go the long way
    if (wouldHitLimit)
    {
        angleDiff = -angleDiff;
        // Use the sign of the long way
        if (std::abs (angleDiff) < M_PI)
        {
            angleDiff = (angleDiff > 0 ? 1.0f : -1.0f) * (2.0f * M_PI - std::abs (angleDiff));
        }
    }
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
    while (angle > M_PI)
        angle -= 2.0f * M_PI;
    while (angle < -M_PI)
        angle += 2.0f * M_PI;
}

void Turret::setTargetAngle (float angle_)
{
    targetAngle = clampAngleToArc (angle_);
}

bool Turret::isAimedAtTarget() const
{
    float angleDiff = targetAngle - angle;
    // Normalize to [-PI, PI]
    while (angleDiff > M_PI)
        angleDiff -= 2.0f * M_PI;
    while (angleDiff < -M_PI)
        angleDiff += 2.0f * M_PI;

    // Consider on target if within ~5 degrees
    return std::abs (angleDiff) < 0.09f;
}

bool Turret::isOnTarget() const
{
    // On target if actually aimed OR at arc limit (can't rotate further)
    return isAimedAtTarget() || isAtArcLimit();
}

bool Turret::isAtArcLimit() const
{
    float arcSize = M_PI * 0.75f; // 135 degrees
    float tolerance = 0.05f;

    if (isFront)
    {
        // Front turrets limited to ±135 degrees from forward
        return std::abs (angle - arcSize) < tolerance || std::abs (angle + arcSize) < tolerance;
    }
    else
    {
        // Rear turrets limited to ±45 degrees from forward (can't go past)
        float minAngle = M_PI - arcSize; // 45 degrees
        return std::abs (angle - minAngle) < tolerance || std::abs (angle + minAngle) < tolerance;
    }
}
