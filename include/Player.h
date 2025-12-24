#pragma once

#include "Vec2.h"

class Player
{
public:
    Player (int playerIndex);
    ~Player();

    void update();

    Vec2 getMoveInput() const { return moveInput; }
    Vec2 getAimInput() const { return aimInput; }
    bool getFireInput() const { return fireInput; }
    bool isConnected() const { return gamepadId >= 0; }
    int getPlayerIndex() const { return playerIndex; }

private:
    int playerIndex;
    int gamepadId = -1;

    Vec2 moveInput;
    Vec2 aimInput;
    bool fireInput = false;

    float deadzone = 0.15f;

    float applyDeadzone (float value) const;
    void tryOpenGamepad();
};
