#pragma once

#include "Vec2.h"
#include <raylib.h>
#include <string>

class Ship;
class Shell;
class Island;
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
    void drawCurrentIndicator (Vec2 current, float screenWidth, float screenHeight);
    void drawIsland (const Island& island);

    void drawOval (Vec2 center, float width, float height, float angle, Color color);
    void drawCircle (Vec2 center, float radius, Color color);
    void drawLine (Vec2 start, Vec2 end, Color color);
    void drawRect (Vec2 topLeft, float width, float height, Color color);
    void drawFilledRect (Vec2 topLeft, float width, float height, Color color);

    // Simple bitmap text rendering (blocky letters)
    void drawText (const std::string& text, Vec2 position, float scale, Color color);
    void drawTextCentered (const std::string& text, Vec2 center, float scale, Color color);

    // Ship dimensions from loaded textures
    float getShipLength (int shipType = 3) const;  // Default to battleship (type 4, index 3)
    float getShipWidth (int shipType = 3) const;

    // Draw ship selection preview
    void drawShipPreview (int shipType, Vec2 position, float angle, int playerIndex = 0);

    // Pixel-perfect hit testing
    bool checkShipHit (const Ship& ship, Vec2 worldPos) const;
    bool checkShipCollision (const Ship& shipA, const Ship& shipB, Vec2& collisionPoint) const;

private:
    void createNoiseTexture();
    void loadShipTextures();
    void drawFilledOval (Vec2 center, float width, float height, float angle, Color color);
    void drawFilledCircle (Vec2 center, float radius, Color color);
    void drawChar (char c, Vec2 position, float scale, Color color);
    int getShipColorIndex (const Ship& ship) const;  // Returns color index (0-3) based on team/player

    Texture2D noiseTexture1 = { 0 };
    Texture2D noiseTexture2 = { 0 };
    static constexpr int noiseTextureSize = 128;

    // Ship hull textures by type (0=1turret, 1=2turret, 2=3turret, 3=4turret)
    static constexpr int NUM_SHIP_TYPES = 4;
    Texture2D shipHullTextures[NUM_SHIP_TYPES] = {};
    Texture2D shipTurretTextures[NUM_SHIP_TYPES] = {};
    Image shipHullImages[NUM_SHIP_TYPES] = {};  // Keep images for pixel-perfect hit testing
    bool shipTexturesLoaded = false;
};
