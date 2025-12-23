#include "Game.h"
#include <cmath>

Game::Game() = default;

Game::~Game() = default;

bool Game::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Heligoland", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }

    sdlRenderer = SDL_CreateRenderer(window, nullptr);
    if (!sdlRenderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        return false;
    }

    renderer = std::make_unique<Renderer>(sdlRenderer);

    // Create ships at starting positions
    for (int i = 0; i < NUM_SHIPS; ++i) {
        ships[i] = std::make_unique<Ship>(i, getShipStartPosition(i), getShipStartAngle(i));
        players[i] = std::make_unique<Player>(i);
        aiControllers[i] = std::make_unique<AIController>();
    }

    running = true;
    lastFrameTime = SDL_GetTicks();

    return true;
}

void Game::run() {
    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        // Cap delta time to avoid spiral of death
        if (dt > 0.1f) dt = 0.1f;

        handleEvents();
        update(dt);
        render();
    }
}

void Game::shutdown() {
    ships = {};
    players = {};
    aiControllers = {};
    renderer.reset();

    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        // Pass events to players for gamepad handling
        for (auto& player : players) {
            player->handleEvent(event);
        }
    }
}

void Game::update(float dt) {
    // Update players and get input
    for (auto& player : players) {
        player->update();
    }

    // Update ships
    for (int i = 0; i < NUM_SHIPS; ++i) {
        Vec2 moveInput, aimInput;

        if (players[i]->isConnected()) {
            moveInput = players[i]->getMoveInput();
            aimInput = players[i]->getAimInput();
        } else {
            // Find nearest enemy ship for AI targeting
            const Ship* target = nullptr;
            float nearestDist = 999999.0f;
            for (int j = 0; j < NUM_SHIPS; ++j) {
                if (i != j) {
                    float dist = (ships[j]->getPosition() - ships[i]->getPosition()).length();
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        target = ships[j].get();
                    }
                }
            }

            aiControllers[i]->update(dt, *ships[i], target, WINDOW_WIDTH, WINDOW_HEIGHT);
            moveInput = aiControllers[i]->getMoveInput();
            aimInput = aiControllers[i]->getAimInput();
        }

        ships[i]->update(dt, moveInput, aimInput, WINDOW_WIDTH, WINDOW_HEIGHT);
    }
}

void Game::render() {
    renderer->clear();

    for (const auto& ship : ships) {
        renderer->drawShip(*ship);
        renderer->drawCrosshair(ship->getCrosshairPosition(), ship->getColor());
    }

    renderer->present();
}

Vec2 Game::getShipStartPosition(int index) const {
    float margin = 150.0f;
    switch (index) {
        case 0: return {margin, margin};                                    // Top-left
        case 1: return {WINDOW_WIDTH - margin, margin};                     // Top-right
        case 2: return {margin, WINDOW_HEIGHT - margin};                    // Bottom-left
        case 3: return {WINDOW_WIDTH - margin, WINDOW_HEIGHT - margin};     // Bottom-right
        default: return {WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f};
    }
}

float Game::getShipStartAngle(int index) const {
    // Point ships toward center
    switch (index) {
        case 0: return M_PI * 0.25f;       // 45 degrees
        case 1: return M_PI * 0.75f;       // 135 degrees
        case 2: return -M_PI * 0.25f;      // -45 degrees
        case 3: return -M_PI * 0.75f;      // -135 degrees
        default: return 0.0f;
    }
}
