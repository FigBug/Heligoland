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
    void drawShipHUD (const Ship& ship, int slot, int totalSlots, float screenWidth, float hudWidth, float alpha = 1.0f);
    void drawWindIndicator (Vec2 wind, float screenWidth, float screenHeight);

    void drawOval (Vec2 center, float width, float height, float angle, Color color);
    void drawCircle (Vec2 center, float radius, Color color);
    void drawLine (Vec2 start, Vec2 end, Color color);
    void drawRect (Vec2 topLeft, float width, float height, Color color);
    void drawFilledRect (Vec2 topLeft, float width, float height, Color color);

    // Simple bitmap text rendering (blocky letters)
    void drawText (const std::string& text, Vec2 position, float scale, Color color);
    void drawTextCentered (const std::string& text, Vec2 center, float scale, Color color);

    // Ship dimensions from loaded textures
    float getShipLength() const;
    float getShipWidth() const;

    // Pixel-perfect hit testing
    bool checkShipHit (const Ship& ship, Vec2 worldPos) const;
    bool checkShipCollision (const Ship& shipA, const Ship& shipB, Vec2& collisionPoint) const;

private:
    void createNoiseTexture();
    void loadShipTextures();
    void drawFilledOval (Vec2 center, float width, float height, float angle, Color color);
    void drawFilledCircle (Vec2 center, float radius, Color color);
    void drawChar (char c, Vec2 position, float scale, Color color);
    int getShipTextureIndex (const Ship& ship) const;

    Texture2D noiseTexture1 = { 0 };
    Texture2D noiseTexture2 = { 0 };
    static constexpr int noiseTextureSize = 128;

    // Ship textures: 0=Blue, 1=Red, 2=Green, 3=Yellow
    Texture2D shipHullTextures[4] = {};
    Texture2D shipTurretTextures[4] = {};
    Image shipHullImages[4] = {};  // Keep images for pixel-perfect hit testing
    bool shipTexturesLoaded = false;
};
