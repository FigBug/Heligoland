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

    window = SDL_CreateWindow ("Heligoland", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

    // Set up logical presentation for high DPI displays
    setupLogicalPresentation();

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
        else if (event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            handleWindowResize();
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
    // Update total time for animations
    time += dt;

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
    // Check for mode switching with bumpers/triggers
    static bool leftBumperWasPressed = false;
    static bool rightBumperWasPressed = false;

    bool leftBumperPressed = false;
    bool rightBumperPressed = false;

    for (auto& player : players)
    {
        if (player->isConnected())
        {
            // Get gamepad for this player (we need to check bumpers directly)
            int numGamepads = 0;
            SDL_JoystickID* gamepads = SDL_GetGamepads (&numGamepads);
            if (gamepads && player->getPlayerIndex() < numGamepads)
            {
                SDL_Gamepad* gp = SDL_OpenGamepad (gamepads[player->getPlayerIndex()]);
                if (gp)
                {
                    if (SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
                        leftBumperPressed = true;
                    if (SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
                        rightBumperPressed = true;
                    SDL_CloseGamepad (gp);
                }
            }
            SDL_free (gamepads);
        }
    }

    // Cycle mode on bumper press (with edge detection)
    if (leftBumperPressed && ! leftBumperWasPressed)
        cycleGameMode (-1);
    if (rightBumperPressed && ! rightBumperWasPressed)
        cycleGameMode (1);

    leftBumperWasPressed = leftBumperPressed;
    rightBumperWasPressed = rightBumperPressed;

    // Check if any face button is pressed to start the game
    if (anyButtonPressed())
    {
        startGame();
    }
}

bool Game::anyButtonPressed()
{
    // Check for face buttons only (not bumpers, which are used for mode switching)
    int numGamepads = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads (&numGamepads);
    if (! gamepads)
        return false;

    bool pressed = false;
    for (int i = 0; i < numGamepads && ! pressed; ++i)
    {
        SDL_Gamepad* gp = SDL_OpenGamepad (gamepads[i]);
        if (gp)
        {
            pressed = SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_SOUTH) ||
                      SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_EAST) ||
                      SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_WEST) ||
                      SDL_GetGamepadButton (gp, SDL_GAMEPAD_BUTTON_NORTH);
            SDL_CloseGamepad (gp);
        }
    }
    SDL_free (gamepads);
    return pressed;
}

void Game::startGame()
{
    // Create ships at starting positions
    bool isTeamMode = (gameMode == GameMode::Teams);
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        ships[i] = std::make_unique<Ship> (i, getShipStartPosition (i), getShipStartAngle (i), isTeamMode);
    }

    shells.clear();
    explosions.clear();
    winnerIndex = -1;
    gameOverTimer = 0.0f;
    gameStartDelay = 0.5f; // Ignore fire input for first 0.5 seconds

    // Initialize wind (minimum 25% strength)
    float windAngle = ((float) rand() / RAND_MAX) * 2.0f * pi;
    float windStrength = 0.25f + ((float) rand() / RAND_MAX) * 0.75f;
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
        // Pick new target wind - minor adjustment from current wind
        // Small angle change (up to 30 degrees either way)
        float currentAngle = std::atan2 (wind.y, wind.x);
        float angleChange = ((float) rand() / RAND_MAX - 0.5f) * pi / 3.0f; // +/- 30 degrees
        float newAngle = currentAngle + angleChange;

        // Small strength change (up to 20% either way), minimum 25%
        float currentStrength = wind.length();
        float strengthChange = ((float) rand() / RAND_MAX - 0.5f) * 0.4f;
        float newStrength = std::clamp (currentStrength + strengthChange, 0.25f, 1.0f);

        targetWind = Vec2::fromAngle (newAngle) * newStrength;
        windChangeTimer = WIND_CHANGE_INTERVAL;
    }

    // Slowly lerp wind toward target
    wind.x += (targetWind.x - wind.x) * WIND_LERP_SPEED * dt;
    wind.y += (targetWind.y - wind.y) * WIND_LERP_SPEED * dt;
}

