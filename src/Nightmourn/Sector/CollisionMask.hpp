// =============================================================================
// CollisionMask.hpp
//
// Game collision layers.
// =============================================================================

#pragma once
#include "Nezumi/Core/EnumClassFlags.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

enum class CollisionMask : uint16_t {
    None      = 0u,
    Layer00 = 1u <<  0,
    Layer01 = 1u <<  1,
    Layer02 = 1u <<  2,
    Layer03 = 1u <<  3,
    Layer04 = 1u <<  4,
    Layer05 = 1u <<  5,
    Layer06 = 1u <<  6,
    Layer07 = 1u <<  7,
    Layer08 = 1u <<  8,
    Layer09 = 1u <<  9,
    Layer10 = 1u << 10,
    Layer11 = 1u << 11,
    Layer12 = 1u << 12,
    Layer13 = 1u << 13,
    Layer14 = 1u << 14,
    Layer15 = 1u << 15,
};

ENUM_CLASS_FLAGS(CollisionMask);

} // namespace Nightmourn
