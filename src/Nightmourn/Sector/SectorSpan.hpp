// =============================================================================
// SectorSpan.hpp
//
// Represents a rectangular range of sectors (inclusive).
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Sector/SectorID.hpp"
#include <algorithm> // max
#include <cassert>   // assert
#include <cstddef>   // size_t

namespace Nightmourn {

class SectorSpan {
public:

    // -------------------------------------------------------------------------
    // Static Members and Functions
    // -------------------------------------------------------------------------

    // See ChunkSpan::span_from_box for the MARGIN and degenerate-box guard.
    static SectorSpan span_from_box(const Area2D& box) {
        static constexpr float MARGIN = 0.001f; // TODO: make this global?
        return SectorSpan(
            SectorID::pos_to_id({ box.min.x, box.min.y }),
            SectorID::pos_to_id({ std::max(box.min.x, box.max.x - MARGIN),
                                  std::max(box.min.y, box.max.y - MARGIN) })
        );
    }

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr SectorSpan() = default;

    constexpr SectorSpan(SectorID min, SectorID max) : min(min), max(max) {
        assert(min.x <= max.x);
        assert(min.y <= max.y);
    }

    constexpr SectorSpan(SectorID::value_t min_x, SectorID::value_t min_y,
                         SectorID::value_t max_x, SectorID::value_t max_y)
        : min(min_x, min_y), max(max_x, max_y)
    {
        assert(min_x <= max_x);
        assert(min_y <= max_y);
    }

    static constexpr SectorSpan null() { return SectorSpan{}; }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    constexpr bool is_null() const {
        return min.is_null() && max.is_null();
    }

    constexpr bool is_valid() const {
        return min.is_valid() && max.is_valid() &&
               min.x <= max.x && min.y <= max.y;
    }

    explicit constexpr operator bool() const { return is_valid(); }

    constexpr bool contains(SectorID id) const {
        assert(is_valid());
        return id.x >= min.x && id.x <= max.x &&
               id.y >= min.y && id.y <= max.y;
    }

    constexpr size_t width() const {
        assert(is_valid());
        return static_cast<size_t>(max.x - min.x + 1);
    }

    constexpr size_t height() const {
        assert(is_valid());
        return static_cast<size_t>(max.y - min.y + 1);
    }

    constexpr size_t area() const {
        assert(is_valid());
        return width() * height();
    }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    constexpr bool operator==(const SectorSpan&) const = default;

    // -------------------------------------------------------------------------
    // Iteration (row-major, deterministic)
    // -------------------------------------------------------------------------

    template <typename Fn>
    constexpr void each(Fn&& fn) const {
        assert(is_valid());
        SectorID id;
        for (id.y = min.y; id.y <= max.y; id.y++) {
            for (id.x = min.x; id.x <= max.x; id.x++) {
                fn(id);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    SectorID min = SectorID::null();
    SectorID max = SectorID::null();
};

} // namespace Nightmourn
