#pragma once

#include "Vec2.h"
#include <SDL3/SDL.h>
#include <string>

class Ship;
class Shell;

class Renderer
{
public:
    Renderer (SDL_Renderer* renderer);

    void clear();
    void present();

    void drawShip (const Ship& ship);
    void drawBubbleTrail (const Ship& ship);
    void drawShell (const Shell& shell);
    void drawCrosshair (Vec2 position, SDL_Color color);
    void drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth);
    void drawWindIndicator (Vec2 wind, float screenWidth, float screenHeight);

    void drawOval (Vec2 center, float width, float height, float angle, SDL_Color color);
    void drawCircle (Vec2 center, float radius, SDL_Color color);
    void drawLine (Vec2 start, Vec2 end, SDL_Color color);
    void drawRect (Vec2 topLeft, float width, float height, SDL_Color color);
    void drawFilledRect (Vec2 topLeft, float width, float height, SDL_Color color);

    // Simple bitmap text rendering (blocky letters)
    void drawText (const std::string& text, Vec2 position, float scale, SDL_Color color);
    void drawTextCentered (const std::string& text, Vec2 center, float scale, SDL_Color color);

private:
    SDL_Renderer* renderer;

    void drawFilledOval (Vec2 center, float width, float height, float angle, SDL_Color color);
    void drawFilledCircle (Vec2 center, float radius, SDL_Color color);
    void drawChar (char c, Vec2 position, float scale, SDL_Color color);
};
