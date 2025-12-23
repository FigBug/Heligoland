#include "Game.h"
#include <algorithm>
#include <cmath>

Game::Game() = default;

Game::~Game() = default;

bool Game::init()
{
    if (! SDL_Init (SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log ("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow ("Heligoland", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (! window)
    {
        SDL_Log ("Failed to create window: %s", SDL_GetError());
        return false;
    }

    sdlRenderer = SDL_CreateRenderer (window, nullptr);
    if (! sdlRenderer)
    {
        SDL_Log ("Failed to create renderer: %s", SDL_GetError());
        return false;
    }

    renderer = std::make_unique<Renderer> (sdlRenderer);

    // Create players (but not ships yet - those are created when game starts)
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        players[i] = std::make_unique<Player> (i);
        aiControllers[i] = std::make_unique<AIController>();
    }

    state = GameState::Title;
    running = true;
    lastFrameTime = SDL_GetTicks();

    return true;
}

void Game::run()
{
    while (running)
    {
        Uint64 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        // Cap delta time to avoid spiral of death
        if (dt > 0.1f)
            dt = 0.1f;

        handleEvents();
        update (dt);
        render();
    }
}

void Game::shutdown()
{
    ships = {};
    players = {};
    aiControllers = {};
    renderer.reset();

    if (sdlRenderer)
    {
        SDL_DestroyRenderer (sdlRenderer);
        sdlRenderer = nullptr;
    }

    if (window)
    {
        SDL_DestroyWindow (window);
        window = nullptr;
    }

    SDL_Quit();
}

void Game::handleEvents()
{
    SDL_Event event;
    while (SDL_PollEvent (&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
        }

        // Pass events to players for gamepad handling
        for (auto& player : players)
        {
            player->handleEvent (event);
        }
    }
}

void Game::update (float dt)
{
    // Update players for gamepad detection
    for (auto& player : players)
    {
        player->update();
    }

    switch (state)
    {
        case GameState::Title:
            updateTitle (dt);
            break;
        case GameState::Playing:
            updatePlaying (dt);
            break;
        case GameState::GameOver:
            updateGameOver (dt);
            break;
    }
}

void Game::updateTitle (float dt)
{
    // Check if any button is pressed to start the game
    if (anyButtonPressed())
    {
        startGame();
    }
}

bool Game::anyButtonPressed()
{
    for (auto& player : players)
    {
        if (player->isConnected() && player->getFireInput())
        {
            return true;
        }
    }
    return false;
}

void Game::startGame()
{
    // Create ships at starting positions
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        ships[i] = std::make_unique<Ship> (i, getShipStartPosition (i), getShipStartAngle (i));
    }

    shells.clear();
    winnerIndex = -1;
    gameOverTimer = 0.0f;
    gameStartDelay = 0.5f; // Ignore fire input for first 0.5 seconds

    // Initialize wind
    float windAngle = ((float) rand() / RAND_MAX) * 2.0f * M_PI;
    float windStrength = ((float) rand() / RAND_MAX);
    wind = Vec2::fromAngle (windAngle) * windStrength;
    targetWind = wind;
    windChangeTimer = WIND_CHANGE_INTERVAL;

    state = GameState::Playing;
}

void Game::updateWind (float dt)
{
    // Update wind change timer
    windChangeTimer -= dt;
    if (windChangeTimer <= 0)
    {
        // Pick new target wind
        float windAngle = ((float) rand() / RAND_MAX) * 2.0f * M_PI;
        float windStrength = ((float) rand() / RAND_MAX);
        targetWind = Vec2::fromAngle (windAngle) * windStrength;
        windChangeTimer = WIND_CHANGE_INTERVAL;
    }

    // Slowly lerp wind toward target
    wind.x += (targetWind.x - wind.x) * WIND_LERP_SPEED * dt;
    wind.y += (targetWind.y - wind.y) * WIND_LERP_SPEED * dt;
}

void Game::updatePlaying (float dt)
{
    // Update start delay
    if (gameStartDelay > 0)
    {
        gameStartDelay -= dt;
    }

    // Update wind
    updateWind (dt);

    // Update ships
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        if (! ships[i] || ! ships[i]->isAlive())
        {
            continue; // Skip dead ships
        }

        Vec2 moveInput, aimInput;
        bool fireInput = false;

        if (players[i]->isConnected())
        {
            moveInput = players[i]->getMoveInput();
            aimInput = players[i]->getAimInput();
            // Only accept fire input after start delay
            fireInput = (gameStartDelay <= 0) && players[i]->getFireInput();
        }
        else
        {
            // Find nearest living enemy ship for AI targeting
            const Ship* target = nullptr;
            float nearestDist = 999999.0f;
            for (int j = 0; j < NUM_SHIPS; ++j)
            {
                if (i != j && ships[j] && ships[j]->isAlive())
                {
                    float dist = (ships[j]->getPosition() - ships[i]->getPosition()).length();
                    if (dist < nearestDist)
                    {
                        nearestDist = dist;
                        target = ships[j].get();
                    }
                }
            }

            aiControllers[i]->update (dt, *ships[i], target, WINDOW_WIDTH, WINDOW_HEIGHT);
            moveInput = aiControllers[i]->getMoveInput();
            aimInput = aiControllers[i]->getAimInput();
            fireInput = aiControllers[i]->getFireInput();
        }

        ships[i]->update (dt, moveInput, aimInput, fireInput, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Collect pending shells from ship
        auto& pendingShells = ships[i]->getPendingShells();
        for (auto& shell : pendingShells)
        {
            shells.push_back (std::move (shell));
        }
        pendingShells.clear();
    }

    // Update shells
    updateShells (dt);

    // Check for collisions
    checkCollisions();

    // Check for game over
    checkGameOver();
}

void Game::updateGameOver (float dt)
{
    gameOverTimer += dt;
    if (gameOverTimer >= GAME_OVER_DELAY)
    {
        returnToTitle();
    }
}

void Game::returnToTitle()
{
    // Clear ships
    for (auto& ship : ships)
    {
        ship.reset();
    }
    shells.clear();
    state = GameState::Title;
}

void Game::updateShells (float dt)
{
    // Calculate wind drift force (5% of shell speed at max wind)
    // Shell speed is roughly 200, so max drift is 200 * 0.05 = 10 per second
    Vec2 windDrift = wind * 200.0f * MAX_WIND_DRIFT;

    for (auto& shell : shells)
    {
        shell.update (dt, windDrift);
    }

    // Remove dead shells
    shells.erase (
        std::remove_if (shells.begin(), shells.end(), [] (const Shell& s)
                        { return ! s.isAlive(); }),
        shells.end());
}

void Game::checkCollisions()
{
    // Shell-to-ship collisions (only when shell has landed/splashed)
    for (auto& shell : shells)
    {
        if (! shell.isAlive())
            continue;
        if (! shell.hasLanded())
            continue; // Shells only hit when they land

        for (auto& ship : ships)
        {
            if (! ship->isAlive())
                continue;
            if (ship->getPlayerIndex() == shell.getOwnerIndex())
                continue; // Don't hit own ship

            // Check if ship is within splash radius
            Vec2 diff = shell.getPosition() - ship->getPosition();
            float dist = diff.length();
            float hitRadius = ship->getLength() / 2.0f; // Approximate ship as circle

            if (dist < hitRadius + shell.getSplashRadius())
            {
                ship->takeDamage (SHELL_DAMAGE);
                shell.kill();
                break;
            }
        }

        // Kill landed shells after checking for hits (they splash and disappear)
        if (shell.hasLanded())
        {
            shell.kill();
        }
    }

    // Ship-to-ship collisions using OBB (Separating Axis Theorem)
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        if (! ships[i]->isAlive())
            continue;

        for (int j = i + 1; j < NUM_SHIPS; ++j)
        {
            if (! ships[j]->isAlive())
                continue;

            // Get corners of both ships
            auto cornersA = ships[i]->getCorners();
            auto cornersB = ships[j]->getCorners();

            // Check if OBBs overlap using SAT
            float minOverlap = 999999.0f;
            Vec2 minAxis;

            bool separated = false;

            // Test 4 axes (2 from each rectangle - perpendicular to edges)
            Vec2 axes[4] = {
                (cornersA[1] - cornersA[0]).normalized(), // Ship A forward axis
                (cornersA[3] - cornersA[0]).normalized(), // Ship A side axis
                (cornersB[1] - cornersB[0]).normalized(), // Ship B forward axis
                (cornersB[3] - cornersB[0]).normalized() // Ship B side axis
            };

            for (int a = 0; a < 4 && ! separated; ++a)
            {
                Vec2 axis = axes[a];
                // Get perpendicular for proper separation axis
                Vec2 perpAxis = { -axis.y, axis.x };

                // Project both shapes onto axis
                float minA = 999999.0f, maxA = -999999.0f;
                float minB = 999999.0f, maxB = -999999.0f;

                for (int c = 0; c < 4; ++c)
                {
                    float projA = cornersA[c].dot (perpAxis);
                    float projB = cornersB[c].dot (perpAxis);
                    minA = std::min (minA, projA);
                    maxA = std::max (maxA, projA);
                    minB = std::min (minB, projB);
                    maxB = std::max (maxB, projB);
                }

                // Check for separation
                if (maxA < minB || maxB < minA)
                {
                    separated = true;
                }
                else
                {
                    // Calculate overlap on this axis
                    float overlap = std::min (maxA - minB, maxB - minA);
                    if (overlap < minOverlap)
                    {
                        minOverlap = overlap;
                        minAxis = perpAxis;
                    }
                }
            }

            if (! separated)
            {
                // Collision detected!
                // Calculate relative speed for damage
                Vec2 relVel = ships[i]->getVelocity() - ships[j]->getVelocity();
                float impactSpeed = relVel.length();

                // Damage proportional to impact speed
                float damage = impactSpeed;
                ships[i]->takeDamage (damage);
                ships[j]->takeDamage (damage);

                // Determine push direction (from i to j)
                Vec2 diff = ships[j]->getPosition() - ships[i]->getPosition();
                if (diff.dot (minAxis) < 0)
                {
                    minAxis = minAxis * -1.0f;
                }

                // Push ships apart and stop them
                float pushDist = minOverlap / 2.0f + 2.0f;
                ships[i]->stopAndPushApart (minAxis * -1.0f, pushDist);
                ships[j]->stopAndPushApart (minAxis, pushDist);

                // Full stop both ships (reset throttle)
                ships[i]->fullStop();
                ships[j]->fullStop();
            }
        }
    }
}

