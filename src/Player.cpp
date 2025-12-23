#include "Player.h"
#include <cmath>

Player::Player (int playerIndex_)
    : playerIndex (playerIndex_)
{
    tryOpenGamepad();
}

Player::~Player()
{
    if (gamepad)
    {
        SDL_CloseGamepad (gamepad);
        gamepad = nullptr;
    }
}

void Player::handleEvent (const SDL_Event& event)
{
    if (event.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        if (! gamepad)
        {
            tryOpenGamepad();
        }
    }
    else if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        if (gamepad && event.gdevice.which == joystickId)
        {
            SDL_CloseGamepad (gamepad);
            gamepad = nullptr;
            joystickId = 0;
        }
    }
}

void Player::update()
{
    if (! gamepad)
    {
        moveInput = { 0, 0 };
        aimInput = { 0, 0 };
        return;
    }

    // Left stick for movement
    float leftX = SDL_GetGamepadAxis (gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float leftY = SDL_GetGamepadAxis (gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

    moveInput.x = applyDeadzone (leftX);
    moveInput.y = applyDeadzone (leftY);

    // Right stick for aiming
    float rightX = SDL_GetGamepadAxis (gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    float rightY = SDL_GetGamepadAxis (gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

    aimInput.x = applyDeadzone (rightX);
    aimInput.y = applyDeadzone (rightY);

    // Right trigger or X button for firing
    float rightTrigger = SDL_GetGamepadAxis (gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    bool xButton = SDL_GetGamepadButton (gamepad, SDL_GAMEPAD_BUTTON_WEST); // X on Xbox, Square on PS
    fireInput = (rightTrigger > 0.5f) || xButton;
}

float Player::applyDeadzone (float value) const
{
    if (std::abs (value) < deadzone)
    {
        return 0.0f;
    }
    // Rescale from deadzone to 1.0
    float sign = value > 0 ? 1.0f : -1.0f;
    return sign * (std::abs (value) - deadzone) / (1.0f - deadzone);
}

void Player::tryOpenGamepad()
{
    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads (&numJoysticks);

    if (joysticks)
    {
        // Find the Nth gamepad for this player
        int gamepadCount = 0;
        for (int i = 0; i < numJoysticks; ++i)
        {
            if (gamepadCount == playerIndex)
            {
                gamepad = SDL_OpenGamepad (joysticks[i]);
                if (gamepad)
                {
                    joystickId = joysticks[i];
                    SDL_Log ("Player %d connected to gamepad: %s",
                             playerIndex,
                             SDL_GetGamepadName (gamepad));
                }
                break;
            }
            gamepadCount++;
        }
        SDL_free (joysticks);
    }
}
