#include "AIController.h"
#include "Config.h"
#include "Ship.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

AIController::AIController()
{
    wanderTarget = { 0, 0 };
}

void AIController::update (float dt, Ship& myShip, const std::vector<const Ship*>& enemies, float arenaWidth, float arenaHeight)
{
    currentMode = determineMode (myShip, enemies);
    const Ship* target = findTarget (myShip, enemies);

    updateMovement (dt, myShip, enemies, arenaWidth, arenaHeight);
    updateAim (myShip, target);
}

AIMode AIController::determineMode (const Ship& myShip, const std::vector<const Ship*>& enemies)
{
    float myHealth = myShip.getHealth();
    float myMaxHealth = myShip.getMaxHealth();
    float myHealthPercent = myHealth / myMaxHealth;

    // Scared mode: health below 25%
    if (myHealthPercent < 0.25f)
        return AIMode::Scared;

    // Check if any enemy has much less health (below 30% of my health)
    for (const Ship* enemy : enemies)
    {
        float enemyHealthPercent = enemy->getHealth() / enemy->getMaxHealth();
        if (enemyHealthPercent < myHealthPercent * 0.5f)
            return AIMode::Aggressive;
    }

    return AIMode::Normal;
}

const Ship* AIController::findTarget (const Ship& myShip, const std::vector<const Ship*>& enemies)
{
    if (enemies.empty())
        return nullptr;

    Vec2 myPos = myShip.getPosition();

    // In aggressive mode, target the weakest enemy
    if (currentMode == AIMode::Aggressive)
    {
        const Ship* weakest = nullptr;
        float lowestHealth = 999999.0f;
        for (const Ship* enemy : enemies)
        {
            if (enemy->getHealth() < lowestHealth)
            {
                lowestHealth = enemy->getHealth();
                weakest = enemy;
            }
        }
        return weakest;
    }

    // Otherwise target the nearest enemy
    const Ship* nearest = nullptr;
    float nearestDist = 999999.0f;
    for (const Ship* enemy : enemies)
    {
        float dist = (enemy->getPosition() - myPos).length();
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = enemy;
        }
    }
    return nearest;
}

