#include "Ship.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>

Ship::Ship(int playerIndex_, Vec2 startPos, float startAngle)
    : playerIndex(playerIndex_)
    , position(startPos)
    , angle(startAngle)
    , turrets{{
        Turret({-width * 0.25f, 0.0f}),   // Front-left
        Turret({width * 0.25f, 0.0f}),    // Front-right
        Turret({-width * 0.25f, 0.0f}),   // Rear-left (will be mirrored)
        Turret({width * 0.25f, 0.0f})     // Rear-right
    }}
{
    // Adjust turret positions for a 4-turret layout (2 fore, 2 aft)
    turrets[0] = Turret({width * 0.3f, -height * 0.15f});   // Front-right
    turrets[1] = Turret({width * 0.3f, height * 0.15f});    // Front-left
    turrets[2] = Turret({-width * 0.3f, -height * 0.15f});  // Rear-right
    turrets[3] = Turret({-width * 0.3f, height * 0.15f});   // Rear-left

    crosshairPos = position + Vec2::fromAngle(angle) * crosshairDistance;
}

void Ship::update(float dt, Vec2 moveInput, Vec2 aimInput, float arenaWidth, float arenaHeight) {
    // Movement stick controls thrust and turning
    if (moveInput.lengthSquared() > 0.01f) {
        // Y-axis (forward on stick) controls thrust
        float thrust = -moveInput.y;  // Negative because stick up is negative
        Vec2 forward = Vec2::fromAngle(angle);
        velocity += forward * thrust * acceleration * dt;

        // X-axis controls turning
        angularVelocity = moveInput.x * turnSpeed;
    } else {
        angularVelocity *= 0.9f;  // Slow down turning when stick released
    }

    // Apply turning
    angle += angularVelocity * dt;

    // Keep angle in [-PI, PI]
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;

    // Apply drag
    velocity *= drag;

    // Clamp speed
    float speed = velocity.length();
    if (speed > maxSpeed) {
        velocity = velocity.normalized() * maxSpeed;
    }

    // Update position
    position += velocity * dt;

    // Clamp to arena
    clampToArena(arenaWidth, arenaHeight);

    // Update crosshair position based on aim stick
    if (aimInput.lengthSquared() > 0.01f) {
        crosshairPos = position + aimInput.normalized() * crosshairDistance;
    } else {
        // Default crosshair to forward direction
        crosshairPos = position + Vec2::fromAngle(angle) * crosshairDistance;
    }

    // Update turrets to aim at crosshair
    Vec2 aimDir = (crosshairPos - position).normalized();
    for (auto& turret : turrets) {
        turret.update(dt, aimDir);
    }
}

void Ship::clampToArena(float arenaWidth, float arenaHeight) {
    float halfWidth = width / 2.0f;
    float halfHeight = height / 2.0f;
    float margin = std::max(halfWidth, halfHeight);

    bool hitEdge = false;

    if (position.x < margin) {
        position.x = margin;
        velocity.x = std::abs(velocity.x) * 0.3f;  // Bounce slightly
        hitEdge = true;
    } else if (position.x > arenaWidth - margin) {
        position.x = arenaWidth - margin;
        velocity.x = -std::abs(velocity.x) * 0.3f;
        hitEdge = true;
    }

    if (position.y < margin) {
        position.y = margin;
        velocity.y = std::abs(velocity.y) * 0.3f;
        hitEdge = true;
    } else if (position.y > arenaHeight - margin) {
        position.y = arenaHeight - margin;
        velocity.y = -std::abs(velocity.y) * 0.3f;
        hitEdge = true;
    }
}

SDL_Color Ship::getColor() const {
    switch (playerIndex) {
        case 0: return {255, 100, 100, 255};  // Red
        case 1: return {100, 100, 255, 255};  // Blue
        case 2: return {100, 255, 100, 255};  // Green
        case 3: return {255, 255, 100, 255};  // Yellow
        default: return {200, 200, 200, 255}; // Gray
    }
}
