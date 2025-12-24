#include "Game.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>

Game::Game() = default;

Game::~Game() = default;

bool Game::init()
{
    SetConfigFlags (FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow (WINDOW_WIDTH, WINDOW_HEIGHT, "Heligoland");
    SetTargetFPS (60);
    HideCursor();

    renderer = std::make_unique<Renderer>();
    audio = std::make_unique<Audio>();

    if (! audio->init())
    {
        // Audio is optional - continue without it
        audio.reset();
    }

    // Create players (but not ships yet - those are created when game starts)
    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        players[i] = std::make_unique<Player> (i);
        aiControllers[i] = std::make_unique<AIController>();
    }

    state = GameState::Title;
    running = true;
    lastFrameTime = GetTime();

    return true;
}

void Game::run()
{
    while (running && !WindowShouldClose())
    {
        double currentTime = GetTime();
        float dt = (float) (currentTime - lastFrameTime);
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
    if (audio)
        audio->shutdown();
    audio.reset();

    CloseWindow();
}

void Game::handleEvents()
{
    if (IsKeyPressed (KEY_ESCAPE))
    {
        running = false;
    }
}

void Game::update (float dt)
{
    // Update total time for animations
    time += dt;

    // Update audio
    if (audio)
    {
        audio->update (dt);
    }

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
    // Check for mode switching with bumpers or arrow keys
    static bool leftWasPressed = false;
    static bool rightWasPressed = false;

    bool leftPressed = IsKeyDown (KEY_LEFT);
    bool rightPressed = IsKeyDown (KEY_RIGHT);

    // Check all gamepads for bumper presses
    for (int i = 0; i < 4; ++i)
    {
        if (IsGamepadAvailable (i))
        {
            if (IsGamepadButtonDown (i, GAMEPAD_BUTTON_LEFT_TRIGGER_1))
                leftPressed = true;
            if (IsGamepadButtonDown (i, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
                rightPressed = true;
        }
    }

    // Cycle mode on press (with edge detection)
    if (leftPressed && ! leftWasPressed)
        cycleGameMode (-1);
    if (rightPressed && ! rightWasPressed)
        cycleGameMode (1);

    leftWasPressed = leftPressed;
    rightWasPressed = rightPressed;

    // Check if any button or click is pressed to start the game
    if (anyButtonPressed())
    {
        startGame();
    }
}

bool Game::anyButtonPressed()
{
    // Mouse click starts game
    if (IsMouseButtonPressed (MOUSE_BUTTON_LEFT))
        return true;

    // Check for face buttons only (not bumpers, which are used for mode switching)
    for (int i = 0; i < 4; ++i)
    {
        if (IsGamepadAvailable (i))
        {
            if (IsGamepadButtonPressed (i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||  // A / Cross
                IsGamepadButtonPressed (i, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || // B / Circle
                IsGamepadButtonPressed (i, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||  // X / Square
                IsGamepadButtonPressed (i, GAMEPAD_BUTTON_RIGHT_FACE_UP))      // Y / Triangle
            {
                return true;
            }
        }
    }
    return false;
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
                // Only target enemies that are alive and not sinking
                if (areEnemies (i, j) && ships[j] && ships[j]->isAlive() && ! ships[j]->isSinking())
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

        // Set crosshair directly for mouse aiming
        if (players[i]->isUsingMouse())
        {
            ships[i]->setCrosshairPosition (players[i]->getMousePosition());
        }

        // Collect pending shells from ship
        auto& pendingShells = ships[i]->getPendingShells();
        if (! pendingShells.empty() && audio)
        {
            // Play cannon sound at ship position
            audio->playCannon (ships[i]->getPosition().x, arenaWidth);
        }
        for (auto& shell : pendingShells)
        {
            shells.push_back (std::move (shell));
        }
        pendingShells.clear();
    }

    // Update engine volume based on average throttle of alive ships
    if (audio)
    {
        float totalThrottle = 0.0f;
        int aliveCount = 0;
        for (int i = 0; i < NUM_SHIPS; ++i)
        {
            if (ships[i] && ships[i]->isAlive())
            {
                totalThrottle += std::abs (ships[i]->getThrottle());
                aliveCount++;
            }
        }
        float avgThrottle = aliveCount > 0 ? totalThrottle / aliveCount : 0.0f;
        audio->setEngineVolume (0.3f + avgThrottle * 0.7f); // Base volume + throttle boost
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
                float arenaWidth, arenaHeight;
                getWindowSize (arenaWidth, arenaHeight);

                ship->takeDamage (SHELL_DAMAGE);

                // Spawn hit explosion
                Explosion explosion;
                explosion.position = shell.getPosition();
                explosion.isHit = true;
                explosions.push_back (explosion);

                // Play explosion sound
                if (audio)
                {
                    audio->playExplosion (shell.getPosition().x, arenaWidth);
                }

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
            float arenaWidth, arenaHeight;
            getWindowSize (arenaWidth, arenaHeight);

            // Spawn splash (miss) - hits are handled above
            Explosion splash;
            splash.position = shell.getPosition();
            splash.isHit = false;
            explosions.push_back (splash);

            // Play splash sound
            if (audio)
            {
                audio->playSplash (shell.getPosition().x, arenaWidth);
            }

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

                // Play collision sound at midpoint between ships
                if (audio && impactSpeed > 10.0f) // Only play for significant impacts
                {
                    float arenaWidth, arenaHeight;
                    getWindowSize (arenaWidth, arenaHeight);
                    Vec2 midpoint = (ships[i]->getPosition() + ships[j]->getPosition()) * 0.5f;
                    audio->playCollision (midpoint.x, arenaWidth);
                }

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
    // Helper to check if a ship can still fight (alive and not sinking)
    auto canFight = [this] (int i) -> bool
    {
        return ships[i] && ships[i]->isAlive() && ! ships[i]->isSinking();
    };

    if (gameMode == GameMode::Teams)
    {
        // Count fighting ships per team
        int team0Alive = 0;
        int team1Alive = 0;

        for (int i = 0; i < NUM_SHIPS; ++i)
        {
            if (canFight (i))
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

            // Record the win
            if (winnerIndex >= 0)
                teamWins[winnerIndex]++;

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
            if (canFight (i))
            {
                aliveCount++;
                lastAlive = i;
            }
        }

        if (aliveCount <= 1)
        {
            winnerIndex = lastAlive;

            // Record the win
            if (winnerIndex >= 0)
                playerWins[winnerIndex]++;

            gameOverTimer = 0.0f;
            state = GameState::GameOver;
        }
    }
}

void Game::render()
{
    BeginDrawing();

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

    EndDrawing();
}

void Game::renderTitle()
{
    float w, h;
    getWindowSize (w, h);

    // Draw title
    Color titleColor = { 255, 255, 255, 255 };
    renderer->drawTextCentered ("HELIGOLAND", { w / 2.0f, h / 3.0f }, 8.0f, titleColor);

    // Draw connected players
    Color subtitleColor = { 200, 200, 200, 255 };
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
    Color modeColor = { 255, 220, 100, 255 };
    std::string modeText = gameMode == GameMode::FFA ? "FREE FOR ALL" : "2 VS 2";
    renderer->drawTextCentered (modeText, { w / 2.0f, h * 0.55f }, 4.0f, modeColor);

    Color modeHintColor = { 120, 120, 120, 255 };
    renderer->drawTextCentered ("LEFT - RIGHT TO CHANGE MODE", { w / 2.0f, h * 0.62f }, 1.5f, modeHintColor);

    // Draw player slots
    float slotY = h * 0.72f;
    float slotSpacing = 80.0f;
    float startX = w / 2.0f - (NUM_SHIPS - 1) * slotSpacing / 2.0f;

    for (int i = 0; i < NUM_SHIPS; ++i)
    {
        Vec2 slotPos = { startX + i * slotSpacing, slotY };
        Color slotColor;

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
    Color instructColor = { 150, 150, 150, 255 };
    renderer->drawTextCentered ("CLICK OR PRESS ANY BUTTON TO START", { w / 2.0f, h * 0.90f }, 2.0f, instructColor);
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
        if (ship && ship->isAlive() && ! ship->isSinking())
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
        if (ship && ship->isAlive() && ! ship->isSinking())
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

    Color textColor = { 255, 255, 255, 255 };
    Color statsColor = { 200, 200, 200, 255 };

    if (winnerIndex >= 0)
    {
        std::string winText;
        if (gameMode == GameMode::Teams)
            winText = "TEAM " + std::to_string (winnerIndex + 1) + " WINS!";
        else
            winText = "PLAYER " + std::to_string (winnerIndex + 1) + " WINS!";

        renderer->drawTextCentered (winText, { w / 2.0f, h / 2.0f - 30.0f }, 5.0f, textColor);
    }
    else
    {
        renderer->drawTextCentered ("DRAW!", { w / 2.0f, h / 2.0f - 30.0f }, 5.0f, textColor);
    }

    // Display win statistics
    if (gameMode == GameMode::Teams)
    {
        std::string statsText = "TEAM 1: " + std::to_string (teamWins[0]) +
                                "  -  TEAM 2: " + std::to_string (teamWins[1]);
        renderer->drawTextCentered (statsText, { w / 2.0f, h / 2.0f + 40.0f }, 2.5f, statsColor);
    }
    else
    {
        std::string statsText = "P1: " + std::to_string (playerWins[0]) +
                                "  P2: " + std::to_string (playerWins[1]) +
                                "  P3: " + std::to_string (playerWins[2]) +
                                "  P4: " + std::to_string (playerWins[3]);
        renderer->drawTextCentered (statsText, { w / 2.0f, h / 2.0f + 40.0f }, 2.5f, statsColor);
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
    width = (float) GetScreenWidth();
    height = (float) GetScreenHeight();
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
