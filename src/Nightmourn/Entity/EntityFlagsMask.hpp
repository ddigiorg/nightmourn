// =============================================================================
// EntityFlagsMask.hpp
//
// Standard entity boolean values, stored as a 32-bit mask.
// =============================================================================

#pragma once
#include "Nezumi/Core/EnumClassFlags.hpp"
#include <cstdint> // uint32_t

namespace Nightmourn {

enum class EntityFlagsMask : std::uint32_t {
    None = 0,
    Flag00 = 1u <<  0,
    Flag01 = 1u <<  1,
    Flag02 = 1u <<  2,
    Flag03 = 1u <<  3,
    Flag04 = 1u <<  4,
    Flag05 = 1u <<  5,
    Flag06 = 1u <<  6,
    Flag07 = 1u <<  7,
    Flag08 = 1u <<  8,
    Flag09 = 1u <<  9,
    Flag10 = 1u << 10,
    Flag11 = 1u << 11,
    Flag12 = 1u << 12,
    Flag13 = 1u << 13,
    Flag14 = 1u << 14,
    Flag15 = 1u << 15,
    Flag16 = 1u << 16,
    Flag17 = 1u << 17,
    Flag18 = 1u << 18,
    Flag19 = 1u << 19,
    Flag20 = 1u << 20,
    Flag21 = 1u << 21,
    Flag22 = 1u << 22,

    // Reserved
    Flag23 = 1u << 23, // Resource: entity derives from EntityResource
    Flag24 = 1u << 24, // Damageable: entity derives from EntityDamageable
    Flag25 = 1u << 25, // Active: entity in a chunk and ticks each tick
    Flag26 = 1u << 26, // Tickable: entity can activate/deactivate
    Flag27 = 1u << 27, // Drawable: entity can be rendered
    Flag28 = 1u << 28, // Collidable: entity is registered into the collision sector grid
    Flag29 = 1u << 29, // Selectable: entity can be selected
    Flag30 = 1u << 30, // PendingCreate: pending entity create at the next world tick command phase
    Flag31 = 1u << 31, // PendingRemove: pending entity remove at the next world tick command phase
};

ENUM_CLASS_FLAGS(EntityFlagsMask);

} // namespace Nightmourn
