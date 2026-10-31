// =============================================================================
// UnitGroup.hpp
//
// A formation of Units that share one command and one route (Factorio's unit
// group, Age of Empires' formations). The group is an Entity: it sits in the
// chunk grid at its formation's anchor, so spatial queries find it, but it is
// not collidable, damageable or drawable.
//
// Commands are UnitCommands (Stop, MoveTo, Attack, AttackMove). Members get
// UnitCommandType::Group and follow the group's Plan:
//
//   State       Anchor                          Members
//   ---------   -----------------------------   -------------------------------
//   Formed      marches the route, or stands    hold their slots
//   Engaged     follows the members' centroid   fight their own targets
//   Reforming   waits                           walk back into their slots
//
// MoveTo never engages. Stop, Attack and AttackMove engage when the group's
// scan (one search per group, every scan_interval ticks) finds an enemy near
// its members, and reform after disengage_ticks without one.
//
// Stragglers: a member steers toward its slot with the group's velocity plus
// a correction capped at its own move_speed, so members that fell behind
// catch up while the anchor marches below the slowest member's speed. The
// march also slows as the members' lag (RMS distance from their slots) grows.
//
// Order independence. Members read their group while ticking, and the group
// ticks in the same phase, before or after them depending on chunk order. So
// the group never writes what members read this tick: it keeps two Plans
// (and slot tables) and, during tick t, members read plan[t & 1] while the
// group writes plan[(t + 1) & 1]. The group reads only start-of-tick state
// (positions, liveness, membership), and members only write themselves. A
// decision takes effect one tick later, and nothing depends on tick order.
//
// Membership and commands change between ticks only (app code, like Unit
// commands). A unit given its own command leaves its group; a group with no
// members left removes itself. Keep a group active while it has members:
// its plan only advances when it ticks.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nightmourn/Unit/UnitCommand.hpp"
#include "Nightmourn/Unit/UnitGroupConfig.hpp"
#include "Nightmourn/Unit/UnitGroupState.hpp"
#include "Nightmourn/Entity/EntityTypes.hpp"
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include <array>   // array
#include <cstdint> // uint16_t, uint32_t, UINT16_MAX
#include <span>    // span
#include <vector>  // vector

namespace Nightmourn {

class  Unit;
class  World;
struct WorldContext;

class UnitGroup : public Entity {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr EntityTypeID TYPE_ID = EntityTypes::UnitGroup;

    // The anchor reaches a waypoint this close to it (tiles)
    static constexpr float ARRIVE_DISTANCE = 0.05f;

    // Turning toward the route (cosines of the angle between the formation's
    // front and the route). Above REFACE_DOT (~15 deg) the front is left
    // alone; below REFORM_DOT (~45 deg) the group stops and reforms facing
    // the new way instead of swinging its slots around while marching.
    static constexpr float REFACE_DOT = 0.966f;
    static constexpr float REFORM_DOT = 0.707f;

    // -------------------------------------------------------------------------
    // Plan
    //
    // What the group tells its members for one tick.
    // -------------------------------------------------------------------------

    struct Plan {
        UnitGroupState state           = UnitGroupState::Reforming;
        Position2D     facing          = { 0.0f, 1.0f }; // unit vector, the formation's front
        Position2D     velocity        = {};             // anchor velocity (Formed, Reforming)
        Position2D     engage_position = {};             // last enemy seen (Engaged)
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the group. Members must share `factions`.
    UnitGroup(const UnitGroupConfig& config, EntityFactionsMask factions);

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    void tick(WorldContext& ctx) override;

    // -------------------------------------------------------------------------
    // Membership (between ticks)
    // -------------------------------------------------------------------------

    // Adds `unit` (leaving its old group, if any) and gives it the Group
    // command. It keeps its place until the next layout, one tick later.
    // Returns false if the group is full.
    bool add_member(Unit& unit);

    // The unit leaves and stops. Same as giving it any command of its own.
    void remove_member(Unit& unit);

    // Moves every member of `other` into this group. `other` removes itself
    // on its next tick.
    void merge_from(World& world, UnitGroup& other);

    // -------------------------------------------------------------------------
    // Commands (between ticks)
    // -------------------------------------------------------------------------

    void issue(const UnitCommand& cmd);

