#include "AIController.h"
#include "Config.h"
#include "Shell.h"
#include "Ship.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

AIController::AIController()
{
    wanderTarget = { 0, 0 };

    // Generate random personality factor between 0.95 and 1.05
    static std::random_device rd;
    static std::mt19937 gen (rd());
    std::uniform_real_distribution<float> dist (0.95f, 1.05f);
    personalityFactor = dist (gen);
}

void AIController::update (float dt, Ship& myShip, const std::vector<const Ship*>& enemies, const std::vector<Shell>& shells, float arenaWidth, float arenaHeight)
{
    currentMode = determineMode (myShip, enemies);
    const Ship* target = findTarget (myShip, enemies);

    updateMovement (dt, myShip, enemies, shells, arenaWidth, arenaHeight);
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
    float firingRange = config.maxShellRange * personalityFactor;

    // Separate enemies into in-range and out-of-range
    std::vector<const Ship*> inRange;
    std::vector<const Ship*> outOfRange;
    for (const Ship* enemy : enemies)
    {
        float dist = (enemy->getPosition() - myPos).length();
        if (dist <= firingRange)
            inRange.push_back (enemy);
        else
            outOfRange.push_back (enemy);
    }

    // Prefer in-range enemies, fall back to out-of-range if none
    const std::vector<const Ship*>& candidates = inRange.empty() ? outOfRange : inRange;

    // In aggressive mode, target the weakest enemy
    if (currentMode == AIMode::Aggressive)
    {
        const Ship* weakest = nullptr;
        float lowestHealth = 999999.0f;
        for (const Ship* enemy : candidates)
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
    for (const Ship* enemy : candidates)
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

void AIController::updateMovement (float dt, const Ship& myShip, const std::vector<const Ship*>& enemies, const std::vector<Shell>& shells, float arenaWidth, float arenaHeight)
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

    // TOP PRIORITY: Dodge incoming shells
    float dodgeUrgency = 0.0f;
    Vec2 dodgeDir = getDodgeDirection (myShip, shells, dodgeUrgency);

    if (dodgeUrgency > 0.5f)
    {
        // Urgent dodge - override all other movement
        Vec2 desiredDir = dodgeDir;
        float desiredSpeed = 1.0f;  // Full speed dodge

        avoidEdges (myShip, arenaWidth, arenaHeight, desiredDir);

        if (desiredDir.lengthSquared() > 0.01f)
        {
            float targetAngle = desiredDir.toAngle();
            float angleDiff = targetAngle - shipAngle;

            while (angleDiff > pi)
                angleDiff -= 2.0f * pi;
            while (angleDiff < -pi)
                angleDiff += 2.0f * pi;

            moveInput.x = std::clamp (angleDiff * 2.0f, -1.0f, 1.0f);

            if (std::abs (angleDiff) < pi * 0.5f)
                moveInput.y = -desiredSpeed;
            else if (std::abs (angleDiff) > pi * 0.75f)
                moveInput.y = 0.3f;  // Reverse while turning
            else
                moveInput.y = 0.0f;
        }
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
            float wanderMargin = config.aiWanderMargin;
            wanderTarget.x = wanderMargin + (rand() / (float) RAND_MAX) * (arenaWidth - 2 * wanderMargin);
            wanderTarget.y = wanderMargin + (rand() / (float) RAND_MAX) * (arenaHeight - 2 * wanderMargin);
            wanderTimer = config.aiWanderInterval + (rand() / (float) RAND_MAX) * 2.0f;
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
            float idealDist = config.maxShellRange * 0.5f * personalityFactor;
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
        // Cautious approach - stay at edge of firing range and get broadside
        const Ship* target = findTarget (myShip, enemies);
        if (target)
        {
            Vec2 toEnemy = target->getPosition() - myPos;
            float dist = toEnemy.length();
            float enemyAngle = target->getAngle();

            // Calculate if we're in the enemy's firing arc (front 180 degrees)
            Vec2 enemyForward = Vec2::fromAngle (enemyAngle);
            Vec2 enemyToUs = (myPos - target->getPosition()).normalized();
            float dotProduct = enemyForward.dot (enemyToUs);
            bool inEnemyFiringArc = dotProduct > 0.0f;  // Enemy can see us

            // Ideal distance - stay just inside our max range, but outside if enemy is aiming at us
            float idealDist = config.maxShellRange * 0.9f * personalityFactor;
            if (inEnemyFiringArc && dist < config.maxShellRange * personalityFactor)
            {
                // Enemy can shoot at us - prefer to stay further back
                idealDist = config.maxShellRange * 1.05f * personalityFactor;
            }

            float tolerance = 40.0f * personalityFactor;

            // Calculate perpendicular direction for circling (to get broadside)
            Vec2 perpendicular = { -toEnemy.y, toEnemy.x };

            // Check which perpendicular direction gets us more broadside to enemy
            // We want to be perpendicular to the enemy's facing direction
            Vec2 enemySide = { -enemyForward.y, enemyForward.x };
            if (perpendicular.dot (enemySide) < 0)
                perpendicular = perpendicular * -1.0f;

            // Start broadside approach at 1.2x firing range
            float broadsideStartDist = config.maxShellRange * 1.2f * personalityFactor;

            if (dist < idealDist - tolerance)
            {
                // Too close - back away while circling
                Vec2 awayDir = toEnemy.normalized() * -1.0f;
                desiredDir = (awayDir + perpendicular * 0.5f).normalized();
                desiredSpeed = 0.6f;
            }
            else if (dist > broadsideStartDist)
            {
                // Far away - approach directly to close distance faster
                desiredDir = toEnemy.normalized();
                desiredSpeed = 0.6f;
            }
            else if (dist > idealDist + tolerance)
            {
                // Within broadside range - approach at an angle to get broadside
                Vec2 approachDir = toEnemy.normalized();
                desiredDir = (approachDir + perpendicular * 0.8f).normalized();
                desiredSpeed = 0.5f;
            }
            else
            {
                // Good distance - circle to maintain broadside position
                desiredDir = perpendicular;
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
    float lookAheadTime = config.aiLookAheadTime;
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
    float margin = config.aiWanderMargin;
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
        float shellSpeed = myShip.getMaxSpeed() * config.shellSpeedMultiplier;

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
            fireInput = crosshairDist < config.aiCrosshairTolerance * personalityFactor &&
                        distance < config.aiFireDistance * personalityFactor &&
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

Vec2 AIController::getDodgeDirection (const Ship& myShip, const std::vector<Shell>& shells, float& urgency)
{
    Vec2 myPos = myShip.getPosition();
    float shipRadius = myShip.getLength() / 2.0f;
    int myIndex = myShip.getPlayerIndex();

    Vec2 totalDodgeDir = { 0, 0 };
    urgency = 0.0f;

    for (const Shell& shell : shells)
    {
        if (! shell.isAlive() || shell.hasLanded())
            continue;

        // Ignore our own shells
        if (shell.getOwnerIndex() == myIndex)
            continue;

        Vec2 shellPos = shell.getPosition();
        Vec2 shellVel = shell.getVelocity();
        float shellSpeed = shellVel.length();

        if (shellSpeed < 1.0f)
            continue;

        Vec2 shellDir = shellVel.normalized();

        // Vector from shell to ship
        Vec2 toShip = myPos - shellPos;

        // Project ship position onto shell's path
        float projDist = toShip.dot (shellDir);

        // Shell is behind us or already passed
        if (projDist < 0)
            continue;

        // Closest point on shell's path to ship
        Vec2 closestPoint = shellPos + shellDir * projDist;
        Vec2 toClosest = myPos - closestPoint;
        float perpDist = toClosest.length();

        // Danger radius - how close is too close
        float dangerRadius = (shipRadius + shell.getSplashRadius() + 30.0f) * personalityFactor;

        if (perpDist < dangerRadius)
        {
            // Shell is heading toward us!
            float timeToImpact = projDist / shellSpeed;

            // Only worry about shells arriving soon
            if (timeToImpact < 2.0f)
            {
                // Calculate urgency based on time and proximity
                float timeUrgency = 1.0f - (timeToImpact / 2.0f);
                float proxUrgency = 1.0f - (perpDist / dangerRadius);
                float shellUrgency = std::max (timeUrgency, proxUrgency);

                // Dodge perpendicular to shell path
                Vec2 dodgeDir;
                if (perpDist > 0.1f)
                {
                    dodgeDir = toClosest.normalized();
                }
                else
                {
                    // Shell is heading straight at us - pick a perpendicular direction
                    dodgeDir = { -shellDir.y, shellDir.x };
                }

                // Weight by urgency
                totalDodgeDir = totalDodgeDir + dodgeDir * shellUrgency;
                urgency = std::max (urgency, shellUrgency);
            }
        }
    }

    if (totalDodgeDir.lengthSquared() > 0.01f)
        return totalDodgeDir.normalized();

    return { 0, 0 };
}
