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
    SetExitKey (0);  // Disable raylib's default ESC-to-close behavior
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
        if (state == GameState::Title)
            running = false;
        else
            returnToTitle();
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

    // Ship selection with D-pad or face buttons for each connected player
    static std::array<bool, MAX_PLAYERS> upWasPressed = {};
    static std::array<bool, MAX_PLAYERS> downWasPressed = {};

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (!players[i]->isConnected())
            continue;

        bool upPressed = false;
        bool downPressed = false;

        if (IsGamepadAvailable (i))
        {
            // Use D-pad for ship selection
            upPressed = IsGamepadButtonDown (i, GAMEPAD_BUTTON_LEFT_FACE_UP);
            downPressed = IsGamepadButtonDown (i, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        }

        // Player 0 can also use keyboard
        if (i == 0)
        {
            if (IsKeyDown (KEY_W))
                upPressed = true;
            if (IsKeyDown (KEY_S))
                downPressed = true;
        }

        // Cycle ship selection with edge detection
        if (upPressed && !upWasPressed[i])
        {
            playerShipSelection[i] = (playerShipSelection[i] + 1) % NUM_SHIP_TYPES;
        }
        if (downPressed && !downWasPressed[i])
        {
            playerShipSelection[i] = (playerShipSelection[i] + NUM_SHIP_TYPES - 1) % NUM_SHIP_TYPES;
        }

        upWasPressed[i] = upPressed;
        downWasPressed[i] = downPressed;
    }

    // Volume control with triggers (up/down keys or gamepad triggers)
    if (audio)
    {
        static bool volumeDownWasPressed = false;
        static bool volumeUpWasPressed = false;

        bool volumeDownPressed = IsKeyDown (KEY_DOWN);
        bool volumeUpPressed = IsKeyDown (KEY_UP);

        // Check all gamepads for trigger presses
        for (int i = 0; i < 4; ++i)
        {
            if (IsGamepadAvailable (i))
            {
                float leftTrigger = GetGamepadAxisMovement (i, GAMEPAD_AXIS_LEFT_TRIGGER);
                float rightTrigger = GetGamepadAxisMovement (i, GAMEPAD_AXIS_RIGHT_TRIGGER);

                if (leftTrigger > 0.5f)
                    volumeDownPressed = true;
                if (rightTrigger > 0.5f)
                    volumeUpPressed = true;
            }
        }

        // Adjust volume on press (with edge detection)
        if (volumeDownWasPressed && ! volumeDownWasPressed)
            audio->setMasterVolume (audio->getMasterVolumeLevel() - 1);
        if (volumeUpPressed && ! volumeUpWasPressed)
            audio->setMasterVolume (audio->getMasterVolumeLevel() + 1);

        volumeDownWasPressed = volumeDownPressed;
        volumeUpWasPressed = volumeUpPressed;
    }

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

        // Determine ship type: use player selection for humans, random for AI
        int playerIdx = getPlayerIndexForShip (i);
        int shipType;
        if (playerIdx >= 0 && players[playerIdx]->isConnected())
        {
            // Human player - use their selection
            shipType = playerShipSelection[playerIdx];
        }
        else
        {
            // AI - random ship type
            shipType = rand() % NUM_SHIP_TYPES;
        }

        float shipLength = renderer->getShipLength (shipType);
        float shipWidth = renderer->getShipWidth (shipType);

        ships[i] = std::make_unique<Ship> (i, getShipStartPosition (i), getShipStartAngle (i), shipLength, shipWidth, team, shipType);
    }

    shells.clear();
    explosions.clear();
    winnerIndex = -1;
    gameOverTimer = 0.0f;
    gameStartDelay = config.gameStartDelay;

    // Initialize wind (minimum strength)
    float windAngle = ((float) rand() / RAND_MAX) * 2.0f * pi;
    float windStrength = config.windMinStrength + ((float) rand() / RAND_MAX) * (1.0f - config.windMinStrength);
    wind = Vec2::fromAngle (windAngle) * windStrength;
    targetWind = wind;
    windChangeTimer = config.windChangeInterval;

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
        float angleChange = ((float) rand() / RAND_MAX - 0.5f) * config.windAngleChangeMax * 2.0f;
        float newAngle = currentAngle + angleChange;

        // Small strength change, minimum strength enforced
        float currentStrength = wind.length();
        float strengthChange = ((float) rand() / RAND_MAX - 0.5f) * config.windStrengthChangeMax;
        float newStrength = std::clamp (currentStrength + strengthChange, config.windMinStrength, 1.0f);

        targetWind = Vec2::fromAngle (newAngle) * newStrength;
        windChangeTimer = config.windChangeInterval;
    }

    // Slowly lerp wind toward target
    wind.x += (targetWind.x - wind.x) * config.windLerpSpeed * dt;
    wind.y += (targetWind.y - wind.y) * config.windLerpSpeed * dt;
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
        if (! ships[shipIdx] || ! ships[shipIdx]->isVisible())
            continue; // Skip dead ships

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
                if (areEnemies (shipIdx, j) && ships[j] && ships[j]->isAlive())
                    enemies.push_back (ships[j].get());

            aiControllers[shipIdx]->update (dt, *ships[shipIdx], enemies, shells, arenaWidth, arenaHeight);
            moveInput = aiControllers[shipIdx]->getMoveInput();
            aimInput = aiControllers[shipIdx]->getAimInput();
            fireInput = aiControllers[shipIdx]->getFireInput();
        }

        ships[shipIdx]->update (dt, moveInput, aimInput, fireInput, arenaWidth, arenaHeight, wind);

        // Set crosshair directly for mouse aiming
        if (isHumanControlled && players[playerIdx]->isUsingMouse())
            ships[shipIdx]->setCrosshairPosition (players[playerIdx]->getMousePosition());

        // Collect pending shells from ship
        auto& pendingShells = ships[shipIdx]->getPendingShells();
        if (! pendingShells.empty() && audio)
        {
            // Play cannon sound at ship position
            audio->playCannon (ships[shipIdx]->getPosition().x, arenaWidth);
        }

        for (auto& shell : pendingShells)
            shells.push_back (std::move (shell));

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
        audio->setEngineVolume (config.audioEngineBaseVolume + avgThrottle * config.audioEngineThrottleBoost);
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
        if (ships[i] && ships[i]->isVisible())
            ships[i]->update (dt, { 0, 0 }, { 0, 0 }, false, arenaWidth, arenaHeight, wind);

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
            splash.duration = config.explosionDuration;
            splash.maxRadius = config.explosionMaxRadius;
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
    if (gameOverTimer >= config.gameOverReturnDelay)
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
    float shellSpeed = config.shipMaxSpeed * config.shellSpeedMultiplier;
    Vec2 windDrift = wind * shellSpeed * config.windMaxDrift;

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
            if (! ship || ! ship->isVisible())
                continue;
            if (ship->getPlayerIndex() == shell.getOwnerIndex())
                continue; // Don't hit own ship

            // First do a quick bounding check, then pixel-perfect if within bounds
            Vec2 diff = shell.getPosition() - ship->getPosition();
            float dist = diff.length();
            float boundingRadius = ship->getLength() / 2.0f + shell.getSplashRadius();

            if (dist < boundingRadius && renderer->checkShipHit (*ship, shell.getPosition()))
            {
                float arenaWidth, arenaHeight;
                getWindowSize (arenaWidth, arenaHeight);

                ship->takeDamage (shell.getDamage());

                // Spawn hit explosion
                Explosion explosion;
                explosion.position = shell.getPosition();
                explosion.isHit = true;
                explosion.duration = config.explosionDuration;
                explosion.maxRadius = config.explosionMaxRadius;
                explosions.push_back (explosion);

                // Play explosion sound
                if (audio)
                    audio->playExplosion (shell.getPosition().x, arenaWidth);

                // Check if ship was sunk
                if (! ship->isAlive())
                {
                    // Big explosion for sinking
                    Explosion sinkExplosion;
                    sinkExplosion.position = ship->getPosition();
                    sinkExplosion.isHit = true;
                    sinkExplosion.maxRadius = config.sinkExplosionMaxRadius;
                    sinkExplosion.duration = config.sinkExplosionDuration;
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
            splash.duration = config.explosionDuration;
            splash.maxRadius = config.explosionMaxRadius;
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
        if (! ships[i] || ! ships[i]->isVisible())
            continue;

        for (int j = i + 1; j < numShips; ++j)
        {
            if (! ships[j] || ! ships[j]->isVisible())
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
                // OBB overlap detected - now do pixel-perfect check
                Vec2 collisionPoint;
                if (!renderer->checkShipCollision (*ships[i], *ships[j], collisionPoint))
                    continue; // No actual pixel overlap

                // Collision detected!
                Vec2 velA = ships[i]->getVelocity();
                Vec2 velB = ships[j]->getVelocity();

                // Calculate relative speed for damage
                Vec2 relVel = velA - velB;
                float impactSpeed = relVel.length();

                // Damage proportional to impact speed
                float damage = impactSpeed * config.collisionDamageScale;
                ships[i]->takeDamage (damage);
                ships[j]->takeDamage (damage);

                // Play collision sound at collision point
                if (audio && impactSpeed > config.audioMinImpactForSound)
                {
                    float arenaWidth, arenaHeight;
                    getWindowSize (arenaWidth, arenaHeight);
                    audio->playCollision (collisionPoint.x, arenaWidth);
                }

                // Determine collision normal (from i to j)
                Vec2 diff = ships[j]->getPosition() - ships[i]->getPosition();
                if (diff.dot (minAxis) < 0)
                    minAxis = minAxis * -1.0f;

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
    renderer->drawTextCentered ("HELIGOLAND", { w / 2.0f, h * 0.15f }, 8.0f, config.colorTitle);

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
    renderer->drawTextCentered (playerText, { w / 2.0f, h * 0.27f }, 3.0f, config.colorSubtitle);

    // Draw game mode selector
    std::string modeText;
    if (gameMode == GameMode::FFA)
        modeText = "FREE FOR ALL";
    else if (gameMode == GameMode::Teams)
        modeText = "2 VS 2";
    else if (gameMode == GameMode::Duel)
        modeText = "1 VS 1";
    else if (gameMode == GameMode::Triple)
        modeText = "1 VS 1 VS 1";
    else
        modeText = "BATTLE 6 VS 6";
    renderer->drawTextCentered (modeText, { w / 2.0f, h * 0.35f }, 4.0f, config.colorModeText);

    renderer->drawTextCentered ("LEFT - RIGHT TO CHANGE MODE", { w / 2.0f, h * 0.41f }, 1.5f, config.colorGreySubtle);

    // Draw ship selection section
    renderer->drawTextCentered ("SELECT YOUR SHIP", { w / 2.0f, h * 0.48f }, 2.5f, config.colorSubtitle);

    // Draw player slots with ship previews
    int numSlots = (gameMode == GameMode::Battle) ? MAX_PLAYERS : getNumShipsForMode();
    float slotY = h * 0.62f;
    float slotSpacing = 140.0f;

    // Helper to get player color
    auto getPlayerColor = [this] (int playerIndex) -> Color
    {
        switch (playerIndex)
        {
            case 0: return config.colorShipRed;
            case 1: return config.colorShipBlue;
            case 2: return config.colorShipGreen;
            case 3: return config.colorShipYellow;
            default: return config.colorGrey;
        }
    };

    if (gameMode == GameMode::Teams || gameMode == GameMode::Battle)
    {
        // Battle mode: show 2 slots per team with team labels
        float teamSpacing = 200.0f;
        float startX = w / 2.0f - teamSpacing / 2.0f - slotSpacing / 2.0f;

        // Team 1 slots (players 0, 1)
        renderer->drawTextCentered ("TEAM 1", { startX + slotSpacing / 2.0f, slotY - 70.0f }, 2.0f, config.colorTeam1);
        for (int i = 0; i < 2; ++i)
        {
            Vec2 slotPos = { startX + i * slotSpacing, slotY };
            Color slotColor = getPlayerColor (i);

            if (players[i]->isConnected())
            {
                // Draw ship preview (45 degrees) with player color on turrets
                renderer->drawShipPreview (playerShipSelection[i], slotPos, -pi / 4.0f, i);

                // Draw player label below
                renderer->drawTextCentered ("P" + std::to_string (i + 1), { slotPos.x, slotPos.y + 50.0f }, 2.0f, slotColor);

                // Draw ship type name
                std::string shipName = config.shipTypes[playerShipSelection[i]].name;
                renderer->drawTextCentered (shipName, { slotPos.x, slotPos.y + 70.0f }, 1.5f, config.colorGreyLight);
            }
            else
            {
                renderer->drawRect ({ slotPos.x - 30, slotPos.y - 40 }, 60, 80, config.colorGreyDark);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, config.colorGreyDark);
            }
        }

        // Team 2 slots (players 2, 3)
        float team2StartX = w / 2.0f + teamSpacing / 2.0f - slotSpacing / 2.0f;
        renderer->drawTextCentered ("TEAM 2", { team2StartX + slotSpacing / 2.0f, slotY - 70.0f }, 2.0f, config.colorTeam2);
        for (int i = 2; i < 4; ++i)
        {
            Vec2 slotPos = { team2StartX + (i - 2) * slotSpacing, slotY };
            Color slotColor = getPlayerColor (i);

            if (players[i]->isConnected())
            {
                // Draw ship preview (45 degrees) with player color on turrets
                renderer->drawShipPreview (playerShipSelection[i], slotPos, -pi / 4.0f, i);

                // Draw player label below
                renderer->drawTextCentered ("P" + std::to_string (i + 1), { slotPos.x, slotPos.y + 50.0f }, 2.0f, slotColor);

                // Draw ship type name
                std::string shipName = config.shipTypes[playerShipSelection[i]].name;
                renderer->drawTextCentered (shipName, { slotPos.x, slotPos.y + 70.0f }, 1.5f, config.colorGreyLight);
            }
            else
            {
                renderer->drawRect ({ slotPos.x - 30, slotPos.y - 40 }, 60, 80, config.colorGreyDark);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, config.colorGreyDark);
            }
        }

        if (gameMode == GameMode::Battle)
        {
            // Show "+4 AI" indicators for each team
            renderer->drawTextCentered ("+4 AI", { startX + slotSpacing / 2.0f, slotY + 90.0f }, 1.5f, config.colorGreySubtle);
            renderer->drawTextCentered ("+4 AI", { team2StartX + slotSpacing / 2.0f, slotY + 90.0f }, 1.5f, config.colorGreySubtle);
        }
    }
    else
    {
        float startX = w / 2.0f - (numSlots - 1) * slotSpacing / 2.0f;

        for (int i = 0; i < numSlots; ++i)
        {
            Vec2 slotPos = { startX + i * slotSpacing, slotY };
            Color slotColor = getPlayerColor (i);

            if (players[i]->isConnected())
            {
                // Draw ship preview (45 degrees) with player color on turrets
                renderer->drawShipPreview (playerShipSelection[i], slotPos, -pi / 4.0f, i);

                // Draw player label below
                renderer->drawTextCentered ("P" + std::to_string (i + 1), { slotPos.x, slotPos.y + 50.0f }, 2.0f, slotColor);

                // Draw ship type name
                std::string shipName = config.shipTypes[playerShipSelection[i]].name;
                renderer->drawTextCentered (shipName, { slotPos.x, slotPos.y + 70.0f }, 1.5f, config.colorGreyLight);
            }
            else
            {
                renderer->drawRect ({ slotPos.x - 30, slotPos.y - 40 }, 60, 80, config.colorGreyDark);
                renderer->drawTextCentered ("AI", slotPos, 2.0f, config.colorGreyDark);
            }
        }
    }

    // Draw ship selection hint for connected players
    bool anyConnected = false;
    for (int i = 0; i < MAX_PLAYERS; ++i)
        if (players[i]->isConnected())
            anyConnected = true;

    if (anyConnected)
        renderer->drawTextCentered ("D-PAD UP - DOWN TO SELECT SHIP", { w / 2.0f, h * 0.82f }, 1.5f, config.colorGreySubtle);

    // Draw volume control
    if (audio)
    {
        std::string volumeText = "VOLUME: " + std::to_string (audio->getMasterVolumeLevel());
        renderer->drawTextCentered (volumeText, { w / 2.0f, h * 0.88f }, 2.0f, config.colorSubtitle);
    }

    // Draw start instruction
    renderer->drawTextCentered ("CLICK OR PRESS ANY BUTTON TO START", { w / 2.0f, h * 0.95f }, 2.0f, config.colorInstruction);
}

