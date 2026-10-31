// =============================================================================
// UnitState.hpp
//
// What a unit is doing this tick. Derived from its UnitCommand every tick and
// read by renderers (and later the Animation class) to pick sprites.
// =============================================================================

#pragma once
#include <cstdint> // uint8_t

namespace Nightmourn {

enum class UnitState : std::uint8_t {
    Idle,      // standing still
    Moving,    // walking toward a position or a target entity
    Attacking, // in range of its target, hitting it every attack_interval
};

} // namespace Nightmourn
