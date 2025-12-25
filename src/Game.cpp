#include "Game.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <vector>

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
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        players[i] = std::make_unique<Player> (i);
    }
    for (int i = 0; i < MAX_SHIPS; ++i)
    {
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
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        players[i]->update();
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
    bool isTeamMode = (gameMode == GameMode::Teams || gameMode == GameMode::Battle);
    int numShips = getNumShipsForMode();
    for (int i = 0; i < numShips; ++i)
    {
        int team = isTeamMode ? getTeam (i) : -1;
        ships[i] = std::make_unique<Ship> (i, getShipStartPosition (i), getShipStartAngle (i), team);
    }

    shells.clear();
    explosions.clear();
    winnerIndex = -1;
    gameOverTimer = 0.0f;
    gameStartDelay = Config::gameStartDelay;

    // Initialize wind (minimum strength)
    float windAngle = ((float) rand() / RAND_MAX) * 2.0f * pi;
    float windStrength = Config::windMinStrength + ((float) rand() / RAND_MAX) * (1.0f - Config::windMinStrength);
    wind = Vec2::fromAngle (windAngle) * windStrength;
    targetWind = wind;
    windChangeTimer = Config::windChangeInterval;

    state = GameState::Playing;
}

void Game::updateWind (float dt)
{
    // Update wind change timer
    windChangeTimer -= dt;
    if (windChangeTimer <= 0)
    {
        // Pick new target wind - minor adjustment from current wind
        float currentAngle = std::atan2 (wind.y, wind.x);
        float angleChange = ((float) rand() / RAND_MAX - 0.5f) * Config::windAngleChangeMax * 2.0f;
        float newAngle = currentAngle + angleChange;

        // Small strength change, minimum strength enforced
        float currentStrength = wind.length();
        float strengthChange = ((float) rand() / RAND_MAX - 0.5f) * Config::windStrengthChangeMax;
        float newStrength = std::clamp (currentStrength + strengthChange, Config::windMinStrength, 1.0f);

        targetWind = Vec2::fromAngle (newAngle) * newStrength;
        windChangeTimer = Config::windChangeInterval;
    }

    // Slowly lerp wind toward target
    wind.x += (targetWind.x - wind.x) * Config::windLerpSpeed * dt;
    wind.y += (targetWind.y - wind.y) * Config::windLerpSpeed * dt;
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
    int numShips = getNumShipsForMode();
    for (int shipIdx = 0; shipIdx < numShips; ++shipIdx)
    {
        if (! ships[shipIdx] || ! ships[shipIdx]->isAlive())
        {
            continue; // Skip dead ships
        }

        Vec2 moveInput, aimInput;
        bool fireInput = false;

        // Check if this ship is controlled by a human player
        int playerIdx = getPlayerIndexForShip (shipIdx);
        bool isHumanControlled = (playerIdx >= 0 && players[playerIdx]->isConnected());

        if (isHumanControlled)
        {
            moveInput = players[playerIdx]->getMoveInput();
            aimInput = players[playerIdx]->getAimInput();
            // Only accept fire input after start delay
            fireInput = (gameStartDelay <= 0) && players[playerIdx]->getFireInput();
        }
        else
        {
            // Find all living enemy ships for AI
            std::vector<const Ship*> enemies;
            for (int j = 0; j < numShips; ++j)
            {
                if (areEnemies (shipIdx, j) && ships[j] && ships[j]->isAlive() && ! ships[j]->isSinking())
                {
                    enemies.push_back (ships[j].get());
                }
            }

            aiControllers[shipIdx]->update (dt, *ships[shipIdx], enemies, shells, arenaWidth, arenaHeight);
            moveInput = aiControllers[shipIdx]->getMoveInput();
            aimInput = aiControllers[shipIdx]->getAimInput();
            fireInput = aiControllers[shipIdx]->getFireInput();
        }

        ships[shipIdx]->update (dt, moveInput, aimInput, fireInput, arenaWidth, arenaHeight, wind);

        // Set crosshair directly for mouse aiming
        if (isHumanControlled && players[playerIdx]->isUsingMouse())
        {
            ships[shipIdx]->setCrosshairPosition (players[playerIdx]->getMousePosition());
        }

        // Collect pending shells from ship
        auto& pendingShells = ships[shipIdx]->getPendingShells();
        if (! pendingShells.empty() && audio)
        {
            // Play cannon sound at ship position
            audio->playCannon (ships[shipIdx]->getPosition().x, arenaWidth);
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
        for (int i = 0; i < numShips; ++i)
        {
            if (ships[i] && ships[i]->isAlive())
            {
                totalThrottle += std::abs (ships[i]->getThrottle());
                aliveCount++;
            }
        }
        float avgThrottle = aliveCount > 0 ? totalThrottle / aliveCount : 0.0f;
        audio->setEngineVolume (Config::audioEngineBaseVolume + avgThrottle * Config::audioEngineThrottleBoost);
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
    int numShips = getNumShipsForMode();
    for (int i = 0; i < numShips; ++i)
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
    if (gameOverTimer >= Config::gameOverReturnDelay)
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
    // Calculate wind drift force based on shell speed and max wind drift
    float shellSpeed = Config::shipMaxSpeed * Config::shellSpeedMultiplier;
    Vec2 windDrift = wind * shellSpeed * Config::windMaxDrift;

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
            if (! ship || ! ship->isAlive())
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

                ship->takeDamage (Config::shellDamage);

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
                    sinkExplosion.maxRadius = Config::sinkExplosionMaxRadius;
                    sinkExplosion.duration = Config::sinkExplosionDuration;
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
    int numShips = getNumShipsForMode();
    for (int i = 0; i < numShips; ++i)
    {
        if (! ships[i] || ! ships[i]->isAlive())
            continue;

        for (int j = i + 1; j < numShips; ++j)
        {
            if (! ships[j] || ! ships[j]->isAlive())
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
                if (audio && impactSpeed > Config::aiMinImpactForSound)
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

    if (gameMode == GameMode::Teams || gameMode == GameMode::Battle)
    {
        // Count fighting ships per team
        int team0Alive = 0;
        int team1Alive = 0;
        int numShips = getNumShipsForMode();

        for (int i = 0; i < numShips; ++i)
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
        // FFA and Duel modes - last ship standing wins
        int aliveCount = 0;
        int lastAlive = -1;
        int numShips = getNumShipsForMode();

        for (int i = 0; i < numShips; ++i)
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
    renderer->drawTextCentered ("HELIGOLAND", { w / 2.0f, h / 3.0f }, 8.0f, Config::colorTitle);

    // Draw connected players
    int connectedCount = 0;
    for (auto& player : players)
    {
        if (player->isConnected())
        {
            connectedCount++;
        }
    }

    std::string playerText = std::to_string (connectedCount) + " PLAYERS CONNECTED";
    renderer->drawTextCentered (playerText, { w / 2.0f, h * 0.45f }, 3.0f, Config::colorSubtitle);

    // Draw game mode selector
    std::string modeText;
    if (gameMode == GameMode::FFA)
        modeText = "FREE FOR ALL";
    else if (gameMode == GameMode::Teams)
        modeText = "2 VS 2";
    else if (gameMode == GameMode::Duel)
        modeText = "1 VS 1";
    else
        modeText = "BATTLE 6 VS 6";
    renderer->drawTextCentered (modeText, { w / 2.0f, h * 0.55f }, 4.0f, Config::colorModeText);

    renderer->drawTextCentered ("LEFT - RIGHT TO CHANGE MODE", { w / 2.0f, h * 0.62f }, 1.5f, Config::colorGreySubtle);

    // Draw player slots (only show human-controllable slots)
    int numSlots = (gameMode == GameMode::Battle) ? MAX_PLAYERS : getNumShipsForMode();
    float slotY = h * 0.72f;
    float slotSpacing = 80.0f;

    if (gameMode == GameMode::Teams || gameMode == GameMode::Battle)
    {
        // Battle mode: show 2 slots per team with team labels
        float teamSpacing = 150.0f;
        float startX = w / 2.0f - teamSpacing / 2.0f - slotSpacing / 2.0f;

        // Team 1 slots (players 0, 1)
        renderer->drawTextCentered ("TEAM 1", { startX + slotSpacing / 2.0f, slotY - 45.0f }, 2.0f, Config::colorShipRed);
        for (int i = 0; i < 2; ++i)
        {
            Vec2 slotPos = { startX + i * slotSpacing, slotY };
            if (players[i]->isConnected())
            {
                Color slotColor = (i == 0) ? Config::colorShipRed : Config::colorShipBlue;
                renderer->drawFilledRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
                renderer->drawTextCentered ("P" + std::to_string (i + 1), slotPos, 3.0f, Config::colorBlack);
            }
            else
            {
                renderer->drawRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, Config::colorGreyDark);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, Config::colorGreyDark);
            }
        }

        // Team 2 slots (players 2, 3)
        float team2StartX = w / 2.0f + teamSpacing / 2.0f - slotSpacing / 2.0f;
        renderer->drawTextCentered ("TEAM 2", { team2StartX + slotSpacing / 2.0f, slotY - 45.0f }, 2.0f, Config::colorShipBlue);
        for (int i = 2; i < 4; ++i)
        {
            Vec2 slotPos = { team2StartX + (i - 2) * slotSpacing, slotY };
            if (players[i]->isConnected())
            {
                Color slotColor = (i == 2) ? Config::colorShipGreen : Config::colorShipYellow;
                renderer->drawFilledRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
                renderer->drawTextCentered ("P" + std::to_string (i + 1), slotPos, 3.0f, Config::colorBlack);
            }
            else
            {
                renderer->drawRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, Config::colorGreyDark);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, Config::colorGreyDark);
            }
        }

        if (gameMode == GameMode::Battle)
        {
            // Show "+4 AI" indicators for each team
            renderer->drawTextCentered ("+4 AI", { startX + slotSpacing / 2.0f, slotY + 45.0f }, 1.5f, Config::colorGreySubtle);
            renderer->drawTextCentered ("+4 AI", { team2StartX + slotSpacing / 2.0f, slotY + 45.0f }, 1.5f, Config::colorGreySubtle);
        }
    }
    else
    {
        float startX = w / 2.0f - (numSlots - 1) * slotSpacing / 2.0f;

        for (int i = 0; i < numSlots; ++i)
        {
            Vec2 slotPos = { startX + i * slotSpacing, slotY };
            Color slotColor;

            if (players[i]->isConnected())
            {
                // Use player color
                switch (i)
                {
                    case 0:
                        slotColor = Config::colorShipRed;
                        break;
                    case 1:
                        slotColor = Config::colorShipBlue;
                        break;
                    case 2:
                        slotColor = Config::colorShipGreen;
                        break;
                    case 3:
                        slotColor = Config::colorShipYellow;
                        break;
                    default:
                        slotColor = Config::colorGrey;
                        break;
                }
                renderer->drawFilledRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
                renderer->drawTextCentered ("P" + std::to_string (i + 1), slotPos, 3.0f, Config::colorBlack);
            }
            else
            {
                slotColor = Config::colorGreyDark;
                renderer->drawRect ({ slotPos.x - 25, slotPos.y - 25 }, 50, 50, slotColor);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, slotColor);
            }
        }
    }

    // Draw start instruction
    renderer->drawTextCentered ("CLICK OR PRESS ANY BUTTON TO START", { w / 2.0f, h * 0.90f }, 2.0f, Config::colorInstruction);
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

    // Draw HUD for ships (in Battle mode, only show human-controlled ships)
    int numShips = getNumShipsForMode();
    std::vector<int> hudShipIndices;

    if (gameMode == GameMode::Battle)
    {
        // Only show HUDs for potential human ships: 0, 1, 6, 7
        hudShipIndices = { 0, 1, 6, 7 };
    }
    else
    {
        for (int i = 0; i < numShips; ++i)
            hudShipIndices.push_back (i);
    }

    int numHuds = (int) hudShipIndices.size();
    float hudWidth = 200.0f;
    float hudHeight = 50.0f;
    float hudSpacing = 20.0f;
    float hudTotalWidth = numHuds * hudWidth + (numHuds - 1) * hudSpacing;
    float hudStartX = (w - hudTotalWidth) / 2.0f;
    float hudY = 10.0f;

    int slot = 0;
    for (int shipIdx : hudShipIndices)
    {
        const auto& ship = ships[shipIdx];
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

            renderer->drawShipHUD (*ship, slot, numHuds, w, alpha);
        }
        slot++;
    }

    // Draw wind indicator
    renderer->drawWindIndicator (wind, w, h);

    // Draw team ship counters for Battle mode
    if (gameMode == GameMode::Battle)
    {
        int team1Alive = 0;
        int team2Alive = 0;
        for (int i = 0; i < numShips; ++i)
        {
            if (ships[i] && ships[i]->isAlive() && ! ships[i]->isSinking())
            {
                if (getTeam (i) == 0)
                    team1Alive++;
                else
                    team2Alive++;
            }
        }

        std::string team1Text = std::to_string (team1Alive);
        std::string team2Text = std::to_string (team2Alive);

        // Draw on left and right sides of screen
        renderer->drawTextCentered (team1Text, { 50.0f, h / 2.0f }, 6.0f, Config::colorTeam1Dark);
        renderer->drawTextCentered (team2Text, { w - 50.0f, h / 2.0f }, 6.0f, Config::colorTeam2Dark);
    }
}

