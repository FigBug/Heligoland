#pragma once

#include "Vec2.h"

class Ship;

class AIController
{
public:
    AIController();

    void update (float dt, Ship& myShip, const Ship* targetShip, float arenaWidth, float arenaHeight);

    Vec2 getMoveInput() const { return moveInput; }
    Vec2 getAimInput() const { return aimInput; }
    bool getFireInput() const { return fireInput; }

private:
    Vec2 moveInput;
    Vec2 aimInput;
    bool fireInput = false;

    Vec2 wanderTarget;
    float wanderTimer = 0.0f;
    float wanderInterval = 3.0f;

    void updateWander (float dt, const Ship& myShip, float arenaWidth, float arenaHeight);
    void updateAim (const Ship& myShip, const Ship* targetShip);
};
