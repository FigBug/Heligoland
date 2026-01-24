#pragma once

#include "Config.h"
#include "Vec2.h"
#include <random>
#include <vector>

class Ship;
class Shell;
class Island;

enum class AIMode
{
    Aggressive,  // Enemy has much less health - move in for the kill
    Normal,      // Health is similar - stay at edge of firing range
    Scared       // Low health - run away from enemies
};

class AIController
{
public:
    AIController();

    // Get this AI's personality factor (0.95 to 1.05)
    float getPersonality() const { return personalityFactor; }

    void update (float dt, Ship& myShip, const std::vector<const Ship*>& enemies, const std::vector<const Ship*>& friendlies, const std::vector<Shell>& shells, const std::vector<Island>& islands, float arenaWidth, float arenaHeight);

    Vec2 getMoveInput() const { return moveInput; }
    Vec2 getAimInput() const { return aimInput; }
    bool getFireInput() const { return fireInput; }

private:
    Vec2 moveInput;
    Vec2 aimInput;
    bool fireInput = false;

    Vec2 wanderTarget;
    float wanderTimer = 0.0f;

    // Personality factor (0.95 to 1.05) - makes each AI slightly different
    float personalityFactor = 1.0f;

    AIMode currentMode = AIMode::Normal;

    AIMode determineMode (const Ship& myShip, const std::vector<const Ship*>& enemies);
    const Ship* findTarget (const Ship& myShip, const std::vector<const Ship*>& enemies);
    void updateMovement (float dt, const Ship& myShip, const std::vector<const Ship*>& enemies, const std::vector<const Ship*>& friendlies, const std::vector<Shell>& shells, const std::vector<Island>& islands, float arenaWidth, float arenaHeight);
    void updateAim (const Ship& myShip, const Ship* targetShip);
    void avoidEdges (const Ship& myShip, float arenaWidth, float arenaHeight, Vec2& desiredDir);
    void avoidShips (const Ship& myShip, const std::vector<const Ship*>& otherShips, Vec2& desiredDir);
    void avoidIslands (const Ship& myShip, const std::vector<Island>& islands, Vec2& desiredDir);
    bool isNearEdge (const Ship& myShip, float arenaWidth, float arenaHeight);
    Vec2 getDodgeDirection (const Ship& myShip, const std::vector<Shell>& shells, float& urgency);
};
