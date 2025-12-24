#include "Renderer.h"
#include "Game.h"
#include "Shell.h"
#include "Ship.h"
#include <cmath>
#include <vector>

Renderer::Renderer (SDL_Renderer* renderer_)
    : renderer (renderer_)
{
}

void Renderer::clear()
{
    SDL_SetRenderDrawColor (renderer, 30, 60, 90, 255); // Dark ocean blue
    SDL_RenderClear (renderer);
}

void Renderer::present()
{
    SDL_RenderPresent (renderer);
}

void Renderer::drawShip (const Ship& ship)
{
    Vec2 pos = ship.getPosition();
    float angle = ship.getAngle();
    SDL_Color color = ship.getColor();

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
        SDL_Color turretColor = {
            (Uint8) (color.r * 0.6f),
            (Uint8) (color.g * 0.6f),
            (Uint8) (color.b * 0.6f),
            255
        };
        drawFilledCircle (turretPos, turret.getRadius(), turretColor);

        // Draw barrel (line from turret center outward)
        float turretAngle = turret.getWorldAngle (angle);
        Vec2 barrelEnd = turretPos + Vec2::fromAngle (turretAngle) * turret.getBarrelLength();
        SDL_Color barrelColor = { 50, 50, 50, 255 };
        drawLine (turretPos, barrelEnd, barrelColor);
    }
}

void Renderer::drawBubbleTrail (const Ship& ship)
{
    for (const auto& bubble : ship.getBubbles())
    {
        Uint8 alpha = (Uint8) (bubble.alpha * 128); // Start at 50% opacity, fade to 0
        SDL_Color color = { 255, 255, 255, alpha };
        drawFilledCircle (bubble.position, bubble.radius, color);
    }
}

void Renderer::drawSmoke (const Ship& ship)
{
    for (const auto& s : ship.getSmoke())
    {
        Uint8 alpha = (Uint8) (s.alpha * 180); // Start at 70% opacity, fade to 0
        SDL_Color color = { 40, 40, 40, alpha }; // Dark grey/black smoke
        drawFilledCircle (s.position, s.radius, color);
    }
}

void Renderer::drawShell (const Shell& shell)
{
    SDL_Color color = { 255, 200, 50, 255 }; // Orange/yellow shell
    drawFilledCircle (shell.getPosition(), shell.getRadius(), color);
}