void Game::checkGameOver()
{
    int aliveCount = 0;
    int lastAlive = -1;

    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        if (ships[i] && ships[i]->isAlive())
        {
            aliveCount++;
            lastAlive = i;
        }
    }

    if (aliveCount <= 1)
    {
        winnerIndex = lastAlive;
        gameOverTimer = 0.0f;
        state = GameState::GameOver;
    }
}

void Game::render()
{
    renderer->clear();

    switch (state)
    {
        case GameState::Title:
            renderTitle();
            break;
        case GameState::Playing:
            renderPlaying();
            break;
        case GameState::GameOver:
            renderPlaying(); // Still show the game
            renderGameOver(); // Overlay the game over text
            break;
    }

    renderer->present();
}

void Game::renderTitle()
{
    // Draw title
    SDL_Color titleColor = { 255, 255, 255, 255 };
    renderer->drawTextCentered ("HELIGOLAND", { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 3.0f }, 8.0f, titleColor);

    // Draw connected players
    SDL_Color subtitleColor = { 200, 200, 200, 255 };
    int connectedCount = 0;
    for (auto& player : players)
    {
        if (player->isConnected())
        {
            connectedCount++;
        }
    }

    std::string playerText = std::to_string (connectedCount) + " PLAYERS CONNECTED";
    renderer->drawTextCentered (playerText, { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f }, 3.0f, subtitleColor);

    // Draw player slots
    float slotY = WINDOW_HEIGHT * 0.6f;
    float slotSpacing = 80.0f;
    float startX = WINDOW_WIDTH / 2.0f - (NUM_SHIPS - 1) * slotSpacing / 2.0f;

    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        Vec2 slotPos = { startX + i * slotSpacing, slotY };
        SDL_Color slotColor;

        if (players[i]->isConnected())
        {
            // Use player color
            switch (i)
            {
                case 0:
                    slotColor = { 255, 100, 100, 255 };
                    break; // Red
                case 1:
                    slotColor = { 100, 100, 255, 255 };
                    break; // Blue
                case 2:
                    slotColor = { 100, 255, 100, 255 };
                    break; // Green
                case 3:
                    slotColor = { 255, 255, 100, 255 };
                    break; // Yellow
                default:
                    slotColor = { 200, 200, 200, 255 };
                    break;
            }
            renderer->drawFilledRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
            renderer->drawTextCentered ("P" + std::to_string (i + 1), slotPos, 3.0f, { 0, 0, 0, 255 });
        }
        else
        {
            slotColor = { 80, 80, 80, 255 };
            renderer->drawRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
            renderer->drawTextCentered ("AI", slotPos, 2.0f, slotColor);
        }
    }

    // Draw start instruction
    SDL_Color instructColor = { 150, 150, 150, 255 };
    renderer->drawTextCentered ("PRESS TRIGGER TO START", { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT * 0.85f }, 2.0f, instructColor);
}

