#include "Renderer.h"
#include "Config.h"
#include "Game.h"
#include "Shell.h"
#include "Ship.h"
#include <cmath>
#include <vector>

Renderer::Renderer()
{
    createNoiseTexture();
}

Renderer::~Renderer()
{
    if (noiseTexture1.id != 0)
        UnloadTexture (noiseTexture1);
    if (noiseTexture2.id != 0)
        UnloadTexture (noiseTexture2);
}

void Renderer::clear()
{
    ClearBackground (Config::colorOcean);
}

void Renderer::drawWater (float time, float screenWidth, float screenHeight)
{
    // Base ocean color
    ClearBackground (Config::colorOcean);

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
    Color color = ship.getColor();

    // Draw firing range circle (very faint white) - only for non-sinking ships
    if (ship.isAlive())
        drawFilledCircle (pos, Config::maxCrosshairDistance, Config::colorFiringRange);

    // Calculate alpha for sinking ships
    float alpha = 1.0f;
    if (ship.isSinking())
        alpha = 1.0f - ship.getSinkProgress();

    // Apply alpha to ship color
    color.a = (unsigned char) (255 * alpha);

    // Draw ship hull - pointy bow, squarer stern
    drawShipHull (pos, ship.getLength(), ship.getWidth(), angle, color);

    // Draw turrets
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    for (const auto& turret : ship.getTurrets())
    {
        Vec2 localOffset = turret.getLocalOffset();

        // Rotate local offset by ship angle
        Vec2 worldOffset;
        worldOffset.x = localOffset.x * cosA - localOffset.y * sinA;
        worldOffset.y = localOffset.x * sinA + localOffset.y * cosA;

        Vec2 turretPos = pos + worldOffset;

        // Draw turret base (darker circle)
        Color turretColor = {
            (unsigned char) (color.r * 0.6f),
            (unsigned char) (color.g * 0.6f),
            (unsigned char) (color.b * 0.6f),
            (unsigned char) (255 * alpha)
        };
        drawFilledCircle (turretPos, turret.getRadius(), turretColor);

        // Draw barrel (line from turret center outward)
        float turretAngle = turret.getWorldAngle (angle);
        Vec2 barrelEnd = turretPos + Vec2::fromAngle (turretAngle) * turret.getBarrelLength();
        Color barrelColor = { Config::colorBarrel.r, Config::colorBarrel.g, Config::colorBarrel.b, (unsigned char) (255 * alpha) };
        drawLine (turretPos, barrelEnd, barrelColor);
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
    unsigned char greyValue = (unsigned char) (Config::smokeGreyStart + sinkProgress * (Config::smokeGreyEnd - Config::smokeGreyStart));

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

        for (int i = Config::shellTrailSegments; i >= 1; --i)
        {
            float t = (float) i / Config::shellTrailSegments;
            Vec2 trailPos = pos + trailDir * (Config::shellTrailLength * t);

            // Fade alpha and shrink radius along trail
            float alpha = (1.0f - t) * 0.6f;
            float trailRadius = radius * (1.0f - t * 0.5f);

            Color trailColor = {
                Config::colorShell.r,
                Config::colorShell.g,
                Config::colorShell.b,
                (unsigned char) (255 * alpha)
            };
            drawFilledCircle (trailPos, trailRadius, trailColor);
        }
    }

    // Draw the shell itself
    drawFilledCircle (pos, radius, Config::colorShell);
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
        Color outerColor = { Config::colorExplosionOuter.r, Config::colorExplosionOuter.g, Config::colorExplosionOuter.b, (unsigned char) (alpha * Config::colorExplosionOuter.a) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            Color midColor = { Config::colorExplosionMid.r, Config::colorExplosionMid.g, Config::colorExplosionMid.b, (unsigned char) (alpha * Config::colorExplosionMid.a) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            Color coreColor = { Config::colorExplosionCore.r, Config::colorExplosionCore.g, Config::colorExplosionCore.b, (unsigned char) (alpha * Config::colorExplosionCore.a) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
    else
    {
        // Miss splash - blue/white
        Color outerColor = { Config::colorSplashOuter.r, Config::colorSplashOuter.g, Config::colorSplashOuter.b, (unsigned char) (alpha * Config::colorSplashOuter.a) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            Color midColor = { Config::colorSplashMid.r, Config::colorSplashMid.g, Config::colorSplashMid.b, (unsigned char) (alpha * Config::colorSplashMid.a) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            Color coreColor = { Config::colorSplashCore.r, Config::colorSplashCore.g, Config::colorSplashCore.b, (unsigned char) (alpha * Config::colorSplashCore.a) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
}

void Renderer::drawCrosshair (const Ship& ship)
{
    Vec2 position = ship.getCrosshairPosition();
    Color shipColor = ship.getColor();

    // Crosshair is grey if not ready to fire
    Color crosshairColor = ship.isReadyToFire() ? shipColor : Config::colorGreyMid;

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
    drawFilledRect ({ position.x - barWidth / 2.0f, barY }, barWidth, barHeight, Config::colorBarBackground);

    float reloadPct = ship.getReloadProgress();
    Color reloadColor = reloadPct >= 1.0f ? Config::colorReloadReady : Config::colorReloadNotReady;
    drawFilledRect ({ position.x - barWidth / 2.0f, barY }, barWidth * reloadPct, barHeight, reloadColor);

    // Draw 4 turret indicator circles below reload bar
    float circleY = barY + barHeight + 6.0f;
    float circleRadius = 4.0f;
    float circleSpacing = 12.0f;
    float startX = position.x - 1.5f * circleSpacing;

    const auto& turrets = ship.getTurrets();
    for (int i = 0; i < 4; ++i)
    {
        Vec2 circlePos = { startX + i * circleSpacing, circleY };
        Color circleColor = turrets[i].isAimedAtTarget() ? shipColor : Config::colorBarBackground;
        drawFilledCircle (circlePos, circleRadius, circleColor);
    }
}

void Renderer::drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth, float alpha)
{
    float hudWidth = 200.0f;
    float hudHeight = 50.0f;
    float spacing = 20.0f;
    float totalWidth = totalSlots * hudWidth + (totalSlots - 1) * spacing;
    float startX = (screenWidth - totalWidth) / 2.0f;
    float x = startX + slot * (hudWidth + spacing);
    float y = 10.0f;

    unsigned char a = (unsigned char) (alpha * 255);

    Color shipColor = { ship.getColor().r, ship.getColor().g, ship.getColor().b, a };
    Color bgColor = { Config::colorHudBackground.r, Config::colorHudBackground.g, Config::colorHudBackground.b, (unsigned char) (alpha * Config::colorHudBackground.a) };
    Color barBg = { Config::colorBarBackground.r, Config::colorBarBackground.g, Config::colorBarBackground.b, a };
    Color white = { Config::colorWhite.r, Config::colorWhite.g, Config::colorWhite.b, a };

    // Background
    drawFilledRect ({ x, y }, hudWidth, hudHeight, bgColor);
    drawRect ({ x, y }, hudWidth, hudHeight, shipColor);

    // Player label
    std::string label = "P" + std::to_string (ship.getPlayerIndex() + 1);
    drawText (label, { x + 5, y + 3 }, 2.0f, shipColor);

    // Speed in knots
    float speedKnots = (ship.getSpeed() / Config::shipMaxSpeed) * Config::shipFullSpeedKnots;
    std::string speedText = std::to_string ((int) std::round (speedKnots)) + "KT";
    Color speedColor = { Config::colorGreyLight.r, Config::colorGreyLight.g, Config::colorGreyLight.b, a };
    drawText (speedText, { x + 5, y + 20 }, 1.0f, speedColor);

    float barX = x + 35;
    float barWidth = hudWidth - 45;
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
    Color throttleColor = { Config::colorThrottleBar.r, Config::colorThrottleBar.g, Config::colorThrottleBar.b, a };
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
    Color rudderColor = { Config::colorRudderBar.r, Config::colorRudderBar.g, Config::colorRudderBar.b, a };
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
    // Draw wind indicator in upper-right corner
    float indicatorSize = 20.0f;
    Vec2 center = { screenWidth - 35, 35 };

    // Background circle
    drawFilledCircle (center, indicatorSize, Config::colorWindBackground);
    drawCircle (center, indicatorSize, Config::colorWindBorder);

    // Wind arrow
    float windStrength = wind.length();
    if (windStrength > 0.01f)
    {
        Vec2 windDir = wind.normalized();
        float arrowLength = indicatorSize * 0.8f * windStrength;

        Vec2 arrowEnd = center + windDir * arrowLength;

        // Arrow shaft
        drawLine (center, arrowEnd, Config::colorWindArrow);

        // Arrow head
        float headSize = 4.0f;
        Vec2 head1 = arrowEnd - windDir * headSize + Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        Vec2 head2 = arrowEnd - windDir * headSize - Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        drawLine (arrowEnd, head1, Config::colorWindArrow);
        drawLine (arrowEnd, head2, Config::colorWindArrow);
    }

    // Label
    drawText ("WIND", { center.x - 6, center.y + indicatorSize + 3 }, 0.75f, Config::colorGreyLight);
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

void Renderer::drawShipHull (Vec2 center, float length, float width, float angle, Color color)
{
    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    float halfLength = length / 2.0f;
    float halfWidth = width / 2.0f;

    // Helper lambda to get hull half-width at a given t position (-1 to 1)
    auto getHalfWidth = [halfWidth] (float t) -> float
    {
        if (t > 0.7f)
        {
            // Bow section - taper to a point
            float bowT = (t - 0.7f) / 0.3f;
            return halfWidth * (1.0f - bowT * bowT);
        }
        else if (t < -0.6f)
        {
            // Stern section - slightly rounded but more square
            float sternT = (-t - 0.6f) / 0.4f;
            return halfWidth * (1.0f - sternT * sternT * 0.3f);
        }
        else
        {
            // Main body - full width with slight curve
            float bodyT = (t + 0.6f) / 1.3f;
            return halfWidth * (1.0f + 0.05f * std::sin (bodyT * pi));
        }
    };

    // Build hull outline points
    std::vector<Vector2> hullPoints;
    int segments = 32;

    // Top edge (stern to bow)
    for (int i = 0; i <= segments; ++i)
    {
        float t = (float) i / segments * 2.0f - 1.0f;
        float localX = t * halfLength;
        float localY = getHalfWidth (t);

        float wx = localX * cosA - localY * sinA + center.x;
        float wy = localX * sinA + localY * cosA + center.y;
        hullPoints.push_back ({ wx, wy });
    }

    // Bottom edge (bow to stern)
    for (int i = segments; i >= 0; --i)
    {
        float t = (float) i / segments * 2.0f - 1.0f;
        float localX = t * halfLength;
        float localY = -getHalfWidth (t);

        float wx = localX * cosA - localY * sinA + center.x;
        float wy = localX * sinA + localY * cosA + center.y;
        hullPoints.push_back ({ wx, wy });
    }

    // Draw filled polygon using triangle fan from center
    Vector2 centerVec = { center.x, center.y };
    for (size_t i = 0; i < hullPoints.size(); ++i)
    {
        size_t next = (i + 1) % hullPoints.size();
        DrawTriangle (centerVec, hullPoints[i], hullPoints[next], color);
    }

    // Draw smooth outline
    Color outlineColor = {
        (unsigned char) (color.r * 0.5f),
        (unsigned char) (color.g * 0.5f),
        (unsigned char) (color.b * 0.5f),
        color.a
    };

    for (size_t i = 0; i < hullPoints.size(); ++i)
    {
        size_t next = (i + 1) % hullPoints.size();
        DrawLineEx (hullPoints[i], hullPoints[next], 1.5f, outlineColor);
    }
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
                    pixelColor = Config::colorWaterHighlight1;
                }
                else if (r < 20)
                {
                    // Medium highlight (12%)
                    pixelColor = Config::colorWaterHighlight2;
                }
                else if (r < 35)
                {
                    // Subtle highlight (15%)
                    pixelColor = Config::colorWaterHighlight3;
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
