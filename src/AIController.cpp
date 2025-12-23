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
        float margin = 100.0f;
        wanderTarget.x = margin + (rand() / (float) RAND_MAX) * (arenaWidth - 2 * margin);
        wanderTarget.y = margin + (rand() / (float) RAND_MAX) * (arenaHeight - 2 * margin);
        wanderTimer = wanderInterval + (rand() / (float) RAND_MAX) * 2.0f;
    }

    Vec2 pos = myShip.getPosition();
    float margin = myShip.getLength();

    // Check if near edge or stopped (likely hit something)
    bool nearEdge = pos.x < margin || pos.x > arenaWidth - margin ||
                    pos.y < margin || pos.y > arenaHeight - margin;
    bool stopped = myShip.getSpeed() < 0.5f && std::abs (myShip.getThrottle()) > 0.1f;

    if (nearEdge || stopped)
    {
        // Reverse and turn away
        moveInput.y = 0.5f; // Positive Y is reverse
        moveInput.x = (rand() % 2 == 0) ? 1.0f : -1.0f; // Random turn direction

        // Pick a new target toward center
        wanderTarget = { arenaWidth / 2.0f, arenaHeight / 2.0f };
        wanderTimer = 2.0f;
        return;
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
        float shipAngle = myShip.getAngle();

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
