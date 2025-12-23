#include "Shell.h"

Shell::Shell (Vec2 startPos, Vec2 vel, int owner, float range)
    : position (startPos), startPosition (startPos), velocity (vel), ownerIndex (owner), targetRange (range)
{
}

void Shell::update (float dt, Vec2 windDrift)
{
    if (landed)
    {
        // Shell has landed, just wait to be cleaned up
        return;
    }

    // Apply wind drift to velocity (wind affects trajectory)
    velocity += windDrift * dt;

    position += velocity * dt;

    // Check if shell has reached target range (lands/splashes)
    float distanceTraveled = (position - startPosition).length();
    if (distanceTraveled >= targetRange)
    {
        landed = true;
        velocity = { 0, 0 }; // Stop moving
    }
}
