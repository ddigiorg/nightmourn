// =============================================================================
// Unit.hpp
//
// A mobile, damageable entity that follows UnitCommands. All sizes and speeds
// are in tiles (see World/Constants.hpp), never pixels.
//
// Each tick the unit turns its command into a UnitState:
//
//   Command      No target in reach        Target in reach (see below)
//   ----------   -----------------------   ---------------------------------
//   Stop         Idle                      Moving (chase) / Attacking
//   MoveTo       Moving, Idle on arrival   (ignores enemies)
//   Attack       Idle (target gone)        Moving (chase) / Attacking
//   AttackMove   Moving, Idle on arrival   Moving (chase) / Attacking
//   Group        follows its UnitGroup's Plan (see UnitGroup.hpp)
//
// "In reach" for Stop and AttackMove means an enemy inside aggro_radius. Those
// targets are dropped once they leave it, so units don't chase across the
// map. An Attack target is chased until it dies.
//
// Group members hold their slot while the group is Formed or Reforming, and
// behave like AttackMove while it is Engaged (heading for the enemy the
// group saw when nothing is in their own reach).
//
// Commands are set between ticks (by the app or UnitGroup::add_member),
// never by another entity while ticking: that would break the "write only
// yourself" rule in Entity.hpp. Issuing a unit its own command takes it out
// of its group.
// =============================================================================

#pragma once
#include "Nightmourn/Unit/UnitCommand.hpp"
#include "Nightmourn/Unit/UnitConfig.hpp"
#include "Nightmourn/Unit/UnitState.hpp"
#include "Nightmourn/Entity/EntityTypes.hpp"
#include "Nightmourn/Entity/EntityDamageable.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include <cassert> // assert
#include <cstdint> // uint16_t

namespace Nightmourn {

struct WorldContext;

class Unit : public EntityDamageable {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr EntityTypeID TYPE_ID = EntityTypes::Unit;

    // Idle and attack-moving units look for enemies every N ticks, staggered
    // by handle index so the searches spread evenly across ticks.
    static constexpr uint16_t ACQUIRE_INTERVAL = 60; // 1.0 s at 60 UPS

    // Members of an Engaged group already know enemies are near, so they
    // look more often.
    static constexpr uint16_t ENGAGED_ACQUIRE_INTERVAL = 10;

    // A move ends this close to its destination (tiles)
    static constexpr float ARRIVE_DISTANCE = 0.05f;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the unit
    Unit(const UnitConfig& config, EntityFactionsMask factions);

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    void tick(WorldContext& ctx) override;

    // -------------------------------------------------------------------------
    // Commands
    // -------------------------------------------------------------------------

    // Leaves the unit's group, if any. Use UnitGroup::add_member to join one.
    void issue(const UnitCommand& cmd) {
        assert(cmd.type != UnitCommandType::Group && "Join groups with UnitGroup::add_member");
        command_ = cmd;
        target_  = cmd.target;
        group_   = {};
    }

    void stop       ()                    { issue(UnitCommand::stop());                }
    void move_to    (Position2D position) { issue(UnitCommand::move_to(position));     }
    void attack     (EntityHandle target) { issue(UnitCommand::attack(target));        }
    void attack_move(Position2D position) { issue(UnitCommand::attack_move(position)); }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const UnitConfig&  config()  const { return *config_; }
    const UnitCommand& command() const { return command_; }
    UnitState          state()   const { return state_;   }

    EntityHandle       target()  const { return target_;  }

    // Unit vector the unit faces (last movement or attack direction)
    Position2D         facing()  const { return facing_;  }

    // Ticks spent in the current state (saturates at 65535, ~18 min at 60 UPS).
    // Lets the app react to long idles, and the future Animation pick frames.
    uint16_t           state_ticks() const { return state_ticks_; }

    // UnitGroup the unit follows (null if none), and its slot in it
    EntityHandle       group()       const { return group_;      }
    uint16_t           group_slot()  const { return group_slot_; }

private:
    friend class UnitGroup; // join_group_

    // -------------------------------------------------------------------------
    // Behaviors
    // -------------------------------------------------------------------------

    // Current target if it is still alive (and, unless `leash` is 0, within
    // `leash` tiles), otherwise clears it and returns nullptr.
    const Entity* resolve_target_(WorldContext& ctx, float leash);

    // Nearest enemy inside aggro_radius, searched every `interval` ticks.
    const Entity* acquire_target_(WorldContext& ctx, uint16_t interval = ACQUIRE_INTERVAL);

    // Group command: read the group's plan for this tick and act on it.
    void follow_group_(WorldContext& ctx);

    // Steer toward a formation slot moving at `slot_velocity`: that velocity
    // plus `gain * (slot - position)`, capped at move_speed. Faces `facing`
    // once settled.
    void hold_slot_(Position2D slot, Position2D slot_velocity, Position2D facing, float gain);

    void join_group_(EntityHandle group, uint16_t slot) {
        command_    = { UnitCommandType::Group, {}, {} };
        target_     = {};
        group_      = group;
        group_slot_ = slot;
    }

    void leave_group_() {
        command_ = UnitCommand::stop();
        group_   = {};
    }

    // Chase `target` into attack range, then hit it whenever off cooldown.
    void engage_(WorldContext& ctx, const Entity& target);

    // Set velocity toward `destination`, stopping `stop_distance` short of
    // it. Returns true (and zero velocity) once there.
    bool move_toward_(WorldContext& ctx, Position2D destination, float stop_distance);

    void face_(Position2D delta, float distance);

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const UnitConfig* config_          = nullptr;            //  8 B
    UnitCommand       command_         = {};                 // 20 B
    EntityHandle      target_          = {};                 //  8 B  engaged target
    EntityHandle      group_           = {};                 //  8 B  UnitGroup followed
    Position2D        facing_          = { 0.0f, -1.0f };    //  8 B  down
    uint16_t          attack_cooldown_ = 0;                  //  2 B  ticks until next hit
    uint16_t          state_ticks_     = 0;                  //  2 B  ticks in state_
    uint16_t          group_slot_      = 0;                  //  2 B  slot in group_
    UnitState         state_           = UnitState::Idle;    //  1 B
};

} // namespace Nightmourn
