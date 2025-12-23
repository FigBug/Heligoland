#pragma once

#include "Ship.h"
#include "Player.h"
#include "AIController.h"
#include "Renderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>

class Game {
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

    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::unique_ptr<Renderer> renderer;

    bool running = false;
    Uint64 lastFrameTime = 0;

    std::array<std::unique_ptr<Ship>, NUM_SHIPS> ships;
    std::array<std::unique_ptr<Player>, NUM_SHIPS> players;
    std::array<std::unique_ptr<AIController>, NUM_SHIPS> aiControllers;

    void handleEvents();
    void update(float dt);
    void render();

    Vec2 getShipStartPosition(int index) const;
    float getShipStartAngle(int index) const;
};
