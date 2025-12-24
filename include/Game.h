#pragma once

#include "AIController.h"
#include "Audio.h"
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
    Teams     // 2v2 - ships 0,1 vs ships 2,3
};

struct Explosion
{
    Vec2 position;
    float timer = 0.0f;
    float duration = 0.5f;
    float maxRadius = 30.0f;
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
    static constexpr int NUM_SHIPS = 4;
    static constexpr float SHELL_DAMAGE = 100.0f;
    static constexpr float GAME_OVER_TEXT_DELAY = 5.0f; // Delay before showing winner text
    static constexpr float GAME_OVER_RETURN_DELAY = 8.0f; // Total delay before returning to title

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

    std::array<std::unique_ptr<Ship>, NUM_SHIPS> ships;
    std::array<std::unique_ptr<Player>, NUM_SHIPS> players;
    std::array<std::unique_ptr<AIController>, NUM_SHIPS> aiControllers;
    std::vector<Shell> shells;
    std::vector<Explosion> explosions;

    // Wind system
    Vec2 wind; // Current wind direction and strength (length = strength 0-1)
    Vec2 targetWind; // Wind is slowly moving toward this target
    float windChangeTimer = 0.0f;
    static constexpr float WIND_CHANGE_INTERVAL = 60.0f; // Seconds between wind target changes
    static constexpr float WIND_LERP_SPEED = 0.05f; // How fast wind changes (very gradual)
    static constexpr float MAX_WIND_DRIFT = 0.01f; // 1% max drift

    // Win tracking
    std::array<int, NUM_SHIPS> playerWins = {}; // Wins per player in FFA mode
    std::array<int, 2> teamWins = {};           // Wins per team in Teams mode

    void updateWind (float dt);

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
    int getTeam (int playerIndex) const;  // Returns 0 or 1 for team mode
    bool areEnemies (int playerA, int playerB) const;
    void getWindowSize (float& width, float& height) const;
    void cycleGameMode (int direction);
};
