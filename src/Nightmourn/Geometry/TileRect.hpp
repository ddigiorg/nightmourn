// =============================================================================
// Geometry/TileRect.hpp
//
// An inclusive rectangle of whole tiles: every tile from `min` to `max`,
// both included. A building's footprint and a settlement's build area are
// TileRects, so "does it fit" is integer math with no float edge cases:
//
//   const TileRect house = TileRect::from_size({ 4, 7 }, { 2, 2 }); // 4..5, 7..8
//   area.contains(house);
//
// A TileRect is never empty (min <= max on both axes).
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include <algorithm> // max, min
#include <cstdint>   // int32_t

namespace Nightmourn {

struct TileRect {
    TileCoord min = {};
    TileCoord max = {};

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    // `size` tiles (each at least 1) from `origin`, its bottom-left tile
    static constexpr TileRect from_size(TileCoord origin, TileCoord size) {
        return { origin, { origin.x + size.x - 1, origin.y + size.y - 1 } };
    }

    // Every tile within `radius` tiles of `center` along each axis: a
    // (2 * radius + 1) square
    static constexpr TileRect around(TileCoord center, int32_t radius) {
        return { { center.x - radius, center.y - radius },
                 { center.x + radius, center.y + radius } };
    }

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    constexpr int32_t width () const { return max.x - min.x + 1; }
    constexpr int32_t height() const { return max.y - min.y + 1; }

    constexpr bool contains(TileCoord t) const {
        return t.x >= min.x && t.x <= max.x && t.y >= min.y && t.y <= max.y;
    }

    constexpr bool contains(const TileRect& r) const {
        return contains(r.min) && contains(r.max);
    }

    constexpr bool overlaps(const TileRect& r) const {
        return min.x <= r.max.x && max.x >= r.min.x && min.y <= r.max.y && max.y >= r.min.y;
    }

    // Grown by `n` tiles on every side
    constexpr TileRect grow(int32_t n) const {
        return { { min.x - n, min.y - n }, { max.x + n, max.y + n } };
    }

    // World-space area it covers: [min, max + 1) on both axes
    constexpr Area2D bounds() const {
        return { min.min(), max.min() + 1.0f };
    }

    constexpr Position2D center() const { return bounds().center(); }

    // Visits every tile, row-major (y, then x)
    template <typename Fn>
    void each(Fn&& fn) const {
        for (int32_t y = min.y; y <= max.y; y++)
            for (int32_t x = min.x; x <= max.x; x++) fn(TileCoord{ x, y });
    }

    constexpr bool operator==(const TileRect&) const = default;
};

} // namespace Nightmourn
