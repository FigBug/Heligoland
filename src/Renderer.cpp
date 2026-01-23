#include "Renderer.h"
#include "Config.h"
#include "Game.h"
#include "Island.h"
#include "Shell.h"
#include "Ship.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <string>

static std::string getResourcePath (const char* filename)
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle)
    {
        CFURLRef resourceURL = CFBundleCopyResourcesDirectoryURL (mainBundle);
        if (resourceURL)
        {
            char path[PATH_MAX];
            if (CFURLGetFileSystemRepresentation (resourceURL, true, (UInt8*) path, PATH_MAX))
            {
                CFRelease (resourceURL);
                return std::string (path) + "/" + filename;
            }
            CFRelease (resourceURL);
        }
    }
    return filename;
}
#elif defined(__linux__)
#include <string>
#include <unistd.h>
#include <linux/limits.h>

static std::string getResourcePath (const char* filename)
{
    if (access (filename, F_OK) == 0)
        return filename;

    char exePath[PATH_MAX];
    ssize_t len = readlink ("/proc/self/exe", exePath, sizeof (exePath) - 1);
    if (len != -1)
    {
        exePath[len] = '\0';
        std::string dir (exePath);
        size_t lastSlash = dir.find_last_of ('/');
        if (lastSlash != std::string::npos)
        {
            dir = dir.substr (0, lastSlash + 1);
            return dir + filename;
        }
    }
    return filename;
}
#else
#include <string>

static std::string getResourcePath (const char* filename)
{
    return filename;
}
#endif

Renderer::Renderer()
{
    createNoiseTexture();
    loadShipTextures();
}

Renderer::~Renderer()
{
    if (noiseTexture1.id != 0)
        UnloadTexture (noiseTexture1);
    if (noiseTexture2.id != 0)
        UnloadTexture (noiseTexture2);

    // Unload ship textures and images
    for (int i = 0; i < NUM_SHIP_TYPES; ++i)
    {
        if (shipHullTextures[i].id != 0)
            UnloadTexture (shipHullTextures[i]);
        if (shipTurretTextures[i].id != 0)
            UnloadTexture (shipTurretTextures[i]);
        if (shipHullImages[i].data != nullptr)
            UnloadImage (shipHullImages[i]);
    }
}

void Renderer::clear()
{
    ClearBackground (config.colorOcean);
}

void Renderer::drawWater (float time, float screenWidth, float screenHeight)
{
    // Base ocean color
    ClearBackground (config.colorOcean);

    // Texture is displayed at 1.5x scale for larger coverage
    float tileSize = noiseTextureSize * 1.5f;

    // Scrolling offsets for two layers - different speeds and directions
    float scroll1X = std::fmod (time * 2.0f, tileSize);
    float scroll1Y = std::fmod (time * 1.25f, tileSize);
    float scroll2X = std::fmod (time * -1.5f, tileSize);
    float scroll2Y = std::fmod (time * 2.25f, tileSize);

    // Calculate tiles needed (add extra for scrolling)
    int tilesX = (int) (screenWidth / tileSize) + 2;
    int tilesY = (int) (screenHeight / tileSize) + 2;

    // Layer 1 - first noise texture
    for (int ty = -1; ty < tilesY; ++ty)
    {
        for (int tx = -1; tx < tilesX; ++tx)
        {
            float tileX = tx * tileSize - scroll1X;
            float tileY = ty * tileSize - scroll1Y;

            Rectangle source = { 0, 0, (float) noiseTextureSize, (float) noiseTextureSize };
            Rectangle dest = { tileX, tileY, tileSize, tileSize };
            DrawTexturePro (noiseTexture1, source, dest, { 0, 0 }, 0.0f, WHITE);
        }
    }

    // Layer 2 - second noise texture with offset and different pattern
    float layerOffset = tileSize * 0.37f; // Non-aligned offset
    for (int ty = -1; ty < tilesY; ++ty)
    {
        for (int tx = -1; tx < tilesX; ++tx)
        {
            float tileX = tx * tileSize - scroll2X + layerOffset;
            float tileY = ty * tileSize - scroll2Y + layerOffset;

            Rectangle source = { 0, 0, (float) noiseTextureSize, (float) noiseTextureSize };
            Rectangle dest = { tileX, tileY, tileSize, tileSize };
            DrawTexturePro (noiseTexture2, source, dest, { 0, 0 }, 0.0f, WHITE);
        }
    }
}

void Renderer::present()
{
    // raylib handles this in EndDrawing() - nothing to do here
}

