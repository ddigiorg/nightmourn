// =============================================================================
// EntityFactionsMask.hpp
//
// Entity factions.
//
// An entity stores its factions as a mask so friend/enemy checks are one AND.
// Per-faction data (pheromones, stats, AI) lives in arrays indexed by
// FactionIndex, the bit position in the mask, e.g. each chunk's
// Pheromones::values[unit.faction_index()].
// =============================================================================

#pragma once
#include "Nezumi/Core/EnumClassFlags.hpp"
#include <bit>     // countr_zero, has_single_bit
#include <cassert> // assert
#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint16_t

namespace Nightmourn {

enum class EntityFactionsMask : std::uint16_t {
    None      = 0u,
    Faction00 = 1u <<  0,
    Faction01 = 1u <<  1,
    Faction02 = 1u <<  2,
    Faction03 = 1u <<  3,
    Faction04 = 1u <<  4,
    Faction05 = 1u <<  5,
    Faction06 = 1u <<  6,
    Faction07 = 1u <<  7,
    Faction08 = 1u <<  8,
    Faction09 = 1u <<  9,
    Faction10 = 1u << 10,
    Faction11 = 1u << 11,
    Faction12 = 1u << 12,
    Faction13 = 1u << 13,
    Faction14 = 1u << 14,
    Faction15 = 1u << 15,
    All       = 0xFFFFu,
};

ENUM_CLASS_FLAGS(EntityFactionsMask);

// -----------------------------------------------------------------------------
// Faction Index
// -----------------------------------------------------------------------------

using FactionIndex = std::uint8_t;

inline constexpr std::size_t MAX_FACTIONS = 16;

// Single-faction mask for an index: faction_bit(3) == Faction03
constexpr EntityFactionsMask faction_bit(FactionIndex index) {
    assert(index < MAX_FACTIONS && "FactionIndex out of range");
    return static_cast<EntityFactionsMask>(1u << index);
}

// Index of a single-faction mask: faction_index(Faction03) == 3
constexpr FactionIndex faction_index(EntityFactionsMask mask) {
    const auto bits = static_cast<std::uint16_t>(mask);
    assert(std::has_single_bit(bits) && "Mask must hold exactly one faction");
    return static_cast<FactionIndex>(std::countr_zero(bits));
}

// Visit the index of every faction in a mask, lowest first
template <typename Fn>
constexpr void each_faction(EntityFactionsMask mask, Fn&& fn) {
    auto bits = static_cast<std::uint16_t>(mask);
    while (bits) {
        fn(static_cast<FactionIndex>(std::countr_zero(bits)));
        bits &= static_cast<std::uint16_t>(bits - 1u);
    }
}

} // namespace Nightmourn
