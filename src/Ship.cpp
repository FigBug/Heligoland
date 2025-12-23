#include "Ship.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

Ship::Ship (int playerIndex_, Vec2 startPos, float startAngle)
    : playerIndex (playerIndex_), position (startPos), angle (startAngle), turrets { {
                                                                               Turret ({ length * 0.35f, 0.0f }, true), // Front
                                                                               Turret ({ length * 0.12f, 0.0f }, true), // Front-mid
                                                                               Turret ({ -length * 0.12f, 0.0f }, false), // Rear-mid
                                                                               Turret ({ -length * 0.35f, 0.0f }, false) // Rear
                                                                           } }
{
    // Start crosshair in front of ship
    crosshairOffset = Vec2::fromAngle (angle) * 150.0f;
}

void Ship::update (float dt, Vec2 moveInput, Vec2 aimInput, bool fireInput, float arenaWidth, float arenaHeight)
{
    // Update fire timer
    if (fireTimer > 0)
    {
        fireTimer -= dt;
    }

    // Fire if requested and ready
    if (fireInput && fireTimer <= 0)
    {
        fireShells();
        fireTimer = fireInterval;
    }

    // Y-axis adjusts throttle (forward/back on stick increases/decreases)
    float throttleInput = -moveInput.y; // Negative because stick up is negative
    if (std::abs (throttleInput) > 0.1f)
    {
        throttle += throttleInput * throttleRate * dt;
        throttle = std::clamp (throttle, -1.0f, 1.0f);
    }

    // X-axis controls rudder
    float rudderInput = moveInput.x;
    if (std::abs (rudderInput) > 0.1f)
    {
        // Apply rudder input
        rudder += rudderInput * rudderRate * dt;
        rudder = std::clamp (rudder, -1.0f, 1.0f);
    }
    else
    {
        // Return rudder to center
        if (rudder > 0)
        {
            rudder -= rudderReturn * dt;
            if (rudder < 0)
                rudder = 0;
        }
        else if (rudder < 0)
        {
            rudder += rudderReturn * dt;
            if (rudder > 0)
                rudder = 0;
        }
    }

    // Apply throttle to velocity
    Vec2 forward = Vec2::fromAngle (angle);
    float targetSpeed = throttle * maxSpeed;
    float currentSpeed = velocity.length();

    // Gradually adjust speed toward target
    if (throttle > 0)
    {
        velocity = forward * std::min (currentSpeed + throttle * maxSpeed * 0.5f * dt, targetSpeed);
    }
    else if (throttle < 0)
    {
        velocity = forward * std::max (currentSpeed + throttle * maxSpeed * 0.5f * dt, targetSpeed);
    }
    else
    {
        // Coast - apply drag
        velocity *= 0.995f;
    }

    // Apply rudder to turning (only when moving)
    float speed = velocity.length();
    if (speed > 0.5f)
    {
        // Turn rate scales with speed - can't turn when stationary
        float turnFactor = std::min (speed / maxSpeed, 1.0f);
        angularVelocity = rudder * 1.5f * turnFactor;
    }
    else
    {
        angularVelocity = 0.0f;
    }

    // Apply turning
    angle += angularVelocity * dt;

    // Keep angle in [-PI, PI]
    while (angle > M_PI)
        angle -= 2.0f * M_PI;
    while (angle < -M_PI)
        angle += 2.0f * M_PI;

    // Update position
    position += velocity * dt;

    // Clamp to arena
    clampToArena (arenaWidth, arenaHeight);

    // Update crosshair offset based on aim stick (moves in X/Y)
    if (aimInput.lengthSquared() > 0.01f)
    {
        crosshairOffset += aimInput * crosshairSpeed * dt;

        // Clamp distance from ship
        float dist = crosshairOffset.length();
        if (dist < minCrosshairDist)
        {
            crosshairOffset = crosshairOffset.normalized() * minCrosshairDist;
        }
        else if (dist > maxCrosshairDist)
        {
            crosshairOffset = crosshairOffset.normalized() * maxCrosshairDist;
        }
    }

    // Update turrets to aim at crosshair from their individual positions
    Vec2 crosshairWorldPos = position + crosshairOffset;
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    for (auto& turret : turrets)
    {
        Vec2 localOffset = turret.getLocalOffset();
        // Calculate turret world position
        Vec2 turretWorldPos;
        turretWorldPos.x = position.x + localOffset.x * cosA - localOffset.y * sinA;
        turretWorldPos.y = position.y + localOffset.x * sinA + localOffset.y * cosA;

        // Direction from this turret to crosshair
        Vec2 aimDir = (crosshairWorldPos - turretWorldPos).normalized();
        turret.update (dt, angle, aimDir);
    }

    // Update bubble trail
    updateBubbles (dt);
}

