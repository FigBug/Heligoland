#include "Renderer.h"
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

    // Draw ship hull (oval) - length is along the ship's facing direction
    drawFilledOval (pos, ship.getLength(), ship.getWidth(), angle, color);

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
        Uint8 alpha = (Uint8) (bubble.alpha * 64); // 0.25 * 255 = ~64
        SDL_Color color = { 255, 255, 255, alpha };
        drawFilledCircle (bubble.position, bubble.radius, color);
    }
}

void Renderer::drawShell (const Shell& shell)
{
    SDL_Color color = { 255, 200, 50, 255 }; // Orange/yellow shell
    drawFilledCircle (shell.getPosition(), shell.getRadius(), color);
}

void Renderer::drawCrosshair (Vec2 position, SDL_Color color)
{
    float size = 15.0f;

    // Draw crosshair as two perpendicular lines
    drawLine ({ position.x - size, position.y }, { position.x + size, position.y }, color);
    drawLine ({ position.x, position.y - size }, { position.x, position.y + size }, color);

    // Draw small circle in center
    drawCircle (position, 5.0f, color);
}

void Renderer::drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth)
{
    float hudWidth = 200.0f;
    float hudHeight = 60.0f;
    float spacing = 20.0f;
    float totalWidth = totalSlots * hudWidth + (totalSlots - 1) * spacing;
    float startX = (screenWidth - totalWidth) / 2.0f;
    float x = startX + slot * (hudWidth + spacing);
    float y = 10.0f;

    SDL_Color shipColor = ship.getColor();
    SDL_Color bgColor = { 30, 30, 30, 200 };
    SDL_Color barBg = { 60, 60, 60, 255 };

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
    SDL_Color healthColor = { (Uint8) (255 * (1 - healthPct)), (Uint8) (255 * healthPct), 0, 255 };
    drawFilledRect ({ barX, healthY }, barWidth * healthPct, barHeight, healthColor);

    // Reload bar
    float reloadY = y + 17;
    drawFilledRect ({ barX, reloadY }, barWidth, barHeight, barBg);
    float reloadPct = ship.getReloadProgress();
    SDL_Color reloadColor = reloadPct >= 1.0f ? SDL_Color { 100, 255, 100, 255 } : SDL_Color { 255, 100, 100, 255 };
    drawFilledRect ({ barX, reloadY }, barWidth * reloadPct, barHeight, reloadColor);

    // Throttle bar (centered, negative goes left, positive goes right)
    float throttleY = y + 28;
    drawFilledRect ({ barX, throttleY }, barWidth, barHeight, barBg);
    float throttle = ship.getThrottle();
    float throttleCenter = barX + barWidth / 2.0f;
    SDL_Color throttleColor = { 100, 150, 255, 255 };
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
    drawLine ({ throttleCenter, throttleY }, { throttleCenter, throttleY + barHeight }, { 255, 255, 255, 255 });

    // Rudder bar (centered, negative goes left, positive goes right)
    float rudderY = y + 39;
    drawFilledRect ({ barX, rudderY }, barWidth, barHeight, barBg);
    float rudder = ship.getRudder();
    float rudderCenter = barX + barWidth / 2.0f;
    SDL_Color rudderColor = { 255, 200, 100, 255 };
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
    drawLine ({ rudderCenter, rudderY }, { rudderCenter, rudderY + barHeight }, { 255, 255, 255, 255 });
}

void Renderer::drawWindIndicator (Vec2 wind, float screenWidth, float screenHeight)
{
    // Draw wind indicator in bottom-right corner
    float indicatorSize = 40.0f;
    Vec2 center = { screenWidth - 60, screenHeight - 60 };

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
        float headSize = 8.0f;
        float headAngle = 0.5f;
        Vec2 head1 = arrowEnd - windDir * headSize + Vec2::fromAngle (windDir.toAngle() + M_PI * 0.5f) * headSize * 0.5f;
        Vec2 head2 = arrowEnd - windDir * headSize - Vec2::fromAngle (windDir.toAngle() + M_PI * 0.5f) * headSize * 0.5f;
        drawLine (arrowEnd, head1, arrowColor);
        drawLine (arrowEnd, head2, arrowColor);
    }

    // Label
    drawText ("WIND", { center.x - 12, center.y + indicatorSize + 5 }, 1.5f, { 150, 150, 150, 255 });
}

