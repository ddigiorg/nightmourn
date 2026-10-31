// =============================================================================
// Entity/EntityDamageable.hpp
//
// Base for entities with health (units, buildings, ...).
//
// Damage is never applied directly while ticking. Issue it through
// World::damage_entity(); World applies it in the damage phase (after every
// entity has ticked), in issue order, and removes entities whose health
// reaches zero. Two units that hit each other on the same tick both take the
// hit, whatever order they ticked in.
//
// The constructor sets the reserved Damageable flag, which is how World
// recognises a damageable entity without RTTI or a virtual call:
//
//   if (entity->is_damageable())
//       static_cast<EntityDamageable*>(entity)->apply_damage(amount);
// =============================================================================

#pragma once
#include "Nightmourn/Entity/Entity.hpp"
#include <algorithm> // min
#include <cstdint>   // uint16_t

namespace Nightmourn {

class EntityDamageable : public Entity {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit EntityDamageable(uint16_t max_health)
        : health(max_health), max_health(max_health)
    {
        this->set_damageable();
    }

    // -------------------------------------------------------------------------
    // Health
    // -------------------------------------------------------------------------

    bool is_dead()        const { return health == 0;          }
    bool is_full_health() const { return health == max_health; }

    // Health as a fraction of max health, in [0, 1]. Render use only: the
    // simulation compares integers.
    float health_ratio() const {
        return max_health
            ? static_cast<float>(health) / static_cast<float>(max_health)
            : 0.0f;
    }

    // Subtract up to `amount` health and return what was actually subtracted
    // (never more than the remaining health). World only.
    uint16_t apply_damage(uint16_t amount) {
        const uint16_t applied = std::min(amount, health);
        health = static_cast<uint16_t>(health - applied);
        return applied;
    }

    // -------------------------------------------------------------------------
    // Members
    //
    // Fits in the 32 B left in Entity's second cache line.
    // -------------------------------------------------------------------------

    uint16_t health     = 0u; // 2 B
    uint16_t max_health = 0u; // 2 B
};

} // namespace Nightmourn
