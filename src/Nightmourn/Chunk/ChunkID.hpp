// =============================================================================
// ChunkID.hpp
//
// Chunk identifier based on grid position.
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/World/Constants.hpp"
#include <cassert>    // assert
#include <cmath>      // floor
#include <compare>    // strong_ordering
#include <cstdint>    // int16_t, uint16_t, uint32_t, INT16_MAX
#include <functional> // hash

namespace Nightmourn {

class ChunkID {
public:
    using value_t = int16_t;

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr int16_t NULL_COORD = INT16_MAX;

    static ChunkID pos_to_id(const Position2D pos) {
        return {
            /*.x = */static_cast<int16_t>(std::floor(pos.x / CHUNK_SIZE.x)),
            /*.y = */static_cast<int16_t>(std::floor(pos.y / CHUNK_SIZE.y))
        };
    }

    static Position2D id_to_pos(const ChunkID id) {
        return {
            /*.x = */static_cast<float>(id.x) * CHUNK_SIZE.x,
            /*.y = */static_cast<float>(id.y) * CHUNK_SIZE.y,
        };
    }

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr ChunkID() = default;

    constexpr ChunkID(int16_t x, int16_t y) : x(x), y(y) {
        assert(x >= CHUNK_MIN_X && "Chunk x must be in range [-31250, 31249]");
        assert(y >= CHUNK_MIN_Y && "Chunk y must be in range [-31250, 31249]");
        assert(x <= CHUNK_MAX_X && "Chunk x must be in range [-31250, 31249]");
        assert(y <= CHUNK_MAX_Y && "Chunk y must be in range [-31250, 31249]");
    }

    static constexpr ChunkID null() { return ChunkID{}; }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    constexpr bool is_null()  const { return x == NULL_COORD && y == NULL_COORD; }
    constexpr bool is_valid() const { return x != NULL_COORD && y != NULL_COORD; }

    explicit constexpr operator bool() const { return is_valid(); }

    // -------------------------------------------------------------------------
    // Comparison
    //
    // Ordering is row-major (y, then x). Used to keep active chunk lists in a
    // canonical, history-independent order for deterministic ticking.
    // -------------------------------------------------------------------------

    constexpr bool operator==(const ChunkID&) const = default;

    constexpr std::strong_ordering operator<=>(const ChunkID& o) const {
        if (const auto c = y <=> o.y; c != 0) return c;
        return x <=> o.x;
    }

    // -------------------------------------------------------------------------
    // Hash
    //
    // MurmurHash3 32-bit finalizer (fmix32).
    // -------------------------------------------------------------------------

    struct Hash {
        constexpr std::size_t operator()(const ChunkID& id) const {
            uint32_t h = (static_cast<uint32_t>(static_cast<uint16_t>(id.x)) << 16)
                        | static_cast<uint32_t>(static_cast<uint16_t>(id.y));
            h ^= h >> 16;
            h *= 0x85ebca6bU;
            h ^= h >> 13;
            h *= 0xc2b2ae35U;
            h ^= h >> 16;
            return static_cast<std::size_t>(h);
        }
    };

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    int16_t x = NULL_COORD;
    int16_t y = NULL_COORD;
};

} // namespace Nightmourn

// -----------------------------------------------------------------------------
// Hash support for use in std containers
// -----------------------------------------------------------------------------

template <>
struct std::hash<Nightmourn::ChunkID> : Nightmourn::ChunkID::Hash {};