void Renderer::drawOval (Vec2 center, float width, float height, float angle, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    float cosA = std::cos (angle);
    float sinA = std::sin (angle);

    int segments = 32;
    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (2.0f * M_PI * i) / segments;
        float theta2 = (2.0f * M_PI * (i + 1)) / segments;

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

void Renderer::drawCircle (Vec2 center, float radius, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    int segments = 24;
    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (2.0f * M_PI * i) / segments;
        float theta2 = (2.0f * M_PI * (i + 1)) / segments;

        float x1 = center.x + radius * std::cos (theta1);
        float y1 = center.y + radius * std::sin (theta1);
        float x2 = center.x + radius * std::cos (theta2);
        float y2 = center.y + radius * std::sin (theta2);

        SDL_RenderLine (renderer, x1, y1, x2, y2);
    }
}

void Renderer::drawFilledCircle (Vec2 center, float radius, SDL_Color color)
{
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
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine (renderer, start.x, start.y, end.x, end.y);
}

void Renderer::drawRect (Vec2 topLeft, float width, float height, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { topLeft.x, topLeft.y, width, height };
    SDL_RenderRect (renderer, &rect);
}

void Renderer::drawFilledRect (Vec2 topLeft, float width, float height, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { topLeft.x, topLeft.y, width, height };
    SDL_RenderFillRect (renderer, &rect);
}

// Simple 5x7 bitmap font patterns (each char is 5 wide, 7 tall)
static const uint8_t font[128][7] = {
    // Non-printable characters (0-31) are empty
    [0 ... 31] = { 0, 0, 0, 0, 0, 0, 0 },
    // Space (32)
    [' '] = { 0, 0, 0, 0, 0, 0, 0 },
    // Numbers 0-9
    ['0'] = { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 },
    ['1'] = { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },
    ['2'] = { 0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111 },
    ['3'] = { 0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110 },
    ['4'] = { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 },
    ['5'] = { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 },
    ['6'] = { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 },
    ['7'] = { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 },
    ['8'] = { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 },
    ['9'] = { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 },
    // Letters A-Z
    ['A'] = { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 },
    ['B'] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 },
    ['C'] = { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 },
    ['D'] = { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 },
    ['E'] = { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 },
    ['F'] = { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 },
    ['G'] = { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110 },
    ['H'] = { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 },
    ['I'] = { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },
    ['J'] = { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 },
    ['K'] = { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 },
    ['L'] = { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 },
    ['M'] = { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 },
    ['N'] = { 0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001 },
    ['O'] = { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 },
    ['P'] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 },
    ['Q'] = { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 },
    ['R'] = { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 },
    ['S'] = { 0b01110, 0b10001, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110 },
    ['T'] = { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 },
    ['U'] = { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 },
    ['V'] = { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 },
    ['W'] = { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 },
    ['X'] = { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 },
    ['Y'] = { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 },
    ['Z'] = { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 },
    // Some punctuation
    ['!'] = { 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100 },
    [':'] = { 0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000 },
    ['-'] = { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 },
};

void Renderer::drawChar (char c, Vec2 position, float scale, SDL_Color color)
{
    SDL_SetRenderDrawColor (renderer, color.r, color.g, color.b, color.a);

    unsigned char uc = (unsigned char) c;
    if (uc >= 128)
        return;

    for (int row = 0; row < 7; ++row)
    {
        uint8_t rowBits = font[uc][row];
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