void Game::updatePlaying (float dt)
{
    float arenaWidth, arenaHeight;
    getWindowSize (arenaWidth, arenaHeight);

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
                // Only target enemies (in team mode, skip teammates)
                if (areEnemies (i, j) && ships[j] && ships[j]->isAlive())
                {
                    float dist = (ships[j]->getPosition() - ships[i]->getPosition()).length();
                    if (dist < nearestDist)
                    {
                        nearestDist = dist;
                        target = ships[j].get();
                    }
                }
            }

            aiControllers[i]->update (dt, *ships[i], target, arenaWidth, arenaHeight);
            moveInput = aiControllers[i]->getMoveInput();
            aimInput = aiControllers[i]->getAimInput();
            fireInput = aiControllers[i]->getFireInput();
        }

        ships[i]->update (dt, moveInput, aimInput, fireInput, arenaWidth, arenaHeight, wind);

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

    // Update explosions
    for (auto& explosion : explosions)
    {
        explosion.timer += dt;
    }
    explosions.erase (
        std::remove_if (explosions.begin(), explosions.end(), [] (const Explosion& e)
                        { return ! e.isAlive(); }),
        explosions.end());

    // Check for game over
    checkGameOver();
}

void Game::updateGameOver (float dt)
{
    float arenaWidth, arenaHeight;
    getWindowSize (arenaWidth, arenaHeight);

    // Keep updating ships (for smoke effects)
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        if (ships[i] && ships[i]->isAlive())
        {
            ships[i]->update (dt, { 0, 0 }, { 0, 0 }, false, arenaWidth, arenaHeight, wind);
        }
    }

    // Keep updating shells so they land and disappear
    updateShells (dt);

    // Kill landed shells and spawn splashes (normally done in checkCollisions)
    for (auto& shell : shells)
    {
        if (shell.isAlive() && shell.hasLanded())
        {
            Explosion splash;
            splash.position = shell.getPosition();
            splash.isHit = false;
            explosions.push_back (splash);
            shell.kill();
        }
    }

    // Keep updating explosions
    for (auto& explosion : explosions)
    {
        explosion.timer += dt;
    }
    explosions.erase (
        std::remove_if (explosions.begin(), explosions.end(), [] (const Explosion& e)
                        { return ! e.isAlive(); }),
        explosions.end());

    gameOverTimer += dt;
    if (gameOverTimer >= GAME_OVER_RETURN_DELAY)
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
    explosions.clear();
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

                // Spawn hit explosion
                Explosion explosion;
                explosion.position = shell.getPosition();
                explosion.isHit = true;
                explosions.push_back (explosion);

                // Check if ship was sunk
                if (! ship->isAlive())
                {
                    // Big explosion for sinking
                    Explosion sinkExplosion;
                    sinkExplosion.position = ship->getPosition();
                    sinkExplosion.isHit = true;
                    sinkExplosion.maxRadius = 80.0f;
                    sinkExplosion.duration = 1.0f;
                    explosions.push_back (sinkExplosion);
                }

                shell.kill();
                break;
            }
        }

        // Kill landed shells after checking for hits (they splash and disappear)
        if (shell.hasLanded() && shell.isAlive())
        {
            // Spawn splash (miss) - hits are handled above
            Explosion splash;
            splash.position = shell.getPosition();
            splash.isHit = false;
            explosions.push_back (splash);

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
                Vec2 velA = ships[i]->getVelocity();
                Vec2 velB = ships[j]->getVelocity();

                // Calculate relative speed for damage
                Vec2 relVel = velA - velB;
                float impactSpeed = relVel.length();

                // Damage proportional to impact speed
                float damage = impactSpeed;
                ships[i]->takeDamage (damage);
                ships[j]->takeDamage (damage);

                // Determine collision normal (from i to j)
                Vec2 diff = ships[j]->getPosition() - ships[i]->getPosition();
                if (diff.dot (minAxis) < 0)
                {
                    minAxis = minAxis * -1.0f;
                }
                Vec2 collisionNormal = minAxis;

                // Push ships apart first
                float pushDist = minOverlap / 2.0f + 2.0f;
                ships[i]->applyCollision (collisionNormal * -1.0f, pushDist, velA, velB);
                ships[j]->applyCollision (collisionNormal, pushDist, velB, velA);
            }
        }
    }
}

