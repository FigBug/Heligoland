#include "Renderer.h"
#include "Ship.h"
#include <cmath>
#include <vector>

Renderer::Renderer(SDL_Renderer* renderer_)
    : renderer(renderer_) {
}

void Renderer::clear() {
    SDL_SetRenderDrawColor(renderer, 30, 60, 90, 255);  // Dark ocean blue
    SDL_RenderClear(renderer);
}

void Renderer::present() {
    SDL_RenderPresent(renderer);
}

void Renderer::drawShip(const Ship& ship) {
    Vec2 pos = ship.getPosition();
    float angle = ship.getAngle();
    SDL_Color color = ship.getColor();

    // Draw ship hull (oval)
    drawFilledOval(pos, ship.getWidth(), ship.getHeight(), angle, color);

    // Draw turrets
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    for (const auto& turret : ship.getTurrets()) {
        Vec2 localOffset = turret.getLocalOffset();

        // Rotate local offset by ship angle
        Vec2 worldOffset;
        worldOffset.x = localOffset.x * cosA - localOffset.y * sinA;
        worldOffset.y = localOffset.x * sinA + localOffset.y * cosA;

        Vec2 turretPos = pos + worldOffset;

        // Draw turret base (darker circle)
        SDL_Color turretColor = {
            (Uint8)(color.r * 0.6f),
            (Uint8)(color.g * 0.6f),
            (Uint8)(color.b * 0.6f),
            255
        };
        drawFilledCircle(turretPos, turret.getRadius(), turretColor);

        // Draw barrel (line from turret center outward)
        float turretAngle = turret.getAngle();
        Vec2 barrelEnd = turretPos + Vec2::fromAngle(turretAngle) * turret.getBarrelLength();
        SDL_Color barrelColor = {50, 50, 50, 255};
        drawLine(turretPos, barrelEnd, barrelColor);
    }
}

void Renderer::drawCrosshair(Vec2 position, SDL_Color color) {
    float size = 15.0f;

    // Draw crosshair as two perpendicular lines
    drawLine({position.x - size, position.y}, {position.x + size, position.y}, color);
    drawLine({position.x, position.y - size}, {position.x, position.y + size}, color);

    // Draw small circle in center
    drawCircle(position, 5.0f, color);
}

void Renderer::drawOval(Vec2 center, float width, float height, float angle, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    int segments = 32;
    for (int i = 0; i < segments; ++i) {
        float theta1 = (2.0f * M_PI * i) / segments;
        float theta2 = (2.0f * M_PI * (i + 1)) / segments;

        // Points on unrotated ellipse
        float x1 = (width / 2.0f) * std::cos(theta1);
        float y1 = (height / 2.0f) * std::sin(theta1);
        float x2 = (width / 2.0f) * std::cos(theta2);
        float y2 = (height / 2.0f) * std::sin(theta2);

        // Rotate points
        float rx1 = x1 * cosA - y1 * sinA;
        float ry1 = x1 * sinA + y1 * cosA;
        float rx2 = x2 * cosA - y2 * sinA;
        float ry2 = x2 * sinA + y2 * cosA;

        SDL_RenderLine(renderer,
                       center.x + rx1, center.y + ry1,
                       center.x + rx2, center.y + ry2);
    }
}

void Renderer::drawFilledOval(Vec2 center, float width, float height, float angle, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    // Draw filled oval using horizontal scanlines
    int numLines = (int)(std::max(width, height) / 2.0f);

    for (int i = -numLines; i <= numLines; ++i) {
        float t = (float)i / numLines;
        float localY = t * (height / 2.0f);

        // Width at this Y position (ellipse equation)
        float localHalfWidth = (width / 2.0f) * std::sqrt(1.0f - t * t);

        if (localHalfWidth > 0.0f) {
            // Two endpoints of the scanline in local coords
            float x1 = -localHalfWidth;
            float x2 = localHalfWidth;

            // Rotate to world coords
            float wx1 = x1 * cosA - localY * sinA + center.x;
            float wy1 = x1 * sinA + localY * cosA + center.y;
            float wx2 = x2 * cosA - localY * sinA + center.x;
            float wy2 = x2 * sinA + localY * cosA + center.y;

            SDL_RenderLine(renderer, wx1, wy1, wx2, wy2);
        }
    }

    // Draw outline for better visibility
    SDL_Color outlineColor = {
        (Uint8)(color.r * 0.5f),
        (Uint8)(color.g * 0.5f),
        (Uint8)(color.b * 0.5f),
        255
    };
    drawOval(center, width, height, angle, outlineColor);
}

void Renderer::drawCircle(Vec2 center, float radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int segments = 24;
    for (int i = 0; i < segments; ++i) {
        float theta1 = (2.0f * M_PI * i) / segments;
        float theta2 = (2.0f * M_PI * (i + 1)) / segments;

        float x1 = center.x + radius * std::cos(theta1);
        float y1 = center.y + radius * std::sin(theta1);
        float x2 = center.x + radius * std::cos(theta2);
        float y2 = center.y + radius * std::sin(theta2);

        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

void Renderer::drawFilledCircle(Vec2 center, float radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    // Draw filled circle using horizontal scanlines
    for (int y = (int)-radius; y <= (int)radius; ++y) {
        float halfWidth = std::sqrt(radius * radius - y * y);
        SDL_RenderLine(renderer,
                       center.x - halfWidth, center.y + y,
                       center.x + halfWidth, center.y + y);
    }
}

void Renderer::drawLine(Vec2 start, Vec2 end, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer, start.x, start.y, end.x, end.y);
}
