#pragma once

#include "Vec2.h"
#include <SDL3/SDL.h>

class Ship;

class Renderer {
public:
    Renderer(SDL_Renderer* renderer);

    void clear();
    void present();

    void drawShip(const Ship& ship);
    void drawCrosshair(Vec2 position, SDL_Color color);

    void drawOval(Vec2 center, float width, float height, float angle, SDL_Color color);
    void drawCircle(Vec2 center, float radius, SDL_Color color);
    void drawLine(Vec2 start, Vec2 end, SDL_Color color);

private:
    SDL_Renderer* renderer;

    void drawFilledOval(Vec2 center, float width, float height, float angle, SDL_Color color);
    void drawFilledCircle(Vec2 center, float radius, SDL_Color color);
};
