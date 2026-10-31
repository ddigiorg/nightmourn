// =============================================================================
// Position2D.hpp
//
// Coordinates in the game world, in tile-space: floor(x) is the tile column,
// the fraction is the offset inside it. See Constants.hpp and TileCoord.hpp.
//
// FUTURE: replaced by fixed-point (24.8) positions. See ROADMAP.md.
// =============================================================================

#pragma once
#include <glm/glm.hpp>

namespace Nightmourn {

struct Position2D {
    float x = 0.0f;
    float y = 0.0f;

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr Position2D() {};

    constexpr Position2D(float x, float y) : x(x), y(y) {};

    // -------------------------------------------------------------------------
    // Conversion
    // -------------------------------------------------------------------------

    glm::vec2 to_vec2() const { return glm::vec2(x, y); }

    // -------------------------------------------------------------------------
    // Arithmetic (scalar)
    // -------------------------------------------------------------------------

    constexpr Position2D operator+(const float s) const { return { x + s, y + s }; }
    constexpr Position2D operator-(const float s) const { return { x - s, y - s }; }
    constexpr Position2D operator*(const float s) const { return { x * s, y * s }; }
    constexpr Position2D operator/(const float s) const { return { x / s, y / s }; }

    // -------------------------------------------------------------------------
    // Arithmetic (other)
    // -------------------------------------------------------------------------

    constexpr Position2D operator+(const Position2D& o) const { return { x + o.x, y + o.y }; }
    constexpr Position2D operator-(const Position2D& o) const { return { x - o.x, y - o.y }; }
    constexpr Position2D operator*(const Position2D& o) const { return { x * o.x, y * o.y }; }
    constexpr Position2D operator/(const Position2D& o) const { return { x / o.x, y / o.y }; }

    // -------------------------------------------------------------------------
    // Negation
    // -------------------------------------------------------------------------

    constexpr Position2D operator-() const { return { -x, -y }; }

    // -------------------------------------------------------------------------
    // Compound Assignment (scalar)
    // -------------------------------------------------------------------------

    constexpr Position2D& operator+=(const float s) { x += s; y += s; return *this; }
    constexpr Position2D& operator-=(const float s) { x -= s; y -= s; return *this; }
    constexpr Position2D& operator*=(const float s) { x *= s; y *= s; return *this; }
    constexpr Position2D& operator/=(const float s) { x /= s; y /= s; return *this; }

    // -------------------------------------------------------------------------
    // Compound Assignment (other)
    // -------------------------------------------------------------------------

    constexpr Position2D& operator+=(const Position2D& o) { x += o.x; y += o.y; return *this; }
    constexpr Position2D& operator-=(const Position2D& o) { x -= o.x; y -= o.y; return *this; }
    constexpr Position2D& operator*=(const Position2D& o) { x *= o.x; y *= o.y; return *this; }
    constexpr Position2D& operator/=(const Position2D& o) { x /= o.x; y /= o.y; return *this; }

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    constexpr bool is_zero() const { return x == 0.0f && y == 0.0f; }

    constexpr float distance2(const Position2D& o) const {
        const float dx = o.x - x;
        const float dy = o.y - y;
        return dx * dx + dy * dy;
    }

    // -------------------------------------------------------------------------
    // Equality
    // -------------------------------------------------------------------------

    constexpr bool approx_equal(const Position2D& o, float margin = 1e-5f) const {
        return o.x >= x - margin
            && o.y >= y - margin
            && o.x <= x + margin
            && o.y <= y + margin;
    }

    constexpr bool operator==(const Position2D&) const = default;
};

} // namespace Nightmourn
