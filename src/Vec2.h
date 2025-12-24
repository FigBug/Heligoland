#pragma once

#include <cmath>
#include <numbers>

inline constexpr double pi = std::numbers::pi;

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2 (float x_, float y_) : x (x_), y (y_) {}

    Vec2 operator+ (const Vec2& other) const { return { x + other.x, y + other.y }; }
    Vec2 operator- (const Vec2& other) const { return { x - other.x, y - other.y }; }
    Vec2 operator* (float scalar) const { return { x * scalar, y * scalar }; }
    Vec2 operator/ (float scalar) const { return { x / scalar, y / scalar }; }

    Vec2& operator+= (const Vec2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vec2& operator-= (const Vec2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    Vec2& operator*= (float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    float length() const { return std::sqrt (x * x + y * y); }
    float lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const
    {
        float len = length();
        if (len > 0.0f)
            return { x / len, y / len };
        return { 0.0f, 0.0f };
    }

    float dot (const Vec2& other) const { return x * other.x + y * other.y; }

    static Vec2 fromAngle (float radians)
    {
        return { std::cos (radians), std::sin (radians) };
    }

    float toAngle() const
    {
        return std::atan2 (y, x);
    }
};