void Game::renderPlaying()
{
    float w, h;
    getWindowSize (w, h);

    // Draw bubble trails first (behind everything)
    for (const auto& ship : ships)
        if (ship && ship->isVisible())
            renderer->drawBubbleTrail (*ship);

    // Draw ships
    for (const auto& ship : ships)
        if (ship && ship->isVisible())
            renderer->drawShip (*ship);

    // Draw smoke (above ships)
    for (const auto& ship : ships)
        if (ship && ship->isVisible())
            renderer->drawSmoke (*ship);

    // Draw shells (on top of ships)
    for (const auto& shell : shells)
        renderer->drawShell (shell);

    // Draw explosions
    for (const auto& explosion : explosions)
        renderer->drawExplosion (explosion);

    // Draw crosshairs (on top of everything)
    for (const auto& ship : ships)
        if (ship && ship->isAlive())
            renderer->drawCrosshair (*ship);

    // Draw HUD for all ships
    int numShips = getNumShipsForMode();

    // Calculate HUD width based on available space (reserve 80px for wind indicator on right)
    float availableWidth = w - 80.0f - 20.0f; // Right margin for wind, left margin
    float hudSpacing = 10.0f;
    float maxHudWidth = 200.0f;
    float minHudWidth = 80.0f;
    float hudWidth = std::min (maxHudWidth, (availableWidth - (numShips - 1) * hudSpacing) / numShips);
    hudWidth = std::max (minHudWidth, hudWidth);

    float hudHeight = 50.0f;
    float hudTotalWidth = numShips * hudWidth + (numShips - 1) * hudSpacing;
    float hudStartX = (w - hudTotalWidth) / 2.0f; // Center HUDs (must match Renderer::drawShipHUD)
    float hudY = 10.0f;

    for (int i = 0; i < numShips; ++i)
    {
        const auto& ship = ships[i];
        if (ship)
        {
            // Check if any ship is under this HUD panel
            float hudX = hudStartX + i * (hudWidth + hudSpacing);
            float alpha = 1.0f;

            // Fade HUD if ship underneath
            for (const auto& otherShip : ships)
            {
                if (otherShip && otherShip->isAlive())
                {
                    Vec2 pos = otherShip->getPosition();
                    float margin = otherShip->getLength() / 2.0f;
                    if (pos.x > hudX - margin && pos.x < hudX + hudWidth + margin &&
                        pos.y > hudY - margin && pos.y < hudY + hudHeight + margin)
                    {
                        alpha = 0.25f;
                        break;
                    }
                }
            }

            // Dim HUD for dead/sinking ships
            if (!ship->isAlive() || ship->isSinking())
                alpha *= 0.4f;

            renderer->drawShipHUD (*ship, i, numShips, w, hudWidth, alpha);
        }
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
        renderer->drawTextCentered (team1Text, { 50.0f, h / 2.0f }, 6.0f, config.colorTeam1);
        renderer->drawTextCentered (team2Text, { w - 50.0f, h / 2.0f }, 6.0f, config.colorTeam2);
    }
}

void Game::renderGameOver()
{
    // Wait before showing text so player can see the final explosion
    if (gameOverTimer < config.gameOverTextDelay)
        return;

    float w, h;
    getWindowSize (w, h);

    Color textColor = config.colorWhite;
    Color statsColor = config.colorSubtitle;

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
    else if (gameMode == GameMode::Triple)
    {
        std::string statsText = "P1: " + std::to_string (playerWins[0]) +
                                "  P2: " + std::to_string (playerWins[1]) +
                                "  P3: " + std::to_string (playerWins[2]);
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

    // Wrap around (5 modes: FFA, Teams, Duel, Triple, Battle)
    if (mode < 0)
        mode = 4;
    else if (mode > 4)
        mode = 0;

    gameMode = static_cast<GameMode> (mode);
}

int Game::getNumShipsForMode() const
{
    if (gameMode == GameMode::Duel)
        return 2;
    if (gameMode == GameMode::Triple)
        return 3;
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