void Game::renderGameOver()
{
    // Wait before showing text so player can see the final explosion
    if (gameOverTimer < Config::gameOverTextDelay)
    {
        return;
    }

    float w, h;
    getWindowSize (w, h);

    Color textColor = Config::colorWhite;
    Color statsColor = Config::colorSubtitle;

    if (winnerIndex >= 0)
    {
        std::string winText;
        if (gameMode == GameMode::Teams || gameMode == GameMode::Battle)
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
    if (gameMode == GameMode::Teams || gameMode == GameMode::Battle)
    {
        std::string statsText = "TEAM 1: " + std::to_string (teamWins[0]) +
                                "  -  TEAM 2: " + std::to_string (teamWins[1]);
        renderer->drawTextCentered (statsText, { w / 2.0f, h / 2.0f + 40.0f }, 2.5f, statsColor);
    }
    else if (gameMode == GameMode::Duel)
    {
        std::string statsText = "P1: " + std::to_string (playerWins[0]) +
                                "  -  P2: " + std::to_string (playerWins[1]);
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

    if (gameMode == GameMode::Duel)
    {
        // 1v1 mode: ships on left and right, centered vertically
        float margin = w * 0.15f;
        if (index == 0)
            return { margin, h / 2.0f };
        else
            return { w - margin, h / 2.0f };
    }

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

    if (gameMode == GameMode::Battle)
    {
        // 6v6 mode: 6 ships per team in a single column on each side
        float margin = w * 0.12f;
        int shipsPerTeam = 6;

        int team = getTeam (index);
        int row = (team == 0) ? index : index - shipsPerTeam;

        float verticalSpacing = h / (shipsPerTeam + 1);
        float y = verticalSpacing + row * verticalSpacing;

        if (team == 0)
            return { margin, y };
        else
            return { w - margin, y };
    }

    // FFA mode: Place ships in a circle around the center
    Vec2 center = { w / 2.0f, h / 2.0f };
    float radius = std::min (w, h) * 0.35f;

    float angleOffset = -pi / 2.0f;
    int numShips = getNumShipsForMode();
    float angle = angleOffset + (index * 2.0f * pi / numShips);

    return center + Vec2::fromAngle (angle) * radius;
}

float Game::getShipStartAngle (int index) const
{
    if (gameMode == GameMode::Duel)
    {
        // 1v1 mode: ships face each other
        if (index == 0)
            return 0.0f;  // Player 1 faces right
        else
            return pi;    // Player 2 faces left
    }

    if (gameMode == GameMode::Teams)
    {
        // 2v2 mode: teams face each other
        if (index < 2)
            return 0.0f;  // Team 0 faces right
        else
            return pi;    // Team 1 faces left
    }

    if (gameMode == GameMode::Battle)
    {
        // 6v6 mode: teams face each other
        if (getTeam (index) == 0)
            return 0.0f;  // Team 0 faces right
        else
            return pi;    // Team 1 faces left
    }

    // FFA mode: Point ships 90 degrees from center (tangent to circle)
    float angleOffset = -pi / 2.0f;
    int numShips = getNumShipsForMode();
    float posAngle = angleOffset + (index * 2.0f * pi / numShips);
    return posAngle + pi + (pi / 2.0f);
}

void Game::getWindowSize (float& width, float& height) const
{
    width = (float) GetScreenWidth();
    height = (float) GetScreenHeight();
}

int Game::getTeam (int shipIndex) const
{
    if (gameMode == GameMode::Battle)
    {
        // Battle mode: ships 0-5 on team 0, ships 6-11 on team 1
        return shipIndex < 6 ? 0 : 1;
    }
    // Teams/other modes: ships 0, 1 on team 0, ships 2, 3 on team 1
    return shipIndex < 2 ? 0 : 1;
}

bool Game::areEnemies (int playerA, int playerB) const
{
    if (gameMode == GameMode::FFA || gameMode == GameMode::Duel)
        return playerA != playerB;  // Everyone is an enemy in FFA and Duel

    return getTeam (playerA) != getTeam (playerB);
}

void Game::cycleGameMode (int direction)
{
    int mode = static_cast<int> (gameMode);
    mode += direction;

    // Wrap around (4 modes: FFA, Teams, Duel, Battle)
    if (mode < 0)
        mode = 3;
    else if (mode > 3)
        mode = 0;

    gameMode = static_cast<GameMode> (mode);
}

int Game::getNumShipsForMode() const
{
    if (gameMode == GameMode::Duel)
        return 2;
    if (gameMode == GameMode::Battle)
        return 12;
    return 4;  // FFA and Teams
}

int Game::getShipIndexForPlayer (int playerIndex) const
{
    if (gameMode == GameMode::Battle)
    {
        // In Battle mode:
        // Player 0 -> Ship 0 (team 1)
        // Player 1 -> Ship 1 (team 1)
        // Player 2 -> Ship 6 (team 2)
        // Player 3 -> Ship 7 (team 2)
        if (playerIndex < 2)
            return playerIndex;
        else
            return playerIndex + 4;  // 2->6, 3->7
    }
    // Other modes: direct mapping
    return playerIndex;
}

int Game::getPlayerIndexForShip (int shipIndex) const
{
    if (gameMode == GameMode::Battle)
    {
        // In Battle mode, only ships 0, 1, 6, 7 can be human-controlled
        if (shipIndex == 0)
            return 0;
        if (shipIndex == 1)
            return 1;
        if (shipIndex == 6)
            return 2;
        if (shipIndex == 7)
            return 3;
        return -1;  // AI controlled
    }
    // Other modes: direct mapping (if within player count)
    if (shipIndex < MAX_PLAYERS)
        return shipIndex;
    return -1;
}