void Renderer::drawShip (const Ship& ship)
{
    Vec2 pos = ship.getPosition();
    float angle = ship.getAngle();

    // Draw firing range circle (very faint white) - only for non-sinking ships
    if (ship.isAlive())
        drawFilledCircle (pos, ship.getMaxRange(), config.colorFiringRange);

    // Calculate alpha for sinking ships
    float alpha = 1.0f;
    if (ship.isSinking())
        alpha = 1.0f - ship.getSinkProgress();

    // Apply alpha for sinking ships (no color tint - ships have their own colors baked in)
    Color tint = { 255, 255, 255, (unsigned char) (255 * alpha) };

    int shipType = ship.getShipType();

    if (shipTexturesLoaded && shipHullTextures[shipType].id != 0)
    {
        // Draw hull using texture at its natural size (already scaled on load)
        Texture2D& hullTex = shipHullTextures[shipType];

        float hullWidth = (float) hullTex.width;
        float hullHeight = (float) hullTex.height;

        Rectangle source = { 0, 0, hullWidth, hullHeight };
        Rectangle dest = { pos.x, pos.y, hullWidth, hullHeight };

        // Origin at center for rotation
        Vector2 origin = { hullWidth / 2.0f, hullHeight / 2.0f };

        // Convert angle from radians to degrees, add 90 to rotate from up-pointing image
        float angleDeg = angle * (180.0f / pi) + 90.0f;

        DrawTexturePro (hullTex, source, dest, origin, angleDeg, tint);

        // Draw turrets using textures at their natural size
        float cosA = std::cos (angle);
        float sinA = std::sin (angle);

        const auto& turrets = ship.getTurrets();
        int numTurrets = ship.getNumTurrets();

        for (int i = 0; i < numTurrets; ++i)
        {
            const auto& turret = turrets[i];

            // Get turret position from ship's turret config
            Vec2 localOffset = turret.getLocalOffset();

            // Rotate local offset by ship angle
            Vec2 worldOffset;
            worldOffset.x = localOffset.x * cosA - localOffset.y * sinA;
            worldOffset.y = localOffset.x * sinA + localOffset.y * cosA;

            Vec2 turretPos = pos + worldOffset;

            if (shipTurretTextures[shipType].id != 0)
            {
                Texture2D& turretTex = shipTurretTextures[shipType];

                float turretWidth = (float) turretTex.width;
                float turretHeight = (float) turretTex.height;

                Rectangle turretSource = { 0, 0, turretWidth, turretHeight };
                Rectangle turretDest = { turretPos.x, turretPos.y, turretWidth, turretHeight };
                Vector2 turretOrigin = { turretWidth / 2.0f, turretHeight / 2.0f };

                // Turret angle, add 90 to rotate from up-pointing image
                float turretAngle = turret.getWorldAngle (angle);
                float turretAngleDeg = turretAngle * (180.0f / pi) + 90.0f;

                // Tint turrets with player color so users can identify their ship (subtle blend with white)
                Color shipColor = ship.getColor();
                float blend = 0.5f; // 0 = white, 1 = full color
                Color turretTint = {
                    (unsigned char) (255 * (1 - blend) + shipColor.r * blend),
                    (unsigned char) (255 * (1 - blend) + shipColor.g * blend),
                    (unsigned char) (255 * (1 - blend) + shipColor.b * blend),
                    tint.a
                };
                DrawTexturePro (turretTex, turretSource, turretDest, turretOrigin, turretAngleDeg, turretTint);
            }
        }
    }
}

void Renderer::drawBubbleTrail (const Ship& ship)
{
    for (const auto& bubble : ship.getBubbles())
    {
        unsigned char alpha = (unsigned char) (bubble.alpha * 128);
        Color color = { 255, 255, 255, alpha };
        drawFilledCircle (bubble.position, bubble.radius, color);
    }
}

void Renderer::drawSmoke (const Ship& ship)
{
    // Smoke lightens as ship sinks
    float sinkProgress = ship.getSinkProgress();
    unsigned char greyValue = (unsigned char) (config.smokeGreyStart + sinkProgress * (config.smokeGreyEnd - config.smokeGreyStart));

    for (const auto& s : ship.getSmoke())
    {
        unsigned char alpha = (unsigned char) (s.alpha * 180);
        Color color = { greyValue, greyValue, greyValue, alpha };
        drawFilledCircle (s.position, s.radius, color);
    }
}

void Renderer::drawShell (const Shell& shell)
{
    Vec2 pos = shell.getPosition();
    Vec2 vel = shell.getVelocity();
    float radius = shell.getRadius();

    // Draw gradient trail behind the shell
    if (vel.length() > 0.1f)
    {
        Vec2 trailDir = vel.normalized() * -1.0f; // Opposite to velocity

        for (int i = config.shellTrailSegments; i >= 1; --i)
        {
            float t = (float) i / config.shellTrailSegments;
            Vec2 trailPos = pos + trailDir * (config.shellTrailLength * t);

            // Fade alpha and shrink radius along trail
            float alpha = (1.0f - t) * 0.6f;
            float trailRadius = radius * (1.0f - t * 0.5f);

            Color trailColor = {
                config.colorShell.r,
                config.colorShell.g,
                config.colorShell.b,
                (unsigned char) (255 * alpha)
            };
            drawFilledCircle (trailPos, trailRadius, trailColor);
        }
    }

    // Draw the shell itself
    drawFilledCircle (pos, radius, config.colorShell);
}