    void stop       ()                    { issue(UnitCommand::stop());                }
    void move_to    (Position2D position) { issue(UnitCommand::move_to(position));     }
    void attack     (EntityHandle target) { issue(UnitCommand::attack(target));        }
    void attack_move(Position2D position) { issue(UnitCommand::attack_move(position)); }

    // Appends a waypoint to the current MoveTo or AttackMove route (a
    // shift-click). The whole group shares the route: once pathfinding
    // exists, a path is computed once per group and stored here.
    void add_waypoint(Position2D position);

    // -------------------------------------------------------------------------
    // Plan (read by members while ticking, by renderers between ticks)
    //
    // Pass the tick being processed (ctx.tick), or World::get_tick() between
    // ticks for the plan of the next one.
    // -------------------------------------------------------------------------

    const Plan& plan(Nezumi::Tick tick) const { return plans_[tick & 1u]; }

    // World position of slot `slot` in the plan for `tick`
    Position2D slot_position(Nezumi::Tick tick, uint16_t slot) const;

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const UnitGroupConfig& config()  const { return *config_; }
    const UnitCommand&     command() const { return command_; }

    // State chosen by the last tick (the next tick's plan)
    UnitGroupState state()       const { return state_;       }
    uint16_t       state_ticks() const { return state_ticks_; }

    // Member handles indexed by slot. Empty slots hold null handles.
    const std::vector<EntityHandle>& members() const { return members_; }

    uint16_t member_count()    const { return member_count_;    }
    uint16_t straggler_count() const { return straggler_count_; }

    // Root mean square of the members' distances from their slots (tiles).
    // RMS rather than mean so a few far stragglers weigh more than many
    // members slightly out of place.
    float    lag()             const { return lag_;             }

    // Remaining route, next waypoint first (MoveTo and AttackMove)
    std::span<const Position2D> waypoints() const {
        return std::span<const Position2D>(waypoints_).subspan(next_waypoint_);
    }

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    struct MemberStats {
        uint16_t   count     = 0;
        uint16_t   stragglers = 0;
        Position2D centroid  = {};
        Area2D     bounds    = {};   // world-space box around member positions
        float      lag       = 0.0f;   // RMS distance from slots
        float      min_speed = 0.0f; // slowest member's move_speed
        float      max_aggro = 0.0f; // largest member aggro_radius
    };

    // Drop members that died or left, and measure the rest against the
    // plan they are following this tick.
    MemberStats measure_members_(WorldContext& ctx);

    // Where the command leads (current waypoint or target), if anywhere.
    // Clears a dead Attack target.
    bool destination_(WorldContext& ctx, Position2D& out);

    // Nearest enemy within `stats.max_aggro` of the members, or null
    EntityHandle scan_(WorldContext& ctx, const MemberStats& stats) const;

    // Anchor velocity toward the route for the next tick, from `anchor`
    // (where this tick leaves it). Advances waypoints and turns the
    // formation; returns true if the turn is sharp enough to reform.
    bool march_(WorldContext& ctx, const MemberStats& stats, Position2D anchor, Plan& next);

    // Assign every member a slot facing `facing` into `offsets`
    void layout_(WorldContext& ctx, Position2D facing, std::vector<Position2D>& offsets);

    bool engages_() const { return command_.type != UnitCommandType::MoveTo; }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const UnitGroupConfig* config_ = nullptr;

    UnitCommand             command_       = {};
    std::vector<Position2D> waypoints_     = {};
    uint32_t                next_waypoint_ = 0;

    // Double-buffered plan: members read [tick & 1], the group writes the other
    std::array<Plan,                    2> plans_   = {};
    std::array<std::vector<Position2D>, 2> offsets_ = {}; // slot offsets: x right, y forward

    std::vector<EntityHandle> members_    = {}; // by slot; null = free
    std::vector<uint16_t>     free_slots_ = {};

    UnitGroupState state_             = UnitGroupState::Reforming;
    uint16_t       state_ticks_       = 0;
    uint16_t       ticks_since_enemy_ = UINT16_MAX;
    uint16_t       member_count_      = 0;
    uint16_t       straggler_count_   = 0;
    float          lag_               = 0.0f;
    bool           layout_dirty_      = true;

    // Layout scratch, reused to avoid allocation
    struct LayoutEntry {
        uint16_t slot;
        float    range;
        float    forward;
        float    lateral;
        uint32_t id; // handle index, the deterministic tie-break
    };
    std::vector<LayoutEntry> layout_scratch_ = {};
};

} // namespace Nightmourn
