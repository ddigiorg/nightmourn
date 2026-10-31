// =============================================================================
// UnitConfig.hpp
//
// Shared, data-driven stats for one kind of unit (Knight, Archer, ...). Units
// keep a pointer to their config, so a config must outlive every unit built
// from it (keep them in static storage or a registry owned by the game).
//
// Distances are in tiles, speeds in tiles per second, and durations in ticks
// so the simulation never converts wall time.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct UnitConfig {
    std::string_view name    = {};
    EntitySubtypeID  subtype = {}; // copied to Entity::subtype_id

    // Health
    uint16_t max_health = 100;

    // Movement
    float move_speed = 2.0f; // tiles per second

    // Combat
    uint16_t attack_damage   = 10;   // per hit
    uint16_t attack_interval = 60;   // ticks between hits (60 = 1 s at 60 UPS)
    float    attack_range    = 1.0f; // tiles, center to center
    float    aggro_radius    = 6.0f; // tiles; enemies inside are engaged when idle
                                     // or attack-moving (0: only on command)

    // Collision box, relative to the unit's position (its feet)
    Area2D collision_bounds = { -0.25f, -0.25f, 0.25f, 0.25f };

    // TODO: selection_bounds, Animation
};

} // namespace Nightmourn
