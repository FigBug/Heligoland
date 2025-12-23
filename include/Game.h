#pragma once

#include "AIController.h"
#include "Player.h"
#include "Renderer.h"
#include "Shell.h"
#include "Ship.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <vector>

enum class GameState
{
    Title,
    Playing,
    GameOver
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
    static constexpr float GAME_OVER_DELAY = 3.0f;

    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::unique_ptr<Renderer> renderer;

    bool running = false;
    GameState state = GameState::Title;
    int winnerIndex = -1;
    float gameOverTimer = 0.0f;
    float gameStartDelay = 0.0f; // Delay before accepting fire input after game starts
    Uint64 lastFrameTime = 0;

    std::array<std::unique_ptr<Ship>, NUM_SHIPS> ships;
    std::array<std::unique_ptr<Player>, NUM_SHIPS> players;
    std::array<std::unique_ptr<AIController>, NUM_SHIPS> aiControllers;
    std::vector<Shell> shells;

    // Wind system
    Vec2 wind; // Current wind direction and strength (length = strength 0-1)
    Vec2 targetWind; // Wind is slowly moving toward this target
    float windChangeTimer = 0.0f;
    static constexpr float WIND_CHANGE_INTERVAL = 5.0f; // Seconds between wind target changes
    static constexpr float WIND_LERP_SPEED = 0.3f; // How fast wind changes
    static constexpr float MAX_WIND_DRIFT = 0.05f; // 5% max drift

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
};
