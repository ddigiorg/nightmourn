// =============================================================================
// Character/Character.cpp
// =============================================================================

#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <algorithm> // clamp, min
#include <cmath>     // sqrt

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

Character::Character(const CharacterConfig& config, EntityFactionsMask factions)
    : EntityDamageable(config.max_health)
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

void Character::tick(WorldContext& ctx) {
    // Only velocity is written: the movement phase moves the character and
    // updates its chunk once every entity has ticked
    const Position2D dir = command_.move_dir;

    const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length == 0.0f) {
        velocity = {};
    } else {
        // Never faster than move_speed, whatever the command's length
        facing_  = dir / length;
        velocity = facing_ * (config_->move_speed * std::min(length, 1.0f));
    }

    mine_(ctx);
}

// -----------------------------------------------------------------------------
// Mining
// -----------------------------------------------------------------------------

bool Character::can_reach(const Entity& resource) const {
    // Distance from the feet to the nearest point of the resource's box, so
    // a big trunk is reached from its edge, not its middle
    const Area2D box = resource.collision_box();
    const float dx = position.x - std::clamp(position.x, box.min.x, box.max.x);
    const float dy = position.y - std::clamp(position.y, box.min.y, box.max.y);
    return dx * dx + dy * dy <= config_->mine_reach * config_->mine_reach;
}

void Character::mine_(WorldContext& ctx) {
    if (mine_cooldown_ > 0) mine_cooldown_--;
    if (!command_.mine_target) return;

    const Entity* target = ctx.world->get_entity(command_.mine_target);
    if (!target || !target->is_alive() || !target->is_resource() || !can_reach(*target)) return;

    // Face the resource while swinging at it, unless walking somewhere
    if (velocity.x == 0.0f && velocity.y == 0.0f) {
        const Position2D to = target->position - position;
        const float d = std::sqrt(to.x * to.x + to.y * to.y);
        if (d > 0.0f) facing_ = to / d;
    }

    if (mine_cooldown_ > 0) return;

    // Applied in the interaction phase, after every entity has ticked
    ctx.world->mine_entity(command_.mine_target, config_->mine_amount, handle);
    mine_cooldown_   = config_->mine_interval;
    last_swing_tick_ = ctx.tick;
    has_swung_       = true;
}

} // namespace Nightmourn
