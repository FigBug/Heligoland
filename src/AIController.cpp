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

    // Move toward wander target
    Vec2 toTarget = wanderTarget - myShip.getPosition();
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
        Vec2 shipForward = Vec2::fromAngle (shipAngle);

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
            aimInput = toTarget.normalized();

            // Fire if target is within reasonable range
            fireInput = distance < 400.0f;
        }
        else
        {
            fireInput = false;
        }
    }
    else
    {
        // No target, aim forward and don't fire
        aimInput = Vec2::fromAngle (myShip.getAngle());
        fireInput = false;
    }
}
