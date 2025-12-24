#pragma once

#include "Shell.h"
#include "Turret.h"
#include "Vec2.h"
#include <raylib.h>
#include <array>
#include <vector>

struct Bubble
{
    Vec2 position;
    float radius;
    float alpha; // 0.0 to 1.0
};

struct Smoke
{
    Vec2 position;
    float radius;
    float alpha; // 0.0 to 1.0
    float windAngleOffset; // Random offset added to wind direction
};

class Ship
{
public:
    Ship (int playerIndex, Vec2 startPos, float startAngle, bool teamMode = false);

    void update (float dt, Vec2 moveInput, Vec2 aimInput, bool fireInput, float arenaWidth, float arenaHeight, Vec2 wind);

    Vec2 getPosition() const { return position; }
    float getAngle() const { return angle; }
    float getLength() const { return length; }
    float getWidth() const { return width; }
    float getMaxSpeed() const { return maxSpeed; }
    int getPlayerIndex() const { return playerIndex; }
    const std::array<Turret, 4>& getTurrets() const { return turrets; }
    Vec2 getCrosshairPosition() const { return position + crosshairOffset; }
    void setCrosshairPosition (Vec2 worldPos);  // For mouse aiming
    const std::vector<Bubble>& getBubbles() const { return bubbles; }
    const std::vector<Smoke>& getSmoke() const { return smoke; }
    float getDamagePercent() const { return 1.0f - (health / maxHealth); }
    std::vector<Shell>& getPendingShells() { return pendingShells; }
    Color getColor() const;

    // Health system
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    bool isAlive() const { return health > 0 || isSinking(); }
    bool isSinking() const { return sinking; }
    bool isFullySunk() const { return sinking && sinkTimer >= sinkDuration; }
    float getSinkProgress() const { return sinking ? sinkTimer / sinkDuration : 0.0f; }
    void takeDamage (float damage);

    // Collision
    Vec2 getVelocity() const { return velocity; }
    float getSpeed() const { return velocity.length(); }
    void applyCollision (Vec2 pushDirection, float pushDistance, Vec2 myVel, Vec2 otherVel);
    std::array<Vec2, 4> getCorners() const; // Get 4 corners for OBB collision

    // HUD info
    float getThrottle() const { return throttle; }
    float getRudder() const { return rudder; }
    float getCrosshairDistance() const; // Distance from ship to crosshair
    float getReloadProgress() const { return 1.0f - (fireTimer / fireInterval); }
    bool isReadyToFire() const; // True if reloaded AND turrets on target AND in range
    bool isCrosshairInRange() const { return crosshairOffset.length() >= minShellRange; }

private:
    int playerIndex;
    bool teamMode;
    Vec2 position;
    Vec2 velocity;
    float angle = 0.0f; // Ship facing direction (radians)
    float angularVelocity = 0.0f;

    float length = 50.0f;
    float width = 15.0f;

    float maxSpeed = 10.0f;
    float throttleRate = 0.5f; // How fast throttle changes per second
    float rudderRate = 2.0f; // How fast rudder moves
    float rudderReturn = 3.0f; // How fast rudder returns to center

    float throttle = 0.0f; // -1 to 1 (current throttle position)
    float rudder = 0.0f; // -1 to 1 (current rudder position)

    Vec2 crosshairOffset; // Offset from ship position (moves with aim stick)
    float crosshairSpeed = 150.0f; // How fast crosshair moves
    float minShellRange = 50.0f; // Minimum range shells can travel
    float maxCrosshairDist = 300.0f;

    std::array<Turret, 4> turrets;

    std::vector<Bubble> bubbles;
    float bubbleSpawnTimer = 0.0f;
    float bubbleSpawnInterval = 0.02f;

    std::vector<Smoke> smoke;
    float smokeSpawnTimer = 0.0f;

    // Health
    float health = 1000.0f;
    float maxHealth = 1000.0f;

    // Sinking
    bool sinking = false;
    float sinkTimer = 0.0f;
    static constexpr float sinkDuration = 30.0f;

    // Shooting
    std::vector<Shell> pendingShells; // Shells to be added to game
    float fireTimer = 0.0f;
    float fireInterval = 10.0f; // Time between shots

    void clampToArena (float arenaWidth, float arenaHeight);
    void updateBubbles (float dt);
    void updateSmoke (float dt, Vec2 wind);
    void fireShells();
};
