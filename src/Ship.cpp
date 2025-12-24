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

void Ship::update (float dt, Vec2 moveInput, Vec2 aimInput, bool fireInput, float arenaWidth, float arenaHeight, Vec2 wind)
{
    // Calculate damage penalty (up to 20% reduction in speed and turning)
    float damagePercent = getDamagePercent();
    float damagePenalty = 1.0f - (damagePercent * 0.2f);

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

    // Apply throttle to velocity (reduced by damage)
    Vec2 forward = Vec2::fromAngle (angle);
    float effectiveMaxSpeed = maxSpeed * damagePenalty;
    float targetSpeed = throttle * effectiveMaxSpeed;

    // Calculate current speed with sign (positive = forward, negative = backward)
    float currentSpeed = velocity.dot (forward);

    // Gradually adjust speed toward target
    if (throttle != 0)
    {
        float newSpeed = currentSpeed + (targetSpeed - currentSpeed) * 0.5f * dt;
        velocity = forward * newSpeed;
    }
    else
    {
        // Coast - apply drag
        velocity *= 0.995f;
    }

    // Apply rudder to turning (only when moving, reduced by damage)
    float speed = velocity.length();
    if (speed > 0.5f)
    {
        // Turn rate based on minimum turning radius of 2x boat length
        // radius = speed / angularVelocity, so angularVelocity = speed / radius
        float minTurnRadius = length * 2.0f / damagePenalty; // Damaged ships turn wider
        angularVelocity = rudder * speed / minTurnRadius;
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
    }

    // Clamp crosshair to stay on screen
    Vec2 crosshairWorldPos = position + crosshairOffset;
    float margin = 10.0f;
    if (crosshairWorldPos.x < margin)
        crosshairOffset.x = margin - position.x;
    else if (crosshairWorldPos.x > arenaWidth - margin)
        crosshairOffset.x = arenaWidth - margin - position.x;
    if (crosshairWorldPos.y < margin)
        crosshairOffset.y = margin - position.y;
    else if (crosshairWorldPos.y > arenaHeight - margin)
        crosshairOffset.y = arenaHeight - margin - position.y;

    // Clamp crosshair to max range
    float crosshairDist = crosshairOffset.length();
    if (crosshairDist > maxCrosshairDist)
    {
        crosshairOffset = crosshairOffset.normalized() * maxCrosshairDist;
    }

    // Update turrets to aim at crosshair from their individual positions
    crosshairWorldPos = position + crosshairOffset;
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

    // Update smoke
    updateSmoke (dt, wind);
}

void Ship::clampToArena (float arenaWidth, float arenaHeight)
{
    // Get actual corners of the rotated ship
    auto corners = getCorners();

    // Find how far each corner is outside the arena
    float pushLeft = 0.0f, pushRight = 0.0f, pushUp = 0.0f, pushDown = 0.0f;

    for (const auto& corner : corners)
    {
        if (corner.x < 0)
            pushLeft = std::max (pushLeft, -corner.x);
        if (corner.x > arenaWidth)
            pushRight = std::max (pushRight, corner.x - arenaWidth);
        if (corner.y < 0)
            pushUp = std::max (pushUp, -corner.y);
        if (corner.y > arenaHeight)
            pushDown = std::max (pushDown, corner.y - arenaHeight);
    }

    // Apply corrections
    if (pushLeft > 0)
    {
        position.x += pushLeft;
        velocity.x = std::abs (velocity.x) * 0.3f;
    }
    else if (pushRight > 0)
    {
        position.x -= pushRight;
        velocity.x = -std::abs (velocity.x) * 0.3f;
    }

    if (pushUp > 0)
    {
        position.y += pushUp;
        velocity.y = std::abs (velocity.y) * 0.3f;
    }
    else if (pushDown > 0)
    {
        position.y -= pushDown;
        velocity.y = -std::abs (velocity.y) * 0.3f;
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
    // Use minimum range if crosshair is too close
    float targetRange = std::max (crosshairOffset.length(), minShellRange);
    float shellSpeed = maxSpeed * 5.0f; // Shells travel at 5x boat speed

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

    // Check if crosshair is in valid range
    if (! isCrosshairInRange())
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

void Ship::updateSmoke (float dt, Vec2 wind)
{
    float fadeTime = 10.0f; // Smoke fades over 10 seconds
    float fadeRate = 1.0f / fadeTime;
    float windStrength = 10.0f; // How much wind affects smoke

    // Update existing smoke - fade and move with wind
    for (auto it = smoke.begin(); it != smoke.end();)
    {
        it->alpha -= fadeRate * dt;

        // Apply wind with this particle's fixed angle offset
        float cosR = std::cos (it->windAngleOffset);
        float sinR = std::sin (it->windAngleOffset);
        Vec2 dispersedWind;
        dispersedWind.x = wind.x * cosR - wind.y * sinR;
        dispersedWind.y = wind.x * sinR + wind.y * cosR;

        it->position += dispersedWind * windStrength * dt;

        if (it->alpha <= 0.0f)
        {
            it = smoke.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Spawn new smoke - all ships make some engine smoke, damaged ships make more
    float damagePercent = getDamagePercent();
    smokeSpawnTimer += dt;

    // Base spawn interval for engine smoke, faster with damage
    float baseSpawnInterval = 0.0333f; // Undamaged ships: smoke every ~33ms
    float spawnInterval = baseSpawnInterval / (1.0f + damagePercent * 4.0f);

    while (smokeSpawnTimer >= spawnInterval)
    {
        smokeSpawnTimer -= spawnInterval;

        // Spawn position depends on damage level
        Vec2 spawnPos;
        float cosA = std::cos (angle);
        float sinA = std::sin (angle);

        if (damagePercent < 0.3f)
        {
            // Light/no damage: smoke from engine (center)
            spawnPos = position;
        }
        else
        {
            // Heavy damage: smoke from random locations across ship
            float randomX = ((float) rand() / RAND_MAX - 0.5f) * length * 0.8f;
            float randomY = ((float) rand() / RAND_MAX - 0.5f) * width * 0.6f;
            spawnPos.x = position.x + randomX * cosA - randomY * sinA;
            spawnPos.y = position.y + randomX * sinA + randomY * cosA;
        }

        // Smoke size: small wisps for undamaged, bigger with damage
        float baseRadius = 1.5f + damagePercent * 2.0f;
        float smokeRadius = baseRadius + ((float) rand() / RAND_MAX) * 1.5f;

        // Lower starting alpha for thinner smoke
        float startAlpha = 0.4f + damagePercent * 0.4f; // 0.4 to 0.8 based on damage

        // Random wind angle offset +/- 3 degrees (0.052 radians)
        float windAngleOffset = ((float) rand() / RAND_MAX - 0.5f) * 0.105f;

        smoke.push_back ({ spawnPos, smokeRadius, startAlpha, windAngleOffset });
    }
}
