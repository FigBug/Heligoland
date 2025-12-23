#pragma once

#include "Vec2.h"

class Turret {
public:
    Turret(Vec2 localOffset);

    void update(float dt, Vec2 targetDir);
    void setTargetAngle(float angle);

    Vec2 getLocalOffset() const { return localOffset; }
    float getAngle() const { return angle; }
    float getRadius() const { return radius; }
    float getBarrelLength() const { return barrelLength; }

private:
    Vec2 localOffset;      // Position relative to ship center
    float angle = 0.0f;    // Current turret rotation (radians)
    float targetAngle = 0.0f;
    float rotationSpeed = 3.0f;  // Radians per second
    float radius = 12.0f;
    float barrelLength = 20.0f;
};