void AIController::updateMovement (float dt, const Ship& myShip, const std::vector<const Ship*>& enemies, float arenaWidth, float arenaHeight)
{
    Vec2 myPos = myShip.getPosition();
    Vec2 vel = myShip.getVelocity();
    float speed = myShip.getSpeed();
    float shipAngle = myShip.getAngle();
    float shipLength = myShip.getLength();

    // Check if crashed into edge
    float margin = shipLength;
    bool nearEdge = myPos.x < margin || myPos.x > arenaWidth - margin ||
                    myPos.y < margin || myPos.y > arenaHeight - margin;
    bool stopped = speed < 0.5f && std::abs (myShip.getThrottle()) > 0.1f;

    if (nearEdge || stopped)
    {
        // Already crashed - reverse and turn away
        moveInput.y = 0.5f;
        moveInput.x = (rand() % 2 == 0) ? 1.0f : -1.0f;
        wanderTarget = { arenaWidth / 2.0f, arenaHeight / 2.0f };
        wanderTimer = 2.0f;
        return;
    }

    Vec2 desiredDir = { 0, 0 };
    float desiredSpeed = 0.5f;

    if (enemies.empty())
    {
        // No enemies - just wander
        wanderTimer -= dt;
        if (wanderTimer <= 0.0f)
        {
            float wanderMargin = Config::aiWanderMargin;
            wanderTarget.x = wanderMargin + (rand() / (float) RAND_MAX) * (arenaWidth - 2 * wanderMargin);
            wanderTarget.y = wanderMargin + (rand() / (float) RAND_MAX) * (arenaHeight - 2 * wanderMargin);
            wanderTimer = wanderInterval + (rand() / (float) RAND_MAX) * 2.0f;
        }
        desiredDir = (wanderTarget - myPos).normalized();
    }
    else if (currentMode == AIMode::Scared)
    {
        // Run away from all enemies
        Vec2 fleeDir = { 0, 0 };
        for (const Ship* enemy : enemies)
        {
            Vec2 awayFromEnemy = myPos - enemy->getPosition();
            float dist = awayFromEnemy.length();
            if (dist > 0.01f)
            {
                // Weight by inverse distance - flee more urgently from closer enemies
                fleeDir = fleeDir + awayFromEnemy.normalized() * (1.0f / (dist + 1.0f));
            }
        }
        if (fleeDir.lengthSquared() > 0.01f)
            desiredDir = fleeDir.normalized();
        desiredSpeed = 1.0f; // Full speed escape
    }
    else if (currentMode == AIMode::Aggressive)
    {
        // Move toward the weakest enemy
        const Ship* target = findTarget (myShip, enemies);
        if (target)
        {
            Vec2 toTarget = target->getPosition() - myPos;
            float dist = toTarget.length();

            // Get close but not too close (stay at half firing range)
            float idealDist = Config::maxCrosshairDistance * 0.5f;
            if (dist > idealDist)
            {
                desiredDir = toTarget.normalized();
                desiredSpeed = 0.7f;
            }
            else
            {
                // Circle around target at ideal distance
                Vec2 perpendicular = { -toTarget.y, toTarget.x };
                desiredDir = perpendicular.normalized();
                desiredSpeed = 0.4f;
            }
        }
    }
    else // Normal mode
    {
        // Stay at edge of firing range from nearest enemy
        const Ship* nearest = findTarget (myShip, enemies);
        if (nearest)
        {
            Vec2 toEnemy = nearest->getPosition() - myPos;
            float dist = toEnemy.length();

            // Ideal distance is just inside max firing range
            float idealDist = Config::maxCrosshairDistance * 0.85f;
            float tolerance = 30.0f;

            if (dist < idealDist - tolerance)
            {
                // Too close - back away
                desiredDir = toEnemy.normalized() * -1.0f;
                desiredSpeed = 0.5f;
            }
            else if (dist > idealDist + tolerance)
            {
                // Too far - move closer
                desiredDir = toEnemy.normalized();
                desiredSpeed = 0.5f;
            }
            else
            {
                // Good distance - circle around to get better angle
                Vec2 perpendicular = { -toEnemy.y, toEnemy.x };
                desiredDir = perpendicular.normalized();
                desiredSpeed = 0.3f;
            }
        }
    }

    // Apply edge avoidance
    avoidEdges (myShip, arenaWidth, arenaHeight, desiredDir);

    // Convert desired direction to steering input
    if (desiredDir.lengthSquared() > 0.01f)
    {
        float targetAngle = desiredDir.toAngle();
        float angleDiff = targetAngle - shipAngle;

        // Normalize to [-PI, PI]
        while (angleDiff > pi)
            angleDiff -= 2.0f * pi;
        while (angleDiff < -pi)
            angleDiff += 2.0f * pi;

        // Turn toward target
        moveInput.x = std::clamp (angleDiff * 2.0f, -1.0f, 1.0f);

        // Move forward if roughly facing target
        if (std::abs (angleDiff) < pi * 0.5f)
        {
            moveInput.y = -desiredSpeed; // Negative Y is forward
        }
        else if (std::abs (angleDiff) > pi * 0.75f)
        {
            // Facing away - reverse a bit while turning
            moveInput.y = 0.2f;
        }
        else
        {
            moveInput.y = 0.0f;
        }
    }
    else
    {
        moveInput = { 0, 0 };
    }
}

