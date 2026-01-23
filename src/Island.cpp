#include "Island.h"
#include <algorithm>
#include <cmath>

Island::Island (Vec2 center_, float baseRadius, unsigned int seed)
    : center (center_)
{
    generateShape (baseRadius, seed);
}

void Island::generateShape (float baseRadius, unsigned int seed)
{
    // LCG random number generator for reproducibility
    auto nextRandom = [&seed]() -> float {
        seed = seed * 1103515245 + 12345;
        return ((seed >> 16) & 0x7FFF) / 32767.0f;
    };

    int numVertices = 16 + (int) (nextRandom() * 8); // 16-24 vertices
    float angleStep = 2.0f * (float) pi / numVertices;

    // Generate variation parameters for organic shape
    float freq1 = 2.0f + nextRandom() * 2.0f;
    float freq2 = 4.0f + nextRandom() * 3.0f;
    float phase1 = nextRandom() * 2.0f * (float) pi;
    float phase2 = nextRandom() * 2.0f * (float) pi;
    float amp1 = 0.2f + nextRandom() * 0.15f;
    float amp2 = 0.1f + nextRandom() * 0.1f;

    boundingRadius = 0.0f;
    vertices.clear();
    vertices.reserve (numVertices);

    for (int i = 0; i < numVertices; ++i)
    {
        float angle = i * angleStep;

        // Combine sine waves for organic variation
        float variation = 1.0f
                          + amp1 * std::sin (freq1 * angle + phase1)
                          + amp2 * std::sin (freq2 * angle + phase2);

        // Add small random perturbation
        variation += (nextRandom() - 0.5f) * 0.1f;
        variation = std::clamp (variation, 0.6f, 1.4f);

        float radius = baseRadius * variation;
        Vec2 vertex = center + Vec2::fromAngle (angle) * radius;
        vertices.push_back (vertex);

        boundingRadius = std::max (boundingRadius, radius);
    }

    boundingRadius *= 1.05f;
}

bool Island::containsPoint (Vec2 point) const
{
    // Ray casting algorithm for point-in-polygon test
    int crossings = 0;
    int n = (int) vertices.size();

    for (int i = 0; i < n; ++i)
    {
        Vec2 v1 = vertices[i];
        Vec2 v2 = vertices[(i + 1) % n];

        if ((v1.y <= point.y && v2.y > point.y) || (v2.y <= point.y && v1.y > point.y))
        {
            float xIntersect = v1.x + (point.y - v1.y) / (v2.y - v1.y) * (v2.x - v1.x);
            if (point.x < xIntersect)
                crossings++;
        }
    }

    return (crossings % 2) == 1;
}

bool Island::getCollisionResponse (Vec2 point, Vec2& pushDirection, float& pushDistance) const
{
    if (!containsPoint (point))
        return false;

    // Find closest edge to push out toward
    float minDist = 999999.0f;
    Vec2 bestPushDir;

    int n = (int) vertices.size();
    for (int i = 0; i < n; ++i)
    {
        Vec2 v1 = vertices[i];
        Vec2 v2 = vertices[(i + 1) % n];

        // Project point onto edge segment
        Vec2 edge = v2 - v1;
        float edgeLen = edge.length();
        if (edgeLen < 0.001f)
            continue;

        Vec2 edgeDir = edge / edgeLen;
        float t = (point - v1).dot (edgeDir);
        t = std::clamp (t, 0.0f, edgeLen);

        Vec2 closest = v1 + edgeDir * t;
        float dist = (point - closest).length();

        if (dist < minDist)
        {
            minDist = dist;
            // Normal points outward (perpendicular to edge)
            bestPushDir = Vec2 (-edgeDir.y, edgeDir.x);

            // Make sure normal points away from center
            if ((closest + bestPushDir - center).lengthSquared() < (closest - center).lengthSquared())
                bestPushDir = bestPushDir * -1.0f;
        }
    }

    pushDirection = bestPushDir;
    pushDistance = minDist + 2.0f;
    return true;
}
