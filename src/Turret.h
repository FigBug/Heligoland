#pragma once

#include "Config.h"
#include "Vec2.h"

class Turret
{
public:
    Turret (Vec2 localOffset, bool isFrontTurret = true);

    void update (float dt, float shipAngle, Vec2 targetDir);
    void setTargetAngle (float angle);

    Vec2 getLocalOffset() const { return localOffset; }
    float getAngle() const { return angle; } // Ship-relative angle
    float getWorldAngle (float shipAngle) const { return angle + shipAngle; } // World angle
    float getRadius() const { return config.turretRadius; }
    float getBarrelLength() const { return config.turretBarrelLength; }
    bool isOnTarget() const; // Returns true if turret is aimed at target OR at arc limit
    bool isAimedAtTarget() const; // Returns true only if actually aimed at target (not at arc limit)

private:
    Vec2 localOffset; // Position relative to ship center
    float angle = 0.0f; // Current turret rotation relative to ship (radians)
    float targetAngle = 0.0f; // Clamped to arc
    float desiredAngle = 0.0f; // Original unclamped desired angle
    bool isFront = true; // Front turrets can't point backward, rear can't point forward

    float clampAngleToArc (float desiredAngle) const;
    bool isAtArcLimit() const; // Returns true if turret is at rotation limit
};