void Renderer::drawExplosion (const Explosion& explosion)
{
    float progress = explosion.getProgress();

    // Explosion expands quickly then fades
    float radius = explosion.maxRadius * std::sqrt (progress); // Fast initial expansion
    float alpha = 1.0f - progress; // Fade out

    if (explosion.isHit)
    {
        // Hit explosion - orange/yellow
        SDL_Color outerColor = { 255, 150, 50, (Uint8) (alpha * 200) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            SDL_Color midColor = { 255, 220, 100, (Uint8) (alpha * 180) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            SDL_Color coreColor = { 255, 255, 200, (Uint8) (alpha * 150) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
    else
    {
        // Miss splash - blue/white
        SDL_Color outerColor = { 100, 150, 255, (Uint8) (alpha * 200) };
        drawCircle (explosion.position, radius, outerColor);

        if (radius > 5.0f)
        {
            SDL_Color midColor = { 150, 200, 255, (Uint8) (alpha * 180) };
            drawCircle (explosion.position, radius * 0.7f, midColor);
        }

        if (radius > 10.0f)
        {
            SDL_Color coreColor = { 220, 240, 255, (Uint8) (alpha * 150) };
            drawFilledCircle (explosion.position, radius * 0.3f, coreColor);
        }
    }
}

void Renderer::drawCrosshair (const Ship& ship)
{
    Vec2 position = ship.getCrosshairPosition();
    SDL_Color shipColor = ship.getColor();

    // Crosshair is grey if not ready to fire
    SDL_Color crosshairColor = ship.isReadyToFire() ? shipColor : SDL_Color { 100, 100, 100, 255 };

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
    SDL_Color barBg = { 60, 60, 60, 255 };
    drawFilledRect ({ position.x - barWidth / 2.0f, barY }, barWidth, barHeight, barBg);

    float reloadPct = ship.getReloadProgress();
    SDL_Color reloadColor = reloadPct >= 1.0f ? SDL_Color { 100, 255, 100, 255 } : SDL_Color { 255, 100, 100, 255 };
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
        SDL_Color circleColor = turrets[i].isAimedAtTarget() ? shipColor : SDL_Color { 60, 60, 60, 255 };
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

    Uint8 a = (Uint8) (alpha * 255);

    SDL_Color shipColor = { ship.getColor().r, ship.getColor().g, ship.getColor().b, a };
    SDL_Color bgColor = { 30, 30, 30, (Uint8) (alpha * 200) };
    SDL_Color barBg = { 60, 60, 60, a };
    SDL_Color white = { 255, 255, 255, a };

    // Background
    drawFilledRect ({ x, y }, hudWidth, hudHeight, bgColor);
    drawRect ({ x, y }, hudWidth, hudHeight, shipColor);

    // Player label
    std::string label = "P" + std::to_string (ship.getPlayerIndex() + 1);
    drawText (label, { x + 5, y + 5 }, 2.0f, shipColor);

    float barX = x + 35;
    float barWidth = hudWidth - 45;
    float barHeight = 8.0f;

    // Health bar
    float healthY = y + 6;
    drawFilledRect ({ barX, healthY }, barWidth, barHeight, barBg);
    float healthPct = ship.getHealth() / ship.getMaxHealth();
    SDL_Color healthColor = { (Uint8) (255 * (1 - healthPct)), (Uint8) (255 * healthPct), 0, a };
    drawFilledRect ({ barX, healthY }, barWidth * healthPct, barHeight, healthColor);

    // Throttle bar (centered, negative goes left, positive goes right)
    float throttleY = y + 17;
    drawFilledRect ({ barX, throttleY }, barWidth, barHeight, barBg);
    float throttle = ship.getThrottle();
    float throttleCenter = barX + barWidth / 2.0f;
    SDL_Color throttleColor = { 100, 150, 255, a };
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
    float rudderY = y + 28;
    drawFilledRect ({ barX, rudderY }, barWidth, barHeight, barBg);
    float rudder = ship.getRudder();
    float rudderCenter = barX + barWidth / 2.0f;
    SDL_Color rudderColor = { 255, 200, 100, a };
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
    SDL_Color bgColor = { 30, 30, 30, 200 };
    drawFilledCircle (center, indicatorSize, bgColor);
    drawCircle (center, indicatorSize, { 100, 100, 100, 255 });

    // Wind arrow
    float windStrength = wind.length();
    if (windStrength > 0.01f)
    {
        Vec2 windDir = wind.normalized();
        float arrowLength = indicatorSize * 0.8f * windStrength;

        Vec2 arrowEnd = center + windDir * arrowLength;

        // Arrow shaft
        SDL_Color arrowColor = { 200, 200, 255, 255 };
        drawLine (center, arrowEnd, arrowColor);

        // Arrow head
        float headSize = 4.0f;
        float headAngle = 0.5f;
        Vec2 head1 = arrowEnd - windDir * headSize + Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        Vec2 head2 = arrowEnd - windDir * headSize - Vec2::fromAngle (windDir.toAngle() + pi * 0.5f) * headSize * 0.5f;
        drawLine (arrowEnd, head1, arrowColor);
        drawLine (arrowEnd, head2, arrowColor);
    }

    // Label
    drawText ("WIND", { center.x - 6, center.y + indicatorSize + 3 }, 0.75f, { 150, 150, 150, 255 });
}

void Renderer::drawOval (Vec2 center, float width, float height, float angle, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

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

        SDL_RenderLine (renderer,
                        center.x + rx1,
                        center.y + ry1,
                        center.x + rx2,
                        center.y + ry2);
    }
}

void Renderer::drawFilledOval (Vec2 center, float width, float height, float angle, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

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

            SDL_RenderLine (renderer, wx1, wy1, wx2, wy2);
        }
    }

    // Draw outline for better visibility
    SDL_Color outlineColor = {
        (Uint8) (color.r * 0.5f),
        (Uint8) (color.g * 0.5f),
        (Uint8) (color.b * 0.5f),
        255
    };
    drawOval (center, width, height, angle, outlineColor);
}

void Renderer::drawShipHull (Vec2 center, float length, float width, float angle, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    float halfLength = length / 2.0f;
    float halfWidth = width / 2.0f;

    // Ship shape: pointy bow, curved sides, squarer stern
    // Draw using horizontal scanlines from stern to bow

    int numLines = (int) length;

    for (int i = 0; i <= numLines; ++i)
    {
        // t goes from -1 (stern) to 1 (bow)
        float t = (float) i / numLines * 2.0f - 1.0f;
        float localX = t * halfLength;

        float localHalfWidth;

        if (t > 0.7f)
        {
            // Bow section - taper to a point
            float bowT = (t - 0.7f) / 0.3f; // 0 to 1 in bow section
            localHalfWidth = halfWidth * (1.0f - bowT * bowT); // Quadratic taper to point
        }
        else if (t < -0.6f)
        {
            // Stern section - slightly rounded but more square
            float sternT = (-t - 0.6f) / 0.4f; // 0 to 1 in stern section
            localHalfWidth = halfWidth * (1.0f - sternT * sternT * 0.3f); // Gentle curve
        }
        else
        {
            // Main body - full width with slight curve
            float bodyT = (t + 0.6f) / 1.3f; // Normalized position in body
            // Slight bulge in the middle
            localHalfWidth = halfWidth * (1.0f + 0.05f * std::sin (bodyT * pi));
        }

        if (localHalfWidth > 0.5f)
        {
            // Two endpoints of the scanline in local coords
            float y1 = -localHalfWidth;
            float y2 = localHalfWidth;

            // Rotate to world coords
            float wx1 = localX * cosA - y1 * sinA + center.x;
            float wy1 = localX * sinA + y1 * cosA + center.y;
            float wx2 = localX * cosA - y2 * sinA + center.x;
            float wy2 = localX * sinA + y2 * cosA + center.y;

            SDL_RenderLine (renderer, wx1, wy1, wx2, wy2);
        }
    }

    // Draw outline for better visibility
    SDL_Color outlineColor = {
        (Uint8) (color.r * 0.5f),
        (Uint8) (color.g * 0.5f),
        (Uint8) (color.b * 0.5f),
        255
    };
    SDL_SetRenderDrawColor (renderer, outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a);

    // Draw outline by tracing the hull shape
    std::vector<Vec2> hullPoints;
    int outlineSegments = 32;

    for (int i = 0; i <= outlineSegments; ++i)
    {
        float t = (float) i / outlineSegments * 2.0f - 1.0f;
        float localX = t * halfLength;

        float localHalfWidth;

        if (t > 0.7f)
        {
            float bowT = (t - 0.7f) / 0.3f;
            localHalfWidth = halfWidth * (1.0f - bowT * bowT);
        }
        else if (t < -0.6f)
        {
            float sternT = (-t - 0.6f) / 0.4f;
            localHalfWidth = halfWidth * (1.0f - sternT * sternT * 0.3f);
        }
        else
        {
            float bodyT = (t + 0.6f) / 1.3f;
            localHalfWidth = halfWidth * (1.0f + 0.05f * std::sin (bodyT * pi));
        }

        // Top edge (positive Y in local)
        float wx = localX * cosA - localHalfWidth * sinA + center.x;
        float wy = localX * sinA + localHalfWidth * cosA + center.y;
        hullPoints.push_back ({ wx, wy });
    }

    // Add bottom edge in reverse
    for (int i = outlineSegments; i >= 0; --i)
    {
        float t = (float) i / outlineSegments * 2.0f - 1.0f;
        float localX = t * halfLength;

        float localHalfWidth;

        if (t > 0.7f)
        {
            float bowT = (t - 0.7f) / 0.3f;
            localHalfWidth = halfWidth * (1.0f - bowT * bowT);
        }
        else if (t < -0.6f)
        {
            float sternT = (-t - 0.6f) / 0.4f;
            localHalfWidth = halfWidth * (1.0f - sternT * sternT * 0.3f);
        }
        else
        {
            float bodyT = (t + 0.6f) / 1.3f;
            localHalfWidth = halfWidth * (1.0f + 0.05f * std::sin (bodyT * pi));
        }

        // Bottom edge (negative Y in local)
        float wx = localX * cosA - (-localHalfWidth) * sinA + center.x;
        float wy = localX * sinA + (-localHalfWidth) * cosA + center.y;
        hullPoints.push_back ({ wx, wy });
    }

    // Draw outline
    for (size_t i = 0; i < hullPoints.size(); ++i)
    {
        size_t next = (i + 1) % hullPoints.size();
        SDL_RenderLine (renderer, hullPoints[i].x, hullPoints[i].y,
                        hullPoints[next].x, hullPoints[next].y);
    }
}

void Renderer::drawCircle (Vec2 center, float radius, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    int segments = 24;
    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (2.0f * pi * i) / segments;
        float theta2 = (2.0f * pi * (i + 1)) / segments;

        float x1 = center.x + radius * std::cos (theta1);
        float y1 = center.y + radius * std::sin (theta1);
        float x2 = center.x + radius * std::cos (theta2);
        float y2 = center.y + radius * std::sin (theta2);

        SDL_RenderLine (renderer, x1, y1, x2, y2);
    }
}