void Game::checkGameOver()
{
    if (gameMode == GameMode::Teams)
    {
        // Count alive ships per team
        int team0Alive = 0;
        int team1Alive = 0;

        for (int i = 0; i < NUM_SHIPS; ++i)
        {
            if (ships[i] && ships[i]->isAlive())
            {
                if (getTeam (i) == 0)
                    team0Alive++;
                else
                    team1Alive++;
            }
        }

        // Game ends when one team is eliminated
        if (team0Alive == 0 || team1Alive == 0)
        {
            if (team0Alive == 0 && team1Alive == 0)
                winnerIndex = -1;  // Draw
            else if (team0Alive > 0)
                winnerIndex = 0;   // Team 1 wins (index 0)
            else
                winnerIndex = 1;   // Team 2 wins (index 1)

            gameOverTimer = 0.0f;
            state = GameState::GameOver;
        }
    }
    else
    {
        // FFA mode - last ship standing wins
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
}

void Game::render()
{
    float w, h;
    getWindowSize (w, h);
    renderer->drawWater (time, w, h);

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
    float w, h;
    getWindowSize (w, h);

    // Draw title
    SDL_Color titleColor = { 255, 255, 255, 255 };
    renderer->drawTextCentered ("HELIGOLAND", { w / 2.0f, h / 3.0f }, 8.0f, titleColor);

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
    renderer->drawTextCentered (playerText, { w / 2.0f, h * 0.45f }, 3.0f, subtitleColor);

    // Draw game mode selector
    SDL_Color modeColor = { 255, 220, 100, 255 };
    std::string modeText = gameMode == GameMode::FFA ? "FREE FOR ALL" : "2 VS 2";
    renderer->drawTextCentered (modeText, { w / 2.0f, h * 0.55f }, 4.0f, modeColor);

    SDL_Color modeHintColor = { 120, 120, 120, 255 };
    renderer->drawTextCentered ("LB - RB TO CHANGE MODE", { w / 2.0f, h * 0.62f }, 1.5f, modeHintColor);

    // Draw player slots
    float slotY = h * 0.72f;
    float slotSpacing = 80.0f;
    float startX = w / 2.0f - (NUM_SHIPS - 1) * slotSpacing / 2.0f;

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
    renderer->drawTextCentered ("PRESS ANY BUTTON TO START", { w / 2.0f, h * 0.90f }, 2.0f, instructColor);
}

void Game::renderPlaying()
{
    float w, h;
    getWindowSize (w, h);

    // Draw bubble trails first (behind everything)
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawBubbleTrail (*ship);
        }
    }

    // Draw ships
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawShip (*ship);
        }
    }

    // Draw smoke (above ships)
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawSmoke (*ship);
        }
    }

    // Draw shells (on top of ships)
    for (const auto& shell : shells)
    {
        renderer->drawShell (shell);
    }

    // Draw explosions
    for (const auto& explosion : explosions)
    {
        renderer->drawExplosion (explosion);
    }

    // Draw crosshairs (on top of everything)
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            renderer->drawCrosshair (*ship);
        }
    }

    // Draw HUD for each ship
    float hudWidth = 200.0f;
    float hudHeight = 50.0f;
    float hudSpacing = 20.0f;
    float hudTotalWidth = NUM_SHIPS * hudWidth + (NUM_SHIPS - 1) * hudSpacing;
    float hudStartX = (w - hudTotalWidth) / 2.0f;
    float hudY = 10.0f;

    int slot = 0;
    for (const auto& ship : ships)
    {
        if (ship && ship->isAlive())
        {
            // Check if any ship is under this HUD panel
            float hudX = hudStartX + slot * (hudWidth + hudSpacing);
            float alpha = 1.0f;

            for (const auto& otherShip : ships)
            {
                if (otherShip && otherShip->isAlive())
                {
                    Vec2 pos = otherShip->getPosition();
                    // Check if ship position is within HUD bounds (with some margin)
                    float margin = otherShip->getLength() / 2.0f;
                    if (pos.x > hudX - margin && pos.x < hudX + hudWidth + margin &&
                        pos.y > hudY - margin && pos.y < hudY + hudHeight + margin)
                    {
                        alpha = 0.25f;
                        break;
                    }
                }
            }

            renderer->drawShipHUD (*ship, slot, NUM_SHIPS, w, alpha);
        }
        slot++;
    }

    // Draw wind indicator
    renderer->drawWindIndicator (wind, w, h);
}

