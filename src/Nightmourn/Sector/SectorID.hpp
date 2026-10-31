// =============================================================================
// SectorID.hpp
//
// Sector identifier based on grid position.
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/World/Constants.hpp"
#include <cassert>    // assert
#include <cmath>      // floor
#include <cstdint>    // int32_t, uint32_t, uint64_t, INT32_MAX
#include <functional> // hash

namespace Nightmourn {

class SectorID {
public:
    using value_t = int32_t;

    // -------------------------------------------------------------------------
    // Static Members and Functions
    // -------------------------------------------------------------------------

    static constexpr int32_t NULL_COORD = INT32_MAX;

    static SectorID pos_to_id(const Position2D pos) {
        return SectorID(
            /*.x = */static_cast<int32_t>(std::floor(pos.x / SECTOR_SIZE.x)),
            /*.y = */static_cast<int32_t>(std::floor(pos.y / SECTOR_SIZE.y))
        );
    }

    static Position2D id_to_pos(const SectorID id) {
        return {
            /*.x = */static_cast<float>(id.x) * SECTOR_SIZE.x,
            /*.y = */static_cast<float>(id.y) * SECTOR_SIZE.y,
        };
    }

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr SectorID() = default;

    constexpr SectorID(int32_t x, int32_t y) : x(x), y(y) {
        assert(x >= SECTOR_MIN_X && "Sector x must be in range [-500000, 499999]");
        assert(y >= SECTOR_MIN_Y && "Sector y must be in range [-500000, 499999]");
        assert(x <= SECTOR_MAX_X && "Sector x must be in range [-500000, 499999]");
        assert(y <= SECTOR_MAX_Y && "Sector y must be in range [-500000, 499999]");
    }

    static constexpr SectorID null() { return SectorID{}; }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    constexpr bool is_null()  const { return x == NULL_COORD && y == NULL_COORD; }
    constexpr bool is_valid() const { return x != NULL_COORD && y != NULL_COORD; }

    explicit constexpr operator bool() const { return is_valid(); }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    constexpr bool operator==(const SectorID&) const = default;

    // -------------------------------------------------------------------------
    // Hash
    //
    // MurmurHash3 64-bit finalizer (fmix64).
    // -------------------------------------------------------------------------

    struct Hash {
        constexpr std::size_t operator()(const SectorID& id) const {
            uint64_t h = (static_cast<uint64_t>(static_cast<uint32_t>(id.x)) << 32)
                        | static_cast<uint64_t>(static_cast<uint32_t>(id.y));
            h ^= h >> 33;
            h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return static_cast<std::size_t>(h);
        }
    };

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    int32_t x = NULL_COORD;
    int32_t y = NULL_COORD;
};

} // namespace Nightmourn

// -----------------------------------------------------------------------------
// Hash support for use in std containers
// -----------------------------------------------------------------------------

template <>
struct std::hash<Nightmourn::SectorID> : Nightmourn::SectorID::Hash {};