void Renderer::drawFilledCircle (Vec2 center, float radius, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    // Draw filled circle using horizontal scanlines
    for (int y = (int) -radius; y <= (int) radius; ++y)
    {
        float halfWidth = std::sqrt (radius * radius - y * y);
        SDL_RenderLine (renderer,
                        center.x - halfWidth,
                        center.y + y,
                        center.x + halfWidth,
                        center.y + y);
    }
}

void Renderer::drawLine (Vec2 start, Vec2 end, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine (renderer, start.x, start.y, end.x, end.y);
}

void Renderer::drawRect (Vec2 topLeft, float width, float height, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { topLeft.x, topLeft.y, width, height };
    SDL_RenderRect (renderer, &rect);
}

void Renderer::drawFilledRect (Vec2 topLeft, float width, float height, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { topLeft.x, topLeft.y, width, height };
    SDL_RenderFillRect (renderer, &rect);
}

// Simple 5x7 bitmap font patterns (each char is 5 wide, 7 tall)
// Returns glyph data for a character, or empty glyph for unsupported chars
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

void Renderer::drawChar (char c, Vec2 position, float scale, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    unsigned char uc = (unsigned char) c;
    const uint8_t* glyph = getGlyph (uc);

    for (int row = 0; row < 7; ++row)
    {
        uint8_t rowBits = glyph[row];
        for (int col = 0; col < 5; ++col)
        {
            if (rowBits & (1 << (4 - col)))
            {
                SDL_FRect pixel = {
                    position.x + col * scale,
                    position.y + row * scale,
                    scale,
                    scale
                };
                SDL_RenderFillRect (renderer, &pixel);
            }
        }
    }
}

void Renderer::drawText (const std::string& text, Vec2 position, float scale, SDL_Color color)
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

void Renderer::drawTextCentered (const std::string& text, Vec2 center, float scale, SDL_Color color)
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
