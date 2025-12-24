#pragma once

#include "Config.h"
#include "Vec2.h"

class Shell
{
public:
    Shell (Vec2 startPos, Vec2 velocity, int ownerIndex, float maxRange);

    void update (float dt, Vec2 windDrift);

    Vec2 getPosition() const { return position; }
    Vec2 getVelocity() const { return velocity; }
    int getOwnerIndex() const { return ownerIndex; }
    float getRadius() const { return radius; }
    float getSplashRadius() const { return splashRadius; }
    bool isAlive() const { return alive; }
    bool hasLanded() const { return landed; } // True when shell reaches target range
    void kill() { alive = false; }

private:
    Vec2 position;
    Vec2 startPosition;
    Vec2 velocity;
    int ownerIndex; // Which player fired this shell
    float radius = Config::shellRadius;
    float splashRadius = Config::shellSplashRadius;
    bool alive = true;
    bool landed = false; // True when shell reaches target range
    float targetRange = 150.0f; // Distance shell travels before landing (set by constructor)
};
