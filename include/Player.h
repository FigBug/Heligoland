#pragma once

#include "Vec2.h"
#include <SDL3/SDL.h>

class Player {
public:
    Player(int playerIndex);
    ~Player();

    void handleEvent(const SDL_Event& event);
    void update();

    Vec2 getMoveInput() const { return moveInput; }
    Vec2 getAimInput() const { return aimInput; }
    bool isConnected() const { return gamepad != nullptr; }
    int getPlayerIndex() const { return playerIndex; }

private:
    int playerIndex;
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID joystickId = 0;

    Vec2 moveInput;
    Vec2 aimInput;

    float deadzone = 0.15f;

    float applyDeadzone(float value) const;
    void tryOpenGamepad();
};
