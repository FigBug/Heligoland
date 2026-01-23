#pragma once

#include "Vec2.h"
#include <vector>

class Island
{
public:
    Island (Vec2 center, float baseRadius, unsigned int seed);

    Vec2 getCenter() const { return center; }
    float getBoundingRadius() const { return boundingRadius; }
    const std::vector<Vec2>& getVertices() const { return vertices; }

    bool containsPoint (Vec2 point) const;
    bool getCollisionResponse (Vec2 point, Vec2& pushDirection, float& pushDistance) const;

private:
    Vec2 center;
    float boundingRadius = 0.0f;
    std::vector<Vec2> vertices;

    void generateShape (float baseRadius, unsigned int seed);
};