void Game::renderPlaying()
{
    // Draw bubble trails first (behind ships)
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawBubbleTrail (*ship);
        }
    }

    // Draw shells
    for (const auto& shell : shells)
    {
        renderer->drawShell (shell);
    }

    // Draw ships
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawShip (*ship);
            // Crosshair is grey if not ready to fire, ship color if ready
            SDL_Color crosshairColor = ship->isReadyToFire()
                                           ? ship->getColor()
                                           : SDL_Color { 100, 100, 100, 255 };
            renderer->drawCrosshair (ship->getCrosshairPosition(), crosshairColor);
        }
    }

    // Draw HUD for each ship
    int slot = 0;
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawShipHUD (*ship, slot, NUM_SHIPS, WINDOW_WIDTH);
        }
        slot++;
    }

    // Draw wind indicator
    renderer->drawWindIndicator (wind, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void Game::renderGameOver()
{
    SDL_Color textColor = { 255, 255, 255, 255 };

    if (winnerIndex >= 0)
    {
        std::string winText = "PLAYER " + std::to_string (winnerIndex + 1) + " WINS!";
        renderer->drawTextCentered (winText, { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f }, 5.0f, textColor);
    }
    else
    {
        renderer->drawTextCentered ("DRAW!", { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f }, 5.0f, textColor);
    }
}

Vec2 Game::getShipStartPosition (int index) const
{
    float margin = 150.0f;
    switch (index)
    {
        case 0:
            return { margin, margin }; // Top-left
        case 1:
            return { WINDOW_WIDTH - margin, margin }; // Top-right
        case 2:
            return { margin, WINDOW_HEIGHT - margin }; // Bottom-left
        case 3:
            return { WINDOW_WIDTH - margin, WINDOW_HEIGHT - margin }; // Bottom-right
        default:
            return { WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f };
    }
}

float Game::getShipStartAngle (int index) const
{
    // Point ships toward center
    switch (index)
    {
        case 0:
            return M_PI * 0.25f; // 45 degrees
        case 1:
            return M_PI * 0.75f; // 135 degrees
        case 2:
            return -M_PI * 0.25f; // -45 degrees
        case 3:
            return -M_PI * 0.75f; // -135 degrees
        default:
            return 0.0f;
    }
}
