// =============================================================================
// Entity/EntityFlagsMask.hpp
//
// An entity's boolean properties, stored as a 32-bit mask. Entity has a named
// is_/set_/clear_ accessor for each one; has_flags() tests several at once.
//
// Lifecycle flags (Active, PendingCreate, PendingRemove) belong to World:
// read them freely, but change Active only through World::set_entity_active
// so the flag and the chunk list membership always agree.
// =============================================================================

#pragma once
#include "Nezumi/Core/EnumClassFlags.hpp"
#include <cstdint> // uint32_t

namespace Nightmourn {

enum class EntityFlagsMask : std::uint32_t {
    None = 0,

    // Lifecycle (World)
    Active         = 1u <<  0, // in its chunk's active list and ticked each tick
    PendingCreate  = 1u <<  1, // joins the world at the next command phase
    PendingRemove  = 1u <<  2, // leaves the world at the next command phase

    // Simulation
    Tickable       = 1u <<  3, // can be activated (has a tick worth running)
    Collidable     = 1u <<  4, // registered in the collision sector grid
    Resource       = 1u <<  5, // derives from EntityResource (can be mined)
    Damageable     = 1u <<  6, // derives from EntityDamageable (has health)
    EmitsPheromone = 1u <<  7, // leaves its faction's scent on its chunk

    // Presentation
    Drawable       = 1u <<  8, // drawn by the world drawers
    Selectable     = 1u <<  9, // can be picked with the cursor

    // Bits 10-31 are free
};

ENUM_CLASS_FLAGS(EntityFlagsMask);

} // namespace Nightmourn
