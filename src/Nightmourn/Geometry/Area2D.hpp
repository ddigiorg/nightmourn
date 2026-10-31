// =============================================================================
// Area2D.hpp
//
// Rectangular 2D area in the game world (tile units).
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Position2D.hpp"
#include <glm/glm.hpp>

namespace Nightmourn {

struct Area2D {
    Position2D min = { 0.0f, 0.0f };
    Position2D max = { 0.0f, 0.0f };

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr Area2D() {};

    constexpr Area2D(Position2D min, Position2D max)
        : min(min), max(max) {}

    constexpr Area2D(float min_x, float min_y, float max_x, float max_y)
        : min({min_x, min_y}), max({max_x, max_y}) {}

    // Square box centered on a point, e.g. for radius searches.
    static constexpr Area2D around(Position2D center, float radius) {
        return { center - radius, center + radius };
    }

    // -------------------------------------------------------------------------
    // Conversion
    // -------------------------------------------------------------------------

    glm::vec4 to_vec4() const {
        return glm::vec4(min.x, min.y, max.x, max.y);
    }

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    constexpr Position2D center() const {
        return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
    }

    constexpr Position2D size() const {
        return { max.x - min.x, max.y - min.y };
    }

    constexpr bool contains(const Position2D& p) const {
        return (p.x >= min.x && p.x <= max.x) &&
               (p.y >= min.y && p.y <= max.y);
    }

    constexpr bool overlaps(const Area2D& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y);
    }

    // -------------------------------------------------------------------------
    // Arithmetic
    // -------------------------------------------------------------------------

    // Movement
    constexpr Area2D operator+(Position2D p) const { return { min + p, max + p }; }
    constexpr Area2D operator-(Position2D p) const { return { min - p, max - p }; }

    // Size
    constexpr Area2D& operator+=(Position2D p) { min += p; max += p; return *this; }
    constexpr Area2D& operator-=(Position2D p) { min -= p; max -= p; return *this; }

    constexpr Area2D& grow(float factor) {
        min -= factor;
        max += factor;
        return *this;
    }

    constexpr Area2D& shrink(float factor) {
        min += factor;
        max -= factor;
        return *this;
    }

    // Comparison
    constexpr bool operator==(const Area2D&) const = default;
};

} // namespace Nightmourn
