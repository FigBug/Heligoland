#include "Shell.h"

Shell::Shell (Vec2 startPos, Vec2 vel, int owner, float range, float dmg)
    : position (startPos), velocity (vel), ownerIndex (owner), damage (dmg)
{
    // Calculate flight time based on range and initial speed
    float speed = vel.length();
    maxFlightTime = (speed > 0) ? (range / speed) : 0.0f;
}

void Shell::update (float dt, Vec2 windDrift)
{
    if (landed)
        return;

    // Apply wind drift to velocity (wind affects trajectory)
    // Tailwind = shell goes further, headwind = shell lands shorter
    velocity += windDrift * dt;

    position += velocity * dt;
    flightTime += dt;

    // Shell lands after fixed flight time
    if (flightTime >= maxFlightTime)
    {
        landed = true;
        velocity = { 0, 0 };
    }
}
