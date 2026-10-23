// =============================================================================
// Character/Character.cpp
// =============================================================================

#include "Nightmourn/Character/Character.hpp"
#include "Nezumi/World/WorldContext.hpp"
#include <algorithm> // min
#include <cmath>     // sqrt

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

Character::Character(const CharacterConfig& config, Nezumi::EntityFactionsMask factions)
    : Nezumi::EntityDamageable(config.max_health)
    , config_(&config)
{
    this->set_active();
    this->set_tickable();
    this->set_drawable();

    factions_mask    = factions;
    collision_bounds = config.collision_bounds;
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void Character::tick(Nezumi::WorldContext&) {
    // Only velocity is written: the movement phase moves the character and
    // updates its chunk once every entity has ticked
    const Nezumi::Position2D dir = command_.move_dir;

    const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length == 0.0f) {
        velocity = {};
        return;
    }

    // Never faster than move_speed, whatever the command's length
    facing_  = dir / length;
    velocity = facing_ * (config_->move_speed * std::min(length, 1.0f));
}

} // namespace Nightmourn
