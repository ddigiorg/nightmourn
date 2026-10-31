// =============================================================================
// TileCoord.hpp
//
// Integer tile coordinates: the integer part of a tile-space Position2D.
//
// Positions map to tiles with floor, so every point belongs to exactly one
// tile, including negative ones:
//
//   TileCoord::from_position({  3.25f, 0.0f }) -> {  3, 0 }
//   TileCoord::from_position({ -0.25f, 0.0f }) -> { -1, 0 }
//
// FUTURE: with fixed-point positions (ROADMAP.md) from_position() becomes an
// arithmetic shift and fraction() a mask; the API stays the same.
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include <cmath>   // floor
#include <cstdint> // int32_t

namespace Nightmourn {

struct TileCoord {
    int32_t x = 0;
    int32_t y = 0;

    // -------------------------------------------------------------------------
    // Conversion
    // -------------------------------------------------------------------------

    // Tile containing `pos`.
    static TileCoord from_position(Position2D pos) {
        return {
            /*.x = */static_cast<int32_t>(std::floor(pos.x)),
            /*.y = */static_cast<int32_t>(std::floor(pos.y)),
        };
    }

    // How far into its tile `pos` is, each axis in [0, 1).
    static Position2D fraction(Position2D pos) {
        return { pos.x - std::floor(pos.x), pos.y - std::floor(pos.y) };
    }

    // Bottom-left corner (the tile's own coordinates as a position).
    constexpr Position2D min() const {
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    constexpr Position2D center() const { return min() + 0.5f; }

    // The tile's area, [x, x + 1) by [y, y + 1). Handy as a sprite pos_box:
    // Sprite{ .pos_box = tile.bounds().to_vec4(), ... }
    constexpr Area2D bounds() const { return { min(), min() + 1.0f }; }

    // -------------------------------------------------------------------------
    // Equality
    // -------------------------------------------------------------------------

    constexpr bool operator==(const TileCoord&) const = default;
};

} // namespace Nightmourn
