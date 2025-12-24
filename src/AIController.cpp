#include "AIController.h"
#include "Ship.h"
#include <cmath>
#include <cstdlib>

AIController::AIController()
{
    wanderTarget = { 0, 0 };
}

void AIController::update (float dt, Ship& myShip, const Ship* targetShip, float arenaWidth, float arenaHeight)
{
    updateWander (dt, myShip, arenaWidth, arenaHeight);
    updateAim (myShip, targetShip);
}

void AIController::updateWander (float dt, const Ship& myShip, float arenaWidth, float arenaHeight)
{
    wanderTimer -= dt;

    if (wanderTimer <= 0.0f)
    {
        // Pick a new random target within the arena
        float margin = 150.0f;
        wanderTarget.x = margin + (rand() / (float) RAND_MAX) * (arenaWidth - 2 * margin);
        wanderTarget.y = margin + (rand() / (float) RAND_MAX) * (arenaHeight - 2 * margin);
        wanderTimer = wanderInterval + (rand() / (float) RAND_MAX) * 2.0f;
    }

    Vec2 pos = myShip.getPosition();
    Vec2 vel = myShip.getVelocity();
    float speed = myShip.getSpeed();
    float shipAngle = myShip.getAngle();
    float shipLength = myShip.getLength();

    // Look ahead based on speed - predict where we'll be
    float lookAheadTime = 2.0f; // Look 2 seconds ahead
    Vec2 futurePos = pos + vel * lookAheadTime;

    // Calculate danger margin - larger when moving faster
    float dangerMargin = shipLength * 2.0f + speed * 1.5f;

    // Check for edge danger
    bool edgeDangerLeft = futurePos.x < dangerMargin;
    bool edgeDangerRight = futurePos.x > arenaWidth - dangerMargin;
    bool edgeDangerTop = futurePos.y < dangerMargin;
    bool edgeDangerBottom = futurePos.y > arenaHeight - dangerMargin;
    bool edgeDanger = edgeDangerLeft || edgeDangerRight || edgeDangerTop || edgeDangerBottom;

    // Check if already hit something (stopped but throttle applied)
    float margin = shipLength;
    bool nearEdge = pos.x < margin || pos.x > arenaWidth - margin ||
                    pos.y < margin || pos.y > arenaHeight - margin;
    bool stopped = speed < 0.5f && std::abs (myShip.getThrottle()) > 0.1f;

    if (nearEdge || stopped)
    {
        // Already crashed - reverse and turn away
        moveInput.y = 0.5f; // Positive Y is reverse
        moveInput.x = (rand() % 2 == 0) ? 1.0f : -1.0f; // Random turn direction

        // Pick a new target toward center
        wanderTarget = { arenaWidth / 2.0f, arenaHeight / 2.0f };
        wanderTimer = 2.0f;
        return;
    }

    if (edgeDanger && speed > 2.0f)
    {
        // Approaching edge - steer away proactively
        Vec2 avoidDir = { 0, 0 };

        if (edgeDangerLeft)
            avoidDir.x += 1.0f;
        if (edgeDangerRight)
            avoidDir.x -= 1.0f;
        if (edgeDangerTop)
            avoidDir.y += 1.0f;
        if (edgeDangerBottom)
            avoidDir.y -= 1.0f;

        if (avoidDir.lengthSquared() > 0.01f)
        {
            avoidDir = avoidDir.normalized();
            float avoidAngle = avoidDir.toAngle();
            float angleDiff = avoidAngle - shipAngle;

            // Normalize to [-PI, PI]
            while (angleDiff > M_PI)
                angleDiff -= 2.0f * M_PI;
            while (angleDiff < -M_PI)
                angleDiff += 2.0f * M_PI;

            // Strong turn to avoid
            moveInput.x = std::clamp (angleDiff * 3.0f, -1.0f, 1.0f);

            // Slow down if heading toward danger
            float dotProduct = vel.normalized().dot (avoidDir * -1.0f);
            if (dotProduct > 0.3f)
            {
                moveInput.y = 0.3f; // Slow down / slight reverse
            }
            else
            {
                moveInput.y = -0.3f; // Keep moving but slower
            }

            // Update wander target to safe area
            wanderTarget = { arenaWidth / 2.0f, arenaHeight / 2.0f };
            wanderTimer = 1.0f;
            return;
        }
    }

    // Move toward wander target
    Vec2 toTarget = wanderTarget - pos;
    float distance = toTarget.length();

    if (distance < 50.0f)
    {
        // Close enough, slow down
        moveInput = { 0, 0 };
    }
    else
    {
        // Calculate desired direction
        Vec2 desiredDir = toTarget.normalized();

        // Calculate angle to target
        float targetAngle = desiredDir.toAngle();
        float angleDiff = targetAngle - shipAngle;

        // Normalize to [-PI, PI]
        while (angleDiff > M_PI)
            angleDiff -= 2.0f * M_PI;
        while (angleDiff < -M_PI)
            angleDiff += 2.0f * M_PI;

        // Turn toward target
        moveInput.x = std::clamp (angleDiff * 2.0f, -1.0f, 1.0f);

        // Move forward if roughly facing target
        if (std::abs (angleDiff) < M_PI * 0.5f)
        {
            moveInput.y = -0.5f; // Negative Y is forward
        }
        else
        {
            moveInput.y = 0.0f;
        }
    }
}

void AIController::updateAim (const Ship& myShip, const Ship* targetShip)
{
    if (targetShip)
    {
        // Aim at the target ship
        Vec2 toTarget = targetShip->getPosition() - myShip.getPosition();
        float distance = toTarget.length();

        if (distance > 0.01f)
        {
            // Calculate where crosshair should be (at target position)
            Vec2 desiredCrosshairPos = targetShip->getPosition();
            Vec2 currentCrosshairPos = myShip.getCrosshairPosition();

            // Move crosshair toward target
            Vec2 crosshairDiff = desiredCrosshairPos - currentCrosshairPos;
            float crosshairDist = crosshairDiff.length();

            if (crosshairDist > 5.0f)
            {
                // Move crosshair toward target position
                aimInput = crosshairDiff.normalized();
            }
            else
            {
                aimInput = { 0, 0 };
            }

            // Fire if crosshair is close to target and ship is ready
            fireInput = crosshairDist < 30.0f && distance < 400.0f && myShip.isReadyToFire();
        }
        else
        {
            aimInput = { 0, 0 };
            fireInput = false;
        }
    }
    else
    {
        // No target, don't move crosshair and don't fire
        aimInput = { 0, 0 };
        fireInput = false;
    }
}
