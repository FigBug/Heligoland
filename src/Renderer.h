#pragma once

#include "Vec2.h"
#include <raylib.h>
#include <string>

class Ship;
class Shell;
struct Explosion;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void clear();
    void drawWater (float time, float screenWidth, float screenHeight);
    void present();

    void drawShip (const Ship& ship);
    void drawBubbleTrail (const Ship& ship);
    void drawSmoke (const Ship& ship);
    void drawShell (const Shell& shell);
    void drawExplosion (const Explosion& explosion);
    void drawCrosshair (const Ship& ship);
    void drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth, float alpha = 1.0f);
    void drawWindIndicator (Vec2 wind, float screenWidth, float screenHeight);

    void drawOval (Vec2 center, float width, float height, float angle, Color color);
    void drawCircle (Vec2 center, float radius, Color color);
    void drawLine (Vec2 start, Vec2 end, Color color);
    void drawRect (Vec2 topLeft, float width, float height, Color color);
    void drawFilledRect (Vec2 topLeft, float width, float height, Color color);

    // Simple bitmap text rendering (blocky letters)
    void drawText (const std::string& text, Vec2 position, float scale, Color color);
    void drawTextCentered (const std::string& text, Vec2 center, float scale, Color color);

private:
    void createNoiseTexture();
    void drawFilledOval (Vec2 center, float width, float height, float angle, Color color);
    void drawFilledCircle (Vec2 center, float radius, Color color);
    void drawShipHull (Vec2 center, float length, float width, float angle, Color color);
    void drawChar (char c, Vec2 position, float scale, Color color);

    Texture2D noiseTexture1 = { 0 };
    Texture2D noiseTexture2 = { 0 };
    static constexpr int noiseTextureSize = 128;
};
