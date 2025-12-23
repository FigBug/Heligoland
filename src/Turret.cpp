#include "Turret.h"
#include <cmath>

Turret::Turret(Vec2 localOffset_)
    : localOffset(localOffset_) {
}

void Turret::update(float dt, Vec2 targetDir) {
    if (targetDir.lengthSquared() > 0.01f) {
        targetAngle = targetDir.toAngle();
    }

    // Smoothly rotate toward target angle
    float angleDiff = targetAngle - angle;

    // Normalize angle difference to [-PI, PI]
    while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
    while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;

    float maxRotation = rotationSpeed * dt;
    if (std::abs(angleDiff) < maxRotation) {
        angle = targetAngle;
    } else if (angleDiff > 0) {
        angle += maxRotation;
    } else {
        angle -= maxRotation;
    }

    // Keep angle in [-PI, PI]
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
}

void Turret::setTargetAngle(float angle_) {
    targetAngle = angle_;
}
