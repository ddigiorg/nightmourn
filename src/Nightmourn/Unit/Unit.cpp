// =============================================================================
// Unit.cpp
// =============================================================================

#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/Entity/EntitySearchFilter.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <cassert> // assert
#include <cmath>   // sqrt
#include <cstdint> // UINT16_MAX

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

Unit::Unit(const UnitConfig& config, EntityFactionsMask factions)
    : EntityDamageable(config.max_health)
    , config_(&config)
{
    this->set_active();
    this->set_tickable();
    this->set_drawable();
    this->set_emits_pheromone();

    subtype_id       = config.subtype;
    factions_mask    = factions;
    collision_bounds = config.collision_bounds;
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void Unit::tick(WorldContext& ctx) {
    if (attack_cooldown_ > 0) attack_cooldown_--;
    velocity = {};

    const UnitState prev_state = state_;

    const float aggro = config_->aggro_radius;

    switch (command_.type) {
        case UnitCommandType::Stop: {
            const Entity* target = resolve_target_(ctx, aggro);
            if (!target) target = acquire_target_(ctx);

            if (target) engage_(ctx, *target);
            else        state_ = UnitState::Idle;
            break;
        }

        case UnitCommandType::MoveTo: {
            state_ = UnitState::Moving;
            if (move_toward_(ctx, command_.position, 0.0f)) stop();
            break;
        }

        case UnitCommandType::Attack: {
            const Entity* target = resolve_target_(ctx, 0.0f);

            if (target) engage_(ctx, *target);
            else        stop();
            break;
        }

        case UnitCommandType::AttackMove: {
            const Entity* target = resolve_target_(ctx, aggro);
            if (!target) target = acquire_target_(ctx);

            if (target) {
                engage_(ctx, *target);
            } else {
                state_ = UnitState::Moving;
                if (move_toward_(ctx, command_.position, 0.0f)) stop();
            }
            break;
        }

        case UnitCommandType::Group: {
            follow_group_(ctx);
            break;
        }
    }

    // A command that completed this tick leaves the unit standing still
    if (velocity.is_zero() && state_ == UnitState::Moving) state_ = UnitState::Idle;

    if (state_ != prev_state)  state_ticks_ = 0;
    else if (state_ticks_ < UINT16_MAX) state_ticks_++;
}

// -----------------------------------------------------------------------------
// Behaviors
// -----------------------------------------------------------------------------

const Entity* Unit::resolve_target_(WorldContext& ctx, float leash) {
    if (!target_) return nullptr;

    const Entity* target = ctx.world->get_entity(target_);
    const bool lost = !target || !target->is_alive() ||
                      (leash > 0.0f && distance2(*target) > leash * leash);

    if (lost) {
        target_ = {};
        return nullptr;
    }
    return target;
}

const Entity* Unit::acquire_target_(WorldContext& ctx, uint16_t interval) {
    const float aggro = config_->aggro_radius;
    if (aggro <= 0.0f) return nullptr;
    if ((ctx.tick + handle.index()) % interval != 0) return nullptr;

    const Surface* surface = ctx.world->get_surface(surface_id);
    assert(surface && "Unit is not on a valid surface");

    target_ = surface->find_nearest_entity(position, aggro, EntitySearchFilter::enemies_of(*this));
    return target_ ? ctx.world->get_entity(target_) : nullptr;
}

void Unit::follow_group_(WorldContext& ctx) {
    const UnitGroup* group = ctx.world->get_entity_as<UnitGroup>(group_);
    if (!group || group->is_pending_remove()) {
        leave_group_(); // the group is gone: stand still from next tick
        state_ = UnitState::Idle;
        return;
    }

    const UnitGroup::Plan& plan = group->plan(ctx.tick);

    if (plan.state == UnitGroupState::Engaged) {
        const Entity* target = resolve_target_(ctx, config_->aggro_radius);
        if (!target) target = acquire_target_(ctx, ENGAGED_ACQUIRE_INTERVAL);

        if (target) {
            engage_(ctx, *target);
        } else {
            // Nothing in reach: head for the enemy the group saw
            state_ = UnitState::Moving;
            move_toward_(ctx, plan.engage_position, config_->attack_range);
        }
        return;
    }

    target_ = {};
    hold_slot_(group->slot_position(ctx.tick, group_slot_), plan.velocity, plan.facing,
               group->config().catch_up_gain);
}

void Unit::hold_slot_(Position2D slot, Position2D slot_velocity, Position2D facing, float gain) {
    const Position2D delta = slot - position;
    const float      dist2 = delta.x * delta.x + delta.y * delta.y;

    if (slot_velocity.is_zero() && dist2 <= ARRIVE_DISTANCE * ARRIVE_DISTANCE) {
        velocity = {};
        facing_  = facing;
        state_   = UnitState::Idle;
        return;
    }

    // Feed-forward the slot's velocity and correct the error. With gain * dt
    // well below 1 the correction never overshoots, and far from the slot
    // the cap makes the unit catch up at its full speed.
    Position2D v = slot_velocity + delta * gain;

    const float max_speed = config_->move_speed;
    const float speed     = std::sqrt(v.x * v.x + v.y * v.y);
    if (speed > max_speed) v *= max_speed / speed;

    velocity = v;
    state_   = UnitState::Moving;

    // Small corrections don't turn the unit away from the formation's front
    if (speed > 0.25f * max_speed) face_(v, speed);
    else                           facing_ = facing;
}

void Unit::engage_(WorldContext& ctx, const Entity& target) {
    const float range = config_->attack_range;

    if (!move_toward_(ctx, target.position, range)) {
        state_ = UnitState::Moving;
        return;
    }

    // In range: turn to the target and hit it whenever off cooldown
    const Position2D delta = target.position - position;
    face_(delta, std::sqrt(delta.x * delta.x + delta.y * delta.y));
    state_ = UnitState::Attacking;

    if (attack_cooldown_ == 0) {
        ctx.world->damage_entity(target.handle, config_->attack_damage, handle);
        attack_cooldown_ = config_->attack_interval;
    }
}

bool Unit::move_toward_(WorldContext& ctx, Position2D destination, float stop_distance) {
    const Position2D delta = destination - position;
    const float      dist2 = delta.x * delta.x + delta.y * delta.y;
    const float      stop  = stop_distance > ARRIVE_DISTANCE ? stop_distance : ARRIVE_DISTANCE;

    if (dist2 <= stop * stop) {
        velocity = {};
        return true;
    }

    const float dist   = std::sqrt(dist2);
    const float travel = dist - stop_distance;       // distance left to cover
    const float step   = config_->move_speed * ctx.dt;

    face_(delta, dist);

    // Don't overshoot: on the last step, move exactly the remaining distance
    const float speed = travel < step ? travel / ctx.dt : config_->move_speed;
    velocity = facing_ * speed;
    return false;
}

void Unit::face_(Position2D delta, float distance) {
    if (distance > 0.0f) facing_ = delta / distance;
}

} // namespace Nightmourn
