#pragma once

#include "Vec2.h"
#include "Turret.h"
#include <array>

struct SDL_Color;

class Ship {
public:
    Ship(int playerIndex, Vec2 startPos, float startAngle);

    void update(float dt, Vec2 moveInput, Vec2 aimInput, float arenaWidth, float arenaHeight);

    Vec2 getPosition() const { return position; }
    float getAngle() const { return angle; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    int getPlayerIndex() const { return playerIndex; }
    const std::array<Turret, 4>& getTurrets() const { return turrets; }
    Vec2 getCrosshairPosition() const { return crosshairPos; }
    SDL_Color getColor() const;

private:
    int playerIndex;
    Vec2 position;
    Vec2 velocity;
    float angle = 0.0f;          // Ship facing direction (radians)
    float angularVelocity = 0.0f;

    float width = 80.0f;
    float height = 40.0f;

    float maxSpeed = 200.0f;
    float acceleration = 150.0f;
    float drag = 0.98f;
    float turnSpeed = 2.5f;

    float crosshairDistance = 150.0f;
    Vec2 crosshairPos;

    std::array<Turret, 4> turrets;

    void clampToArena(float arenaWidth, float arenaHeight);
};