void Game::renderGameOver()
{
    // Wait before showing text so player can see the final explosion
    if (gameOverTimer < GAME_OVER_TEXT_DELAY)
    {
        return;
    }

    float w, h;
    getWindowSize (w, h);

    SDL_Color textColor = { 255, 255, 255, 255 };

    if (winnerIndex >= 0)
    {
        std::string winText;
        if (gameMode == GameMode::Teams)
            winText = "TEAM " + std::to_string (winnerIndex + 1) + " WINS!";
        else
            winText = "PLAYER " + std::to_string (winnerIndex + 1) + " WINS!";

        renderer->drawTextCentered (winText, { w / 2.0f, h / 2.0f }, 5.0f, textColor);
    }
    else
    {
        renderer->drawTextCentered ("DRAW!", { w / 2.0f, h / 2.0f }, 5.0f, textColor);
    }
}

Vec2 Game::getShipStartPosition (int index) const
{
    float w, h;
    getWindowSize (w, h);

    if (gameMode == GameMode::Teams)
    {
        // 2v2 mode: teams on left and right sides
        float margin = w * 0.15f;
        float verticalSpacing = h * 0.25f;

        if (index < 2)
        {
            // Team 0 (players 0, 1) on left
            float x = margin;
            float y = h / 2.0f + (index == 0 ? -verticalSpacing : verticalSpacing);
            return { x, y };
        }
        else
        {
            // Team 1 (players 2, 3) on right
            float x = w - margin;
            float y = h / 2.0f + (index == 2 ? -verticalSpacing : verticalSpacing);
            return { x, y };
        }
    }

    // FFA mode: Place ships in a circle around the center
    Vec2 center = { w / 2.0f, h / 2.0f };
    float radius = std::min (w, h) * 0.35f;

    float angleOffset = -pi / 2.0f;
    float angle = angleOffset + (index * 2.0f * pi / NUM_SHIPS);

    return center + Vec2::fromAngle (angle) * radius;
}

float Game::getShipStartAngle (int index) const
{
    if (gameMode == GameMode::Teams)
    {
        // 2v2 mode: teams face each other
        if (index < 2)
            return 0.0f;  // Team 0 faces right
        else
            return pi;    // Team 1 faces left
    }

    // FFA mode: Point ships toward center
    float angleOffset = -pi / 2.0f;
    float posAngle = angleOffset + (index * 2.0f * pi / NUM_SHIPS);
    return posAngle + pi;
}

void Game::getWindowSize (float& width, float& height) const
{
    int w, h;
    SDL_GetWindowSize (window, &w, &h);
    width = (float) w;
    height = (float) h;
}

void Game::setupLogicalPresentation()
{
    // Get logical size (points) and physical size (pixels)
    int logicalW, logicalH;
    int pixelW, pixelH;
    SDL_GetWindowSize (window, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels (window, &pixelW, &pixelH);

    // Calculate scale for HiDPI displays
    float scaleX = (float) pixelW / logicalW;
    float scaleY = (float) pixelH / logicalH;

    SDL_SetRenderScale (sdlRenderer, scaleX, scaleY);
}

void Game::handleWindowResize()
{
    // Update logical presentation when window is resized
    setupLogicalPresentation();
}

int Game::getTeam (int playerIndex) const
{
    // Team 0: players 0, 1 (left side)
    // Team 1: players 2, 3 (right side)
    return playerIndex < 2 ? 0 : 1;
}

bool Game::areEnemies (int playerA, int playerB) const
{
    if (gameMode == GameMode::FFA)
        return playerA != playerB;  // Everyone is an enemy in FFA

    return getTeam (playerA) != getTeam (playerB);
}

void Game::cycleGameMode (int direction)
{
    int mode = static_cast<int> (gameMode);
    mode += direction;

    // Wrap around
    if (mode < 0)
        mode = 1;
    else if (mode > 1)
        mode = 0;

    gameMode = static_cast<GameMode> (mode);
}
