#include "Ship.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

Ship::Ship (int playerIndex_, Vec2 startPos, float startAngle, int team_)
    : playerIndex (playerIndex_), team (team_), position (startPos), angle (startAngle), turrets { {
                                                                                                     Turret ({ length * 0.35f, 0.0f }, true), // Front
                                                                                                     Turret ({ length * 0.12f, 0.0f }, true), // Front-mid
                                                                                                     Turret ({ -length * 0.12f, 0.0f }, false), // Rear-mid
                                                                                                     Turret ({ -length * 0.35f, 0.0f }, false) // Rear
                                                                                                 } }
{
    // Start crosshair in front of ship
    crosshairOffset = Vec2::fromAngle (angle) * Config::crosshairStartDistance;
}

void Ship::update (float dt, Vec2 moveInput, Vec2 aimInput, bool fireInput, float arenaWidth, float arenaHeight, Vec2 wind)
{
    // Handle sinking
    if (sinking)
    {
        sinkTimer += dt;
        if (sinkTimer > Config::shipSinkDuration)
            sinkTimer = Config::shipSinkDuration; // Cap to prevent alpha wraparound

        // Slow down while sinking
        velocity *= Config::shipSinkVelocityDecay;
        angularVelocity *= Config::shipSinkAngularDecay;

        // Still update smoke and bubbles while sinking
        updateSmoke (dt, wind);
        updateBubbles (dt);
        return; // Don't process any other input while sinking
    }

    // Calculate damage penalty (up to configured reduction in speed and turning)
    float damagePercent = getDamagePercent();
    float damagePenalty = 1.0f - (damagePercent * Config::shipDamagePenaltyMax);

    // Update fire timer
    if (fireTimer > 0)
    {
        fireTimer -= dt;
    }

    // Fire if requested and ready
    if (fireInput && fireTimer <= 0)
    {
        fireShells();
        fireTimer = Config::fireInterval;
    }

    // Y-axis adjusts throttle (forward/back on stick increases/decreases)
    float throttleInput = -moveInput.y; // Negative because stick up is negative
    if (std::abs (throttleInput) > 0.1f)
    {
        throttle += throttleInput * Config::shipThrottleRate * dt;
        throttle = std::clamp (throttle, -1.0f, 1.0f);
    }

    // X-axis controls rudder
    float rudderInput = moveInput.x;
    if (std::abs (rudderInput) > 0.1f)
    {
        // Apply rudder input
        rudder += rudderInput * Config::shipRudderRate * dt;
        rudder = std::clamp (rudder, -1.0f, 1.0f);
    }
    else
    {
        // Return rudder to center
        if (rudder > 0)
        {
            rudder -= Config::shipRudderReturn * dt;
            if (rudder < 0)
                rudder = 0;
        }
        else if (rudder < 0)
        {
            rudder += Config::shipRudderReturn * dt;
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
        velocity *= Config::shipDragCoefficient;
    }

    // Apply rudder to turning (only when moving, reduced by damage)
    float speed = velocity.length();
    if (speed > 0.5f)
    {
        // Turn rate based on minimum turning radius
        // radius = speed / angularVelocity, so angularVelocity = speed / radius
        float minTurnRadius = length * Config::shipMinTurnRadiusMultiplier / damagePenalty; // Damaged ships turn wider
        angularVelocity = rudder * speed / minTurnRadius;
    }
    else
    {
        angularVelocity = 0.0f;
    }

    // Apply turning
    angle += angularVelocity * dt;

    // Keep angle in [-PI, PI]
    while (angle > pi)
        angle -= 2.0f * pi;
    while (angle < -pi)
        angle += 2.0f * pi;

    // Update position
    position += velocity * dt;

    // Clamp to arena
    clampToArena (arenaWidth, arenaHeight);

    // Update crosshair offset based on aim stick (moves in X/Y)
    if (aimInput.lengthSquared() > 0.01f)
    {
        crosshairOffset += aimInput * Config::crosshairSpeed * dt;
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
    if (crosshairDist > Config::maxCrosshairDistance)
    {
        crosshairOffset = crosshairOffset.normalized() * Config::maxCrosshairDistance;
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

Color Ship::getColor() const
{
    if (team >= 0)
    {
        // Team mode: alternate between dark and light shades
        if (team == 0)
            return (playerIndex % 2 == 0) ? Config::colorTeam1Dark : Config::colorTeam1Light;
        else
            return (playerIndex % 2 == 0) ? Config::colorTeam2Dark : Config::colorTeam2Light;
    }
    else
    {
        // FFA: distinct colors for each player
        switch (playerIndex)
        {
            case 0:
                return Config::colorShipRed;
            case 1:
                return Config::colorShipBlue;
            case 2:
                return Config::colorShipGreen;
            case 3:
                return Config::colorShipYellow;
            default:
                return Config::colorGrey;
        }
    }
}

void Ship::takeDamage (float damage)
{
    if (sinking)
        return; // Can't take more damage while sinking

    health -= damage;
    if (health <= 0)
    {
        health = 0;
        sinking = true;
        sinkTimer = 0.0f;
    }
}

void Ship::applyCollision (Vec2 pushDirection, float pushDistance, Vec2 myVel, Vec2 otherVel)
{
    // Push apart to resolve overlap
    position += pushDirection * pushDistance;

    // Calculate collision response using elastic collision formula
    // For equal mass objects: v1' = v2 and v2' = v1 along collision normal
    // We'll use a coefficient of restitution for energy loss

    Vec2 normal = pushDirection.normalized();
    float restitution = Config::collisionRestitution;

    // Relative velocity along collision normal
    float relVelNormal = (myVel - otherVel).dot (normal);

    // Only resolve if objects are moving toward each other
    if (relVelNormal < 0)
    {
        // For equal masses, the impulse is simpler
        float impulse = -(1.0f + restitution) * relVelNormal * 0.5f;

        // Apply impulse to velocity
        velocity = myVel + normal * impulse;

        // Add some rotation based on where the hit occurred
        // This is simplified - just add some angular velocity
        float lateralComponent = (myVel - otherVel).dot ({ -normal.y, normal.x });
        angularVelocity += lateralComponent * Config::collisionAngularFactor;
    }
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
    float targetRange = std::max (crosshairOffset.length(), Config::minShellRange);
    float shellSpeed = maxSpeed * Config::shellSpeedMultiplier;

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

void Ship::setCrosshairPosition (Vec2 worldPos)
{
    crosshairOffset = worldPos - position;

    // Clamp to max range
    float dist = crosshairOffset.length();
    if (dist > Config::maxCrosshairDistance)
    {
        crosshairOffset = crosshairOffset.normalized() * Config::maxCrosshairDistance;
    }
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
    float fadeRate = 1.0f / Config::bubbleFadeTime;

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
    if (speed > Config::bubbleMinSpeed)
    {
        bubbleSpawnTimer += dt;

        // Spawn rate increases with speed
        float spawnRate = Config::bubbleSpawnInterval * (50.0f / speed);

        while (bubbleSpawnTimer >= spawnRate)
        {
            bubbleSpawnTimer -= spawnRate;

            // Spawn position at rear of ship with some randomness
            Vec2 backward = Vec2::fromAngle (angle + pi);
            Vec2 spawnPos = position + backward * (length * 0.5f);

            // Add some random offset perpendicular to ship direction
            float perpOffset = ((float) rand() / RAND_MAX - 0.5f) * width * 0.8f;
            Vec2 perp = Vec2::fromAngle (angle + pi * 0.5f);
            spawnPos += perp * perpOffset;

            // Random bubble size
            float bubbleRadius = Config::bubbleMinRadius + ((float) rand() / RAND_MAX) * Config::bubbleRadiusVariation;

            bubbles.push_back ({ spawnPos, bubbleRadius, 1.0f });
        }
    }
}

void Ship::updateSmoke (float dt, Vec2 wind)
{
    float fadeRate = 1.0f / Config::smokeFadeTime;

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

        it->position += dispersedWind * Config::smokeWindStrength * dt;

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
    // Sinking ships produce less and less smoke
    float damagePercent = getDamagePercent();
    float sinkFactor = sinking ? (1.0f - getSinkProgress()) : 1.0f;

    if (sinkFactor <= 0.0f)
        return; // No more smoke when fully sunk

    smokeSpawnTimer += dt;

    // Base spawn interval for engine smoke, faster with damage
    float spawnInterval = Config::smokeBaseSpawnInterval / ((1.0f + damagePercent * Config::smokeDamageMultiplier) * sinkFactor);

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
        float baseRadius = Config::smokeBaseRadius + damagePercent * 2.0f;
        float smokeRadius = baseRadius + ((float) rand() / RAND_MAX) * 1.5f;

        // Lower starting alpha for thinner smoke, reduce further when sinking
        float startAlpha = (Config::smokeBaseAlpha + damagePercent * 0.4f) * sinkFactor;

        // Random wind angle offset
        float windAngleOffset = ((float) rand() / RAND_MAX - 0.5f) * Config::smokeWindAngleVariation;

        smoke.push_back ({ spawnPos, smokeRadius, startAlpha, windAngleOffset });
    }
}