void Renderer::drawExplosion (const Explosion& explosion)
{
    float progress = explosion.getProgress();

    // Explosion expands quickly then fades
    float radius = explosion.maxRadius * std::sqrt (progress);
    float alpha = 1.0f - progress;

    if (explosion.isHit)
    {
        // Hit explosion - orange/yellow
        Color outerColor = { config.colorExplosionOuter.r, config.colorExplosionOuter.g, config.colorExplosionOuter.b, (unsigned char) (alpha * config.colorExplosionOuter.a) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            Color midColor = { config.colorExplosionMid.r, config.colorExplosionMid.g, config.colorExplosionMid.b, (unsigned char) (alpha * config.colorExplosionMid.a) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            Color coreColor = { config.colorExplosionCore.r, config.colorExplosionCore.g, config.colorExplosionCore.b, (unsigned char) (alpha * config.colorExplosionCore.a) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
    else
    {
        // Miss splash - blue/white
        Color outerColor = { config.colorSplashOuter.r, config.colorSplashOuter.g, config.colorSplashOuter.b, (unsigned char) (alpha * config.colorSplashOuter.a) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            Color midColor = { config.colorSplashMid.r, config.colorSplashMid.g, config.colorSplashMid.b, (unsigned char) (alpha * config.colorSplashMid.a) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            Color coreColor = { config.colorSplashCore.r, config.colorSplashCore.g, config.colorSplashCore.b, (unsigned char) (alpha * config.colorSplashCore.a) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
}

void Renderer::drawCrosshair (const Ship& ship)
{
    Vec2 position = ship.getCrosshairPosition();
    Color shipColor = ship.getColor();

    // Crosshair is grey if not ready to fire
    Color crosshairColor = ship.isReadyToFire() ? shipColor : config.colorGreyMid;

    float size = 15.0f;

    // Draw crosshair as two perpendicular lines
    drawLine ({ position.x - size, position.y }, { position.x + size, position.y }, crosshairColor);
    drawLine ({ position.x, position.y - size }, { position.x, position.y + size }, crosshairColor);

    // Draw small circle in center
    drawCircle (position, 5.0f, crosshairColor);

    // Draw reload bar below crosshair
    float barWidth = 40.0f;
    float barHeight = 4.0f;
    float barY = position.y + size + 8.0f;
    drawFilledRect ({ position.x - barWidth / 2.0f, barY }, barWidth, barHeight, config.colorBarBackground);

    float reloadPct = ship.getReloadProgress();
    Color reloadColor = reloadPct >= 1.0f ? config.colorReloadReady : config.colorReloadNotReady;
    drawFilledRect ({ position.x - barWidth / 2.0f, barY }, barWidth * reloadPct, barHeight, reloadColor);

    // Draw turret indicator circles below reload bar (only for actual turrets)
    int numTurrets = ship.getNumTurrets();
    float circleY = barY + barHeight + 6.0f;
    float circleRadius = 4.0f;
    float circleSpacing = 12.0f;
    float startX = position.x - (numTurrets - 1) * circleSpacing / 2.0f;

    const auto& turrets = ship.getTurrets();
    for (int i = 0; i < numTurrets; ++i)
    {
        Vec2 circlePos = { startX + i * circleSpacing, circleY };
        bool isReady = turrets[i].isLoaded() && turrets[i].isAimedAtTarget();
        Color circleColor = isReady ? shipColor : config.colorBarBackground;
        drawFilledCircle (circlePos, circleRadius, circleColor);
    }
}

void Renderer::drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth, float hudWidth, float alpha)
{
    float hudHeight = 50.0f;
    float spacing = 10.0f;

    // Center HUDs horizontally
    float totalWidth = totalSlots * hudWidth + (totalSlots - 1) * spacing;
    float startX = (screenWidth - totalWidth) / 2.0f;
    float x = startX + slot * (hudWidth + spacing);
    float y = 10.0f;

    unsigned char a = (unsigned char) (alpha * 255);

    Color shipColor = { ship.getColor().r, ship.getColor().g, ship.getColor().b, a };
    Color bgColor = { config.colorHudBackground.r, config.colorHudBackground.g, config.colorHudBackground.b, (unsigned char) (alpha * config.colorHudBackground.a) };
    Color barBg = { config.colorBarBackground.r, config.colorBarBackground.g, config.colorBarBackground.b, a };
    Color white = { config.colorWhite.r, config.colorWhite.g, config.colorWhite.b, a };

    // Background
    drawFilledRect ({ x, y }, hudWidth, hudHeight, bgColor);
    drawRect ({ x, y }, hudWidth, hudHeight, shipColor);

    // Player label
    int playerNum = ship.getPlayerIndex() + 1;
    std::string label = std::to_string (playerNum);
    float labelScale = hudWidth < 120.0f ? 1.5f : 2.0f;
    drawText (label, { x + 3, y + 3 }, labelScale, shipColor);

    // Speed in knots (actual speed - faster ships show higher knots)
    float speedKnots = (ship.getSpeed() / config.shipMaxSpeed) * config.shipFullSpeedKnots;
    std::string speedText = std::to_string ((int) std::round (speedKnots)) + "KT";
    Color speedColor = { config.colorGreyLight.r, config.colorGreyLight.g, config.colorGreyLight.b, a };
    drawText (speedText, { x + 3, y + 20 }, 1.0f, speedColor);

    // Bars start after label area - scale with HUD width
    float labelWidth = hudWidth < 120.0f ? 28.0f : 35.0f;
    float barX = x + labelWidth;
    float barWidth = hudWidth - labelWidth - 5.0f;
    float barHeight = 8.0f;

    // Health bar
    float healthY = y + 5;
    drawFilledRect ({ barX, healthY }, barWidth, barHeight, barBg);
    float healthPct = ship.getHealth() / ship.getMaxHealth();
    Color healthColor = { (unsigned char) (255 * (1 - healthPct)), (unsigned char) (255 * healthPct), 0, a };
    drawFilledRect ({ barX, healthY }, barWidth * healthPct, barHeight, healthColor);

    // Throttle bar (centered, negative goes left, positive goes right)
    float throttleY = y + 20;
    drawFilledRect ({ barX, throttleY }, barWidth, barHeight, barBg);
    float throttle = ship.getThrottle();
    float throttleCenter = barX + barWidth / 2.0f;
    Color throttleColor = { config.colorThrottleBar.r, config.colorThrottleBar.g, config.colorThrottleBar.b, a };
    if (throttle > 0)
    {
        drawFilledRect ({ throttleCenter, throttleY }, barWidth / 2.0f * throttle, barHeight, throttleColor);
    }
    else if (throttle < 0)
    {
        float negWidth = barWidth / 2.0f * (-throttle);
        drawFilledRect ({ throttleCenter - negWidth, throttleY }, negWidth, barHeight, throttleColor);
    }
    // Center line
    drawLine ({ throttleCenter, throttleY }, { throttleCenter, throttleY + barHeight }, white);

    // Rudder bar (centered, negative goes left, positive goes right)
    float rudderY = y + 35;
    drawFilledRect ({ barX, rudderY }, barWidth, barHeight, barBg);
    float rudder = ship.getRudder();
    float rudderCenter = barX + barWidth / 2.0f;
    Color rudderColor = { config.colorRudderBar.r, config.colorRudderBar.g, config.colorRudderBar.b, a };
    if (rudder > 0)
    {
        drawFilledRect ({ rudderCenter, rudderY }, barWidth / 2.0f * rudder, barHeight, rudderColor);
    }
    else if (rudder < 0)
    {
        float negWidth = barWidth / 2.0f * (-rudder);
        drawFilledRect ({ rudderCenter - negWidth, rudderY }, negWidth, barHeight, rudderColor);
    }
    // Center line
    drawLine ({ rudderCenter, rudderY }, { rudderCenter, rudderY + barHeight }, white);
}

void Renderer::drawWindIndicator (Vec2 wind, float screenWidth, float screenHeight)
{
    // Draw wind indicator in bottom-left corner
    float indicatorSize = 20.0f;
    Vec2 center = { 35, screenHeight - 35 };

    // Background circle
    drawFilledCircle (center, indicatorSize, config.colorWindBackground);
    drawCircle (center, indicatorSize, config.colorWindBorder);

    // Wind arrow
    float windStrength = wind.length();
    if (windStrength > 0.01f)
    {
        Vec2 windDir = wind.normalized();
        float arrowLength = indicatorSize * 0.8f * windStrength;

        Vec2 arrowEnd = center + windDir * arrowLength;

        // Arrow shaft
        drawLine (center, arrowEnd, config.colorWindArrow);

        // Arrow head
        float headSize = 4.0f;
        Vec2 head1 = arrowEnd - windDir * headSize + Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        Vec2 head2 = arrowEnd - windDir * headSize - Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        drawLine (arrowEnd, head1, config.colorWindArrow);
        drawLine (arrowEnd, head2, config.colorWindArrow);
    }

    // Label
    drawText ("WIND", { center.x - 6, center.y + indicatorSize + 3 }, 0.75f, config.colorGreyLight);
}

void Renderer::drawOval (Vec2 center, float width, float height, float angle, Color color)
{
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    int segments = 32;
    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (2.0f * pi * i) / segments;
        float theta2 = (2.0f * pi * (i + 1)) / segments;

        // Points on unrotated ellipse
        float x1 = (width / 2.0f) * std::cos (theta1);
        float y1 = (height / 2.0f) * std::sin (theta1);
        float x2 = (width / 2.0f) * std::cos (theta2);
        float y2 = (height / 2.0f) * std::sin (theta2);

        // Rotate points
        float rx1 = x1 * cosA - y1 * sinA;
        float ry1 = x1 * sinA + y1 * cosA;
        float rx2 = x2 * cosA - y2 * sinA;
        float ry2 = x2 * sinA + y2 * cosA;

        DrawLine ((int) (center.x + rx1), (int) (center.y + ry1),
                  (int) (center.x + rx2), (int) (center.y + ry2), color);
    }
}

void Renderer::drawFilledOval (Vec2 center, float width, float height, float angle, Color color)
{
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    // Draw filled oval using horizontal scanlines
    int numLines = (int) (std::max (width, height) / 2.0f);

    for (int i = -numLines; i <= numLines; ++i)
    {
        float t = (float) i / numLines;
        float localY = t * (height / 2.0f);

        // Width at this Y position (ellipse equation)
        float localHalfWidth = (width / 2.0f) * std::sqrt (1.0f - t * t);

        if (localHalfWidth > 0.0f)
        {
            // Two endpoints of the scanline in local coords
            float x1 = -localHalfWidth;
            float x2 = localHalfWidth;

            // Rotate to world coords
            float wx1 = x1 * cosA - localY * sinA + center.x;
            float wy1 = x1 * sinA + localY * cosA + center.y;
            float wx2 = x2 * cosA - localY * sinA + center.x;
            float wy2 = x2 * sinA + localY * cosA + center.y;

            DrawLine ((int) wx1, (int) wy1, (int) wx2, (int) wy2, color);
        }
    }

    // Draw outline for better visibility
    Color outlineColor = {
        (unsigned char) (color.r * 0.5f),
        (unsigned char) (color.g * 0.5f),
        (unsigned char) (color.b * 0.5f),
        255
    };
    drawOval (center, width, height, angle, outlineColor);
}

void Renderer::drawCircle (Vec2 center, float radius, Color color)
{
    DrawCircleLinesV ({ center.x, center.y }, radius, color);
}

void Renderer::drawFilledCircle (Vec2 center, float radius, Color color)
{
    DrawCircleV ({ center.x, center.y }, radius, color);
}

void Renderer::drawLine (Vec2 start, Vec2 end, Color color)
{
    DrawLineV ({ start.x, start.y }, { end.x, end.y }, color);
}

void Renderer::drawRect (Vec2 topLeft, float width, float height, Color color)
{
    DrawRectangleLinesEx ({ topLeft.x, topLeft.y, width, height }, 1.0f, color);
}

void Renderer::drawFilledRect (Vec2 topLeft, float width, float height, Color color)
{
    DrawRectangleV ({ topLeft.x, topLeft.y }, { width, height }, color);
}

// Simple 5x7 bitmap font patterns (each char is 5 wide, 7 tall)
static const uint8_t* getGlyph (unsigned char c)
{
    static const uint8_t empty[7] = { 0, 0, 0, 0, 0, 0, 0 };

    // Numbers 0-9
    static const uint8_t g0[7] = { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 };
    static const uint8_t g1[7] = { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
    static const uint8_t g2[7] = { 0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111 };
    static const uint8_t g3[7] = { 0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110 };
    static const uint8_t g4[7] = { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 };
    static const uint8_t g5[7] = { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 };
    static const uint8_t g6[7] = { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 };
    static const uint8_t g7[7] = { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 };
    static const uint8_t g8[7] = { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 };
    static const uint8_t g9[7] = { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 };

    // Letters A-Z
    static const uint8_t gA[7] = { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
    static const uint8_t gB[7] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 };
    static const uint8_t gC[7] = { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 };
    static const uint8_t gD[7] = { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 };
    static const uint8_t gE[7] = { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 };
    static const uint8_t gF[7] = { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 };
    static const uint8_t gG[7] = { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110 };
    static const uint8_t gH[7] = { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
    static const uint8_t gI[7] = { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
    static const uint8_t gJ[7] = { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 };
    static const uint8_t gK[7] = { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 };
    static const uint8_t gL[7] = { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 };
    static const uint8_t gM[7] = { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 };
    static const uint8_t gN[7] = { 0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001 };
    static const uint8_t gO[7] = { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
    static const uint8_t gP[7] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 };
    static const uint8_t gQ[7] = { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 };
    static const uint8_t gR[7] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 };
    static const uint8_t gS[7] = { 0b01110, 0b10001, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110 };
    static const uint8_t gT[7] = { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 };
    static const uint8_t gU[7] = { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
    static const uint8_t gV[7] = { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 };
    static const uint8_t gW[7] = { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 };
    static const uint8_t gX[7] = { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 };
    static const uint8_t gY[7] = { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 };
    static const uint8_t gZ[7] = { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 };

    // Punctuation
    static const uint8_t gExclaim[7] = { 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100 };
    static const uint8_t gColon[7] = { 0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000 };
    static const uint8_t gDash[7] = { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 };

    switch (c)
    {
        case '0': return g0;
        case '1': return g1;
        case '2': return g2;
        case '3': return g3;
        case '4': return g4;
        case '5': return g5;
        case '6': return g6;
        case '7': return g7;
        case '8': return g8;
        case '9': return g9;
        case 'A': return gA;
        case 'B': return gB;
        case 'C': return gC;
        case 'D': return gD;
        case 'E': return gE;
        case 'F': return gF;
        case 'G': return gG;
        case 'H': return gH;
        case 'I': return gI;
        case 'J': return gJ;
        case 'K': return gK;
        case 'L': return gL;
        case 'M': return gM;
        case 'N': return gN;
        case 'O': return gO;
        case 'P': return gP;
        case 'Q': return gQ;
        case 'R': return gR;
        case 'S': return gS;
        case 'T': return gT;
        case 'U': return gU;
        case 'V': return gV;
        case 'W': return gW;
        case 'X': return gX;
        case 'Y': return gY;
        case 'Z': return gZ;
        case '!': return gExclaim;
        case ':': return gColon;
        case '-': return gDash;
        default: return empty;
    }
}

void Renderer::drawChar (char c, Vec2 position, float scale, Color color)
{
    unsigned char uc = (unsigned char) c;
    const uint8_t* glyph = getGlyph (uc);

    // Use ceiling for size to ensure no gaps between pixels
    float pixelSize = std::ceil (scale);

    for (int row = 0; row < 7; ++row)
    {
        uint8_t rowBits = glyph[row];
        for (int col = 0; col < 5; ++col)
        {
            if (rowBits & (1 << (4 - col)))
            {
                Rectangle rect = {
                    position.x + col * scale,
                    position.y + row * scale,
                    pixelSize,
                    pixelSize
                };
                DrawRectangleRec (rect, color);
            }
        }
    }
}

void Renderer::drawText (const std::string& text, Vec2 position, float scale, Color color)
{
    float charWidth = 6 * scale; // 5 pixels + 1 spacing
    Vec2 pos = position;

    for (char c : text)
    {
        char upper = (c >= 'a' && c <= 'z') ? (c - 32) : c;
        drawChar (upper, pos, scale, color);
        pos.x += charWidth;
    }
}

void Renderer::drawTextCentered (const std::string& text, Vec2 center, float scale, Color color)
{
    float charWidth = 6 * scale;
    float charHeight = 7 * scale;
    float textWidth = text.length() * charWidth;

    Vec2 topLeft = {
        center.x - textWidth / 2.0f,
        center.y - charHeight / 2.0f
    };

    drawText (text, topLeft, scale, color);
}

void Renderer::createNoiseTexture()
{
    // Create two noise textures for water highlights with different patterns
    // This reduces visible repetition when tiling

    auto generateNoiseImage = [] (unsigned int seed) -> Image
    {
        Image noiseImage = GenImageColor (noiseTextureSize, noiseTextureSize, { 0, 0, 0, 0 });

        // Simple LCG random number generator
        auto nextRandom = [&seed]() -> unsigned int
        {
            seed = seed * 1103515245 + 12345;
            return (seed >> 16) & 0x7FFF;
        };

        for (int y = 0; y < noiseTextureSize; ++y)
        {
            for (int x = 0; x < noiseTextureSize; ++x)
            {
                unsigned int r = nextRandom() % 100;
                Color pixelColor = { 0, 0, 0, 0 };

                if (r < 8)
                {
                    // Bright highlight (8%)
                    pixelColor = config.colorWaterHighlight1;
                }
                else if (r < 20)
                {
                    // Medium highlight (12%)
                    pixelColor = config.colorWaterHighlight2;
                }
                else if (r < 35)
                {
                    // Subtle highlight (15%)
                    pixelColor = config.colorWaterHighlight3;
                }

                ImageDrawPixel (&noiseImage, x, y, pixelColor);
            }
        }

        return noiseImage;
    };

    // Create first texture
    Image noiseImage1 = generateNoiseImage (12345);
    noiseTexture1 = LoadTextureFromImage (noiseImage1);
    UnloadImage (noiseImage1);
    SetTextureFilter (noiseTexture1, TEXTURE_FILTER_BILINEAR);

    // Create second texture with different seed
    Image noiseImage2 = generateNoiseImage (67890);
    noiseTexture2 = LoadTextureFromImage (noiseImage2);
    UnloadImage (noiseImage2);
    SetTextureFilter (noiseTexture2, TEXTURE_FILTER_BILINEAR);
}

void Renderer::loadShipTextures()
{
    // Scale factor applied to all ship textures on load
    const float shipTextureScale = 0.25f;

    // New ship textures: ship1.png (1 turret) through ship4.png (4 turrets)
    const char* hullPaths[NUM_SHIP_TYPES] = {
        "assets/ships/ship1.png",
        "assets/ships/ship2.png",
        "assets/ships/ship3.png",
        "assets/ships/ship4.png"
    };

    const char* turretPaths[NUM_SHIP_TYPES] = {
        "assets/ships/turret1.png",
        "assets/ships/turret2.png",
        "assets/ships/turret3.png",
        "assets/ships/turret4.png"
    };

    shipTexturesLoaded = true;

    for (int i = 0; i < NUM_SHIP_TYPES; ++i)
    {
        // Load hull image, scale it, and keep for pixel-perfect hit testing
        Image hullImage = LoadImage (getResourcePath (hullPaths[i]).c_str());
        if (hullImage.data != nullptr)
        {
            int newWidth = (int) (hullImage.width * shipTextureScale);
            int newHeight = (int) (hullImage.height * shipTextureScale);
            ImageResize (&hullImage, newWidth, newHeight);

            shipHullImages[i] = hullImage;
            shipHullTextures[i] = LoadTextureFromImage (shipHullImages[i]);
            SetTextureFilter (shipHullTextures[i], TEXTURE_FILTER_BILINEAR);
        }
        else
        {
            TraceLog (LOG_WARNING, "Failed to load ship hull texture: %s", hullPaths[i]);
            shipTexturesLoaded = false;
        }

        // Load turret image and scale it
        Image turretImage = LoadImage (getResourcePath (turretPaths[i]).c_str());
        if (turretImage.data != nullptr)
        {
            int newWidth = (int) (turretImage.width * shipTextureScale);
            int newHeight = (int) (turretImage.height * shipTextureScale);
            ImageResize (&turretImage, newWidth, newHeight);

            shipTurretTextures[i] = LoadTextureFromImage (turretImage);
            SetTextureFilter (shipTurretTextures[i], TEXTURE_FILTER_BILINEAR);
            UnloadImage (turretImage);
        }
        else
        {
            TraceLog (LOG_WARNING, "Failed to load ship turret texture: %s", turretPaths[i]);
            shipTexturesLoaded = false;
        }
    }
}

float Renderer::getShipLength (int shipType) const
{
    // Ship length is the hull texture height (bow points up in image)
    int idx = std::clamp (shipType, 0, NUM_SHIP_TYPES - 1);
    if (shipTexturesLoaded && shipHullTextures[idx].id != 0)
        return (float) shipHullTextures[idx].height;
    return 100.0f; // Fallback
}

float Renderer::getShipWidth (int shipType) const
{
    // Ship width is the hull texture width
    int idx = std::clamp (shipType, 0, NUM_SHIP_TYPES - 1);
    if (shipTexturesLoaded && shipHullTextures[idx].id != 0)
        return (float) shipHullTextures[idx].width;
    return 25.0f; // Fallback
}

int Renderer::getShipColorIndex (const Ship& ship) const
{
    int team = ship.getTeam();
    if (team >= 0)
        return team; // Team mode: 0=Red, 1=Blue

    // FFA mode: use player index (0=Red, 1=Blue, 2=Green, 3=Yellow)
    return std::clamp (ship.getPlayerIndex(), 0, 3);
}

void Renderer::drawShipPreview (int shipType, Vec2 position, float angle, int playerIndex)
{
    int idx = std::clamp (shipType, 0, NUM_SHIP_TYPES - 1);

    if (!shipTexturesLoaded || shipHullTextures[idx].id == 0)
        return;

    Texture2D& hullTex = shipHullTextures[idx];

    float hullWidth = (float) hullTex.width;
    float hullHeight = (float) hullTex.height;

    Rectangle source = { 0, 0, hullWidth, hullHeight };
    Rectangle dest = { position.x, position.y, hullWidth, hullHeight };

    Vector2 origin = { hullWidth / 2.0f, hullHeight / 2.0f };
    float angleDeg = angle * (180.0f / pi) + 90.0f;

    DrawTexturePro (hullTex, source, dest, origin, angleDeg, WHITE);

    // Get player color for turret tinting (subtle blend with white)
    Color shipColor;
    switch (playerIndex)
    {
        case 0: shipColor = config.colorShipRed; break;
        case 1: shipColor = config.colorShipBlue; break;
        case 2: shipColor = config.colorShipGreen; break;
        case 3: shipColor = config.colorShipYellow; break;
        default: shipColor = WHITE; break;
    }
    float blend = 0.5f; // 0 = white, 1 = full color
    Color turretTint = {
        (unsigned char) (255 * (1 - blend) + shipColor.r * blend),
        (unsigned char) (255 * (1 - blend) + shipColor.g * blend),
        (unsigned char) (255 * (1 - blend) + shipColor.b * blend),
        255
    };

    // Draw turrets at their default positions
    int numTurrets = config.shipTypes[idx].numTurrets;
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    if (shipTurretTextures[idx].id != 0)
    {
        Texture2D& turretTex = shipTurretTextures[idx];
        float turretWidth = (float) turretTex.width;
        float turretHeight = (float) turretTex.height;

        for (int i = 0; i < numTurrets; ++i)
        {
            // Get turret position from config
            float offsetPct = config.shipTypes[idx].turrets[i].localOffsetX;
            float localX = offsetPct * hullHeight;

            Vec2 worldOffset;
            worldOffset.x = localX * cosA;
            worldOffset.y = localX * sinA;

            Vec2 turretPos = position + worldOffset;

            Rectangle turretSource = { 0, 0, turretWidth, turretHeight };
            Rectangle turretDest = { turretPos.x, turretPos.y, turretWidth, turretHeight };
            Vector2 turretOrigin = { turretWidth / 2.0f, turretHeight / 2.0f };

            // Front turrets point forward, rear turrets point backward
            bool isFront = config.shipTypes[idx].turrets[i].isFront;
            float turretAngleDeg = isFront ? angleDeg : angleDeg + 180.0f;
            DrawTexturePro (turretTex, turretSource, turretDest, turretOrigin, turretAngleDeg, turretTint);
        }
    }
}

bool Renderer::checkShipHit (const Ship& ship, Vec2 worldPos) const
{
    if (!shipTexturesLoaded)
        return true; // Fallback to always hit if no textures

    int texIdx = ship.getShipType();
    const Image& img = shipHullImages[texIdx];
    if (img.data == nullptr)
        return true; // Fallback

    // Transform world position to ship-local coordinates
    Vec2 shipPos = ship.getPosition();
    float angle = ship.getAngle();
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    float dx = worldPos.x - shipPos.x;
    float dy = worldPos.y - shipPos.y;

    // Rotate to ship-local (X = forward toward bow, Y = starboard)
    float localX = dx * cosA + dy * sinA;
    float localY = -dx * sinA + dy * cosA;

    // Convert to image coordinates
    // Image has bow pointing UP, so:
    // - imageX corresponds to ship's Y (starboard)
    // - imageY corresponds to -ship's X (bow is up = negative Y in image)
    float imgCenterX = img.width / 2.0f;
    float imgCenterY = img.height / 2.0f;

    int imageX = (int) (imgCenterX + localY);
    int imageY = (int) (imgCenterY - localX);

    // Check bounds
    if (imageX < 0 || imageX >= img.width || imageY < 0 || imageY >= img.height)
        return false; // Outside image bounds = miss

    // Get pixel color and check alpha
    Color pixel = GetImageColor (img, imageX, imageY);
    return pixel.a > 0; // Hit if not fully transparent
}

bool Renderer::checkShipCollision (const Ship& shipA, const Ship& shipB, Vec2& collisionPoint) const
{
    if (!shipTexturesLoaded)
        return false;

    int texIdxA = shipA.getShipType();
    const Image& imgA = shipHullImages[texIdxA];
    if (imgA.data == nullptr)
        return false;

    // Quick bounding check first
    Vec2 posA = shipA.getPosition();
    Vec2 posB = shipB.getPosition();
    float maxDist = (shipA.getLength() + shipB.getLength()) / 2.0f;
    if ((posA - posB).length() > maxDist)
        return false;

    // Sample points along ship A's hull outline and check against ship B
    float angleA = shipA.getAngle();
    float cosA = std::cos (angleA);
    float sinA = std::sin (angleA);

    // Scan through ship A's image and check non-transparent edge pixels against ship B
    int stepSize = 2; // Check every 2nd pixel for performance
    for (int iy = 0; iy < imgA.height; iy += stepSize)
    {
        for (int ix = 0; ix < imgA.width; ix += stepSize)
        {
            Color pixelA = GetImageColor (imgA, ix, iy);
            if (pixelA.a == 0)
                continue; // Skip transparent pixels

            // Convert image coords to ship-local coords
            float localX = (imgA.height / 2.0f) - iy; // Forward direction
            float localY = ix - (imgA.width / 2.0f);  // Starboard direction

            // Convert to world coords
            Vec2 worldPos;
            worldPos.x = posA.x + localX * cosA - localY * sinA;
            worldPos.y = posA.y + localX * sinA + localY * cosA;

            // Check if this point hits ship B
            if (checkShipHit (shipB, worldPos))
            {
                collisionPoint = worldPos;
                return true;
            }
        }
    }

    return false;
}

void Renderer::drawIsland (const Island& island)
{
    const auto& vertices = island.getVertices();
    if (vertices.size() < 3)
        return;

    // Calculate bounding box
    float minX = vertices[0].x, maxX = vertices[0].x;
    float minY = vertices[0].y, maxY = vertices[0].y;
    for (const auto& v : vertices)
    {
        minX = std::min (minX, v.x);
        maxX = std::max (maxX, v.x);
        minY = std::min (minY, v.y);
        maxY = std::max (maxY, v.y);
    }

    // Fill using horizontal scanlines
    for (int y = (int) minY; y <= (int) maxY; ++y)
    {
        std::vector<float> intersections;
        int n = (int) vertices.size();

        for (int i = 0; i < n; ++i)
        {
            Vec2 v1 = vertices[i];
            Vec2 v2 = vertices[(i + 1) % n];

            if ((v1.y <= y && v2.y > y) || (v2.y <= y && v1.y > y))
            {
                float xIntersect = v1.x + (y - v1.y) / (v2.y - v1.y) * (v2.x - v1.x);
                intersections.push_back (xIntersect);
            }
        }

        std::sort (intersections.begin(), intersections.end());

        for (size_t i = 0; i + 1 < intersections.size(); i += 2)
        {
            int x1 = (int) intersections[i];
            int x2 = (int) intersections[i + 1];
            DrawLine (x1, y, x2, y, config.colorIslandSand);
        }
    }

    // Draw outline
    int n = (int) vertices.size();
    for (int i = 0; i < n; ++i)
    {
        Vec2 v1 = vertices[i];
        Vec2 v2 = vertices[(i + 1) % n];
        DrawLine ((int) v1.x, (int) v1.y, (int) v2.x, (int) v2.y, config.colorIslandOutline);
    }
}