void Ship::clampToArena (float arenaWidth, float arenaHeight)
{
    float halfLength = length / 2.0f;
    float halfWidth = width / 2.0f;
    float margin = std::max (halfLength, halfWidth);

    bool hitEdge = false;

    if (position.x < margin)
    {
        position.x = margin;
        velocity.x = std::abs (velocity.x) * 0.3f; // Bounce slightly
        hitEdge = true;
    }
    else if (position.x > arenaWidth - margin)
    {
        position.x = arenaWidth - margin;
        velocity.x = -std::abs (velocity.x) * 0.3f;
        hitEdge = true;
    }

    if (position.y < margin)
    {
        position.y = margin;
        velocity.y = std::abs (velocity.y) * 0.3f;
        hitEdge = true;
    }
    else if (position.y > arenaHeight - margin)
    {
        position.y = arenaHeight - margin;
        velocity.y = -std::abs (velocity.y) * 0.3f;
        hitEdge = true;
    }
}

SDL_Color Ship::getColor() const
{
    switch (playerIndex)
    {
        case 0:
            return { 255, 100, 100, 255 }; // Red
        case 1:
            return { 100, 100, 255, 255 }; // Blue
        case 2:
            return { 100, 255, 100, 255 }; // Green
        case 3:
            return { 255, 255, 100, 255 }; // Yellow
        default:
            return { 200, 200, 200, 255 }; // Gray
    }
}

void Ship::takeDamage (float damage)
{
    health -= damage;
    if (health < 0)
    {
        health = 0;
    }
}

void Ship::stopAndPushApart (Vec2 pushDirection, float pushDistance)
{
    velocity = { 0, 0 };
    position += pushDirection * pushDistance;
}

void Ship::fullStop()
{
    velocity = { 0, 0 };
    throttle = 0.0f;
}

std::array<Vec2, 4> Ship::getCorners() const
{
    float halfLength = length / 2.0f;
    float halfWidth = width / 2.0f;

    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    // Local corners (unrotated)
    Vec2 corners[4] = {
        { -halfLength, -halfWidth }, // Back-left
        { halfLength, -halfWidth }, // Front-left
        { halfLength, halfWidth }, // Front-right
        { -halfLength, halfWidth } // Back-right
    };

    // Rotate and translate to world coordinates
    std::array<Vec2, 4> worldCorners;
    for (int i = 0; i < 4; ++i)
    {
        worldCorners[i].x = position.x + corners[i].x * cosA - corners[i].y * sinA;
        worldCorners[i].y = position.y + corners[i].x * sinA + corners[i].y * cosA;
    }

    return worldCorners;
}

void Ship::fireShells()
{
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);
    float targetRange = crosshairOffset.length();
    float shellSpeed = maxSpeed * 2.0f; // Shells travel at 2x boat speed

    for (const auto& turret : turrets)
    {
        Vec2 localOffset = turret.getLocalOffset();

        // Rotate local offset by ship angle to get world position
        Vec2 worldOffset;
        worldOffset.x = localOffset.x * cosA - localOffset.y * sinA;
        worldOffset.y = localOffset.x * sinA + localOffset.y * cosA;

        Vec2 turretPos = position + worldOffset;

        // Shell fires in direction turret is facing (world angle)
        Vec2 shellVel = Vec2::fromAngle (turret.getWorldAngle (angle)) * shellSpeed;

        pendingShells.push_back (Shell (turretPos, shellVel, playerIndex, targetRange));
    }
}

float Ship::getCrosshairDistance() const
{
    return crosshairOffset.length();
}

bool Ship::isReadyToFire() const
{
    // Check if reloaded
    if (fireTimer > 0)
    {
        return false;
    }

    // Check if all turrets are on target
    for (const auto& turret : turrets)
    {
        if (! turret.isOnTarget())
        {
            return false;
        }
    }

    return true;
}

void Ship::updateBubbles (float dt)
{
    float speed = velocity.length();
    float fadeTime = 10.0f; // Bubbles fade over 10 seconds
    float fadeRate = 1.0f / fadeTime;

    // Fade and remove old bubbles
    for (auto it = bubbles.begin(); it != bubbles.end();)
    {
        it->alpha -= fadeRate * dt;
        if (it->alpha <= 0.0f)
        {
            it = bubbles.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Spawn new bubbles at the rear of the ship when moving
    if (speed > 5.0f)
    {
        bubbleSpawnTimer += dt;

        // Spawn rate increases with speed
        float spawnRate = bubbleSpawnInterval * (50.0f / speed);

        while (bubbleSpawnTimer >= spawnRate)
        {
            bubbleSpawnTimer -= spawnRate;

            // Spawn position at rear of ship with some randomness
            Vec2 backward = Vec2::fromAngle (angle + M_PI);
            Vec2 spawnPos = position + backward * (length * 0.5f);

            // Add some random offset perpendicular to ship direction
            float perpOffset = ((float) rand() / RAND_MAX - 0.5f) * width * 0.8f;
            Vec2 perp = Vec2::fromAngle (angle + M_PI * 0.5f);
            spawnPos += perp * perpOffset;

            // Random bubble size
            float bubbleRadius = 1.5f + ((float) rand() / RAND_MAX) * 2.0f;

            bubbles.push_back ({ spawnPos, bubbleRadius, 1.0f });
        }
    }
}