void AIController::avoidEdges (const Ship& myShip, float arenaWidth, float arenaHeight, Vec2& desiredDir)
{
    Vec2 pos = myShip.getPosition();
    Vec2 vel = myShip.getVelocity();
    float speed = myShip.getSpeed();
    float shipLength = myShip.getLength();

    // Look ahead based on speed
    float lookAheadTime = Config::aiLookAheadTime;
    Vec2 futurePos = pos + vel * lookAheadTime;

    float dangerMargin = shipLength * 2.0f + speed * 1.5f;

    Vec2 avoidDir = { 0, 0 };
    float urgency = 0.0f;

    if (futurePos.x < dangerMargin)
    {
        avoidDir.x += 1.0f;
        urgency = std::max (urgency, 1.0f - futurePos.x / dangerMargin);
    }
    if (futurePos.x > arenaWidth - dangerMargin)
    {
        avoidDir.x -= 1.0f;
        urgency = std::max (urgency, 1.0f - (arenaWidth - futurePos.x) / dangerMargin);
    }
    if (futurePos.y < dangerMargin)
    {
        avoidDir.y += 1.0f;
        urgency = std::max (urgency, 1.0f - futurePos.y / dangerMargin);
    }
    if (futurePos.y > arenaHeight - dangerMargin)
    {
        avoidDir.y -= 1.0f;
        urgency = std::max (urgency, 1.0f - (arenaHeight - futurePos.y) / dangerMargin);
    }

    if (avoidDir.lengthSquared() > 0.01f)
    {
        avoidDir = avoidDir.normalized();
        // Blend avoid direction with desired direction based on urgency
        urgency = std::clamp (urgency * 2.0f, 0.0f, 1.0f);
        desiredDir = desiredDir * (1.0f - urgency) + avoidDir * urgency;
        if (desiredDir.lengthSquared() > 0.01f)
            desiredDir = desiredDir.normalized();
    }
}

bool AIController::isNearEdge (const Ship& myShip, float arenaWidth, float arenaHeight)
{
    Vec2 pos = myShip.getPosition();
    float margin = Config::aiWanderMargin;
    return pos.x < margin || pos.x > arenaWidth - margin ||
           pos.y < margin || pos.y > arenaHeight - margin;
}

void AIController::updateAim (const Ship& myShip, const Ship* targetShip)
{
    if (targetShip)
    {
        Vec2 myPos = myShip.getPosition();
        Vec2 targetPos = targetShip->getPosition();
        Vec2 targetVel = targetShip->getVelocity();

        // Calculate shell speed (same formula as Ship::fireShells)
        float shellSpeed = myShip.getMaxSpeed() * Config::shellSpeedMultiplier;

        // Calculate distance to target
        Vec2 toTarget = targetPos - myPos;
        float distance = toTarget.length();

        if (distance > 0.01f)
        {
            // Calculate flight time based on distance
            float flightTime = distance / shellSpeed;

            // Predict where target will be when shell arrives
            Vec2 predictedPos = targetPos + targetVel * flightTime;

            // Refine prediction: recalculate with new distance
            float newDistance = (predictedPos - myPos).length();
            flightTime = newDistance / shellSpeed;
            predictedPos = targetPos + targetVel * flightTime;

            // Calculate where crosshair should be
            Vec2 desiredCrosshairPos = predictedPos;
            Vec2 currentCrosshairPos = myShip.getCrosshairPosition();

            // Move crosshair toward predicted target position
            Vec2 crosshairDiff = desiredCrosshairPos - currentCrosshairPos;
            float crosshairDist = crosshairDiff.length();

            if (crosshairDist > 5.0f)
            {
                aimInput = crosshairDiff.normalized();
            }
            else
            {
                aimInput = { 0, 0 };
            }

            // Fire if crosshair is close to predicted position and in range
            fireInput = crosshairDist < Config::aiCrosshairTolerance &&
                        distance < Config::aiFireDistance &&
                        myShip.isReadyToFire();
        }
        else
        {
            aimInput = { 0, 0 };
            fireInput = false;
        }
    }
    else
    {
        aimInput = { 0, 0 };
        fireInput = false;
    }
}
