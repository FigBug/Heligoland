#include "Player.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>

Player::Player (int playerIndex_)
    : playerIndex (playerIndex_)
{
    tryOpenGamepad();
}

Player::~Player()
{
}

void Player::update()
{
    // Check if gamepad is still available, or try to find one
    if (gamepadId < 0 || !IsGamepadAvailable (gamepadId))
    {
        tryOpenGamepad();
    }

    if (gamepadId < 0)
    {
        moveInput = { 0, 0 };
        aimInput = { 0, 0 };
        fireInput = false;
        return;
    }

    // Left stick Y for throttle
    float leftY = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_LEFT_Y);
    moveInput.y = applyDeadzone (leftY);

    // Rudder: left stick X and triggers
    float leftX = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_LEFT_X);
    float leftTrigger = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_LEFT_TRIGGER);
    float rightTrigger = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_RIGHT_TRIGGER);

    // Triggers on raylib return -1 to 1, where -1 is unpressed and 1 is fully pressed
    // Normalize to 0-1 range
    leftTrigger = (leftTrigger + 1.0f) / 2.0f;
    rightTrigger = (rightTrigger + 1.0f) / 2.0f;

    float stickRudder = applyDeadzone (leftX);
    float triggerRudder = rightTrigger - leftTrigger; // Right trigger = turn right, left = turn left

    moveInput.x = std::clamp (stickRudder + triggerRudder, -1.0f, 1.0f);

    // Right stick for aiming crosshair
    float rightX = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_RIGHT_X);
    float rightY = GetGamepadAxisMovement (gamepadId, GAMEPAD_AXIS_RIGHT_Y);
    aimInput.x = applyDeadzone (rightX);
    aimInput.y = applyDeadzone (rightY);

    // Any button fires
    fireInput = IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||  // A / Cross
                IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || // B / Circle
                IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||  // X / Square
                IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_RIGHT_FACE_UP) ||    // Y / Triangle
                IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
                IsGamepadButtonDown (gamepadId, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
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
    // Find an available gamepad for this player index
    // Count available gamepads up to our player index
    int gamepadCount = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (IsGamepadAvailable (i))
        {
            if (gamepadCount == playerIndex)
            {
                gamepadId = i;
                TraceLog (LOG_INFO, "Player %d connected to gamepad %d: %s",
                          playerIndex, i, GetGamepadName (i));
                return;
            }
            gamepadCount++;
        }
    }

    // No gamepad found for this player
    gamepadId = -1;
}
