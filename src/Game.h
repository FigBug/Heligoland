#pragma once

#include "AIController.h"
#include "Audio.h"
#include "Config.h"
#include "Island.h"
#include "Player.h"
#include "Renderer.h"
#include "Shell.h"
#include "Ship.h"
#include <array>
#include <memory>
#include <vector>

enum class GameState
{
    Title,
    Playing,
    GameOver
};

enum class GameMode
{
    FFA,      // Free for all - every ship for themselves
    Teams,    // 2v2 - ships 0,1 vs ships 2,3
    Duel,     // 1v1 - ship 0 vs ship 1
    Triple,   // 1v1v1 - 3 ships
    Battle    // 6v6 - ships 0-5 vs ships 6-11, up to 2 humans per team
};

struct Explosion
{
    Vec2 position;
    float timer = 0.0f;
    float duration = 0.0f;  // Set at creation, defaults applied in Game.cpp
    float maxRadius = 0.0f; // Set at creation, defaults applied in Game.cpp
    bool isHit = false; // true = explosion (orange), false = splash (blue)

    float getProgress() const { return timer / duration; }
    bool isAlive() const { return timer < duration; }
};

class Game
{
public:
    Game();
    ~Game();

    bool init();
    void run();
    void shutdown();

private:
    static constexpr int WINDOW_WIDTH = 1280;
    static constexpr int WINDOW_HEIGHT = 720;
    static constexpr int MAX_SHIPS = 12;      // Maximum ships (for Battle mode 6v6)
    static constexpr int MAX_PLAYERS = 4;     // Maximum human players

    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Audio> audio;

    bool running = false;
    GameState state = GameState::Title;
    GameMode gameMode = GameMode::FFA;
    int winnerIndex = -1;  // In FFA: player index, in Teams: team index (0 or 1)
    float gameOverTimer = 0.0f;
    float gameStartDelay = 0.0f; // Delay before accepting fire input after game starts
    float time = 0.0f; // Total elapsed time for animations
    double lastFrameTime = 0.0;

    std::array<std::unique_ptr<Ship>, MAX_SHIPS> ships;
    std::array<std::unique_ptr<Player>, MAX_PLAYERS> players;
    std::array<std::unique_ptr<AIController>, MAX_SHIPS> aiControllers;
    std::vector<Shell> shells;
    std::vector<Explosion> explosions;
    std::vector<Island> islands;

    // Ship selection for each player (0-3 = ship types with 1-4 turrets)
    std::array<int, MAX_PLAYERS> playerShipSelection = { 3, 2, 2, 2 };  // Default to cruiser

    // AI ship selection (randomly cycles on title screen)
    std::array<int, MAX_PLAYERS> aiShipSelection = {};
    float aiShipChangeTimer = 0.0f;

    // Ready-up / lock-in system
    std::array<bool, MAX_PLAYERS> playerLockedIn = {};
    float lockInCountdown = -1.0f;  // Negative = not counting down

    // Wind system
    Vec2 wind; // Current wind direction and strength (length = strength 0-1)
    Vec2 targetWind; // Wind is slowly moving toward this target
    float windChangeTimer = 0.0f;

    // Current system (affects ship movement)
    Vec2 current; // Current direction and strength (length = strength 0-1)
    Vec2 targetCurrent; // Current is slowly moving toward this target
    float currentChangeTimer = 0.0f;

    // Win tracking
    std::array<int, MAX_PLAYERS> playerWins = {}; // Wins per player in FFA mode
    std::array<int, 2> teamWins = {};             // Wins per team in Teams/Battle mode

    void updateWind (float dt);
    void updateCurrent (float dt);

    void handleEvents();
    void update (float dt);
    void render();

    // Title screen
    void updateTitle (float dt);
    void renderTitle();
    bool anyButtonPressed();

    // Gameplay
    void startGame();
    void updatePlaying (float dt);
    void renderPlaying();
    void updateShells (float dt);
    void checkCollisions();
    void checkGameOver();

    // Game over
    void updateGameOver (float dt);
    void renderGameOver();
    void returnToTitle();

    Vec2 getShipStartPosition (int index) const;
    float getShipStartAngle (int index) const;
    int getTeam (int shipIndex) const;  // Returns 0 or 1 for team mode
    bool areEnemies (int shipA, int shipB) const;
    void getWindowSize (float& width, float& height) const;
    void cycleGameMode (int direction);
    int getNumShipsForMode() const;  // Returns number of ships for current game mode
    int getShipIndexForPlayer (int playerIndex) const;  // Maps player slot to ship index
    int getPlayerIndexForShip (int shipIndex) const;    // Maps ship index to player slot (-1 if AI)
};
