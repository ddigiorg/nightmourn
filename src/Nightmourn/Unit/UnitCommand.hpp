// =============================================================================
// UnitCommand.hpp
//
// What a unit was told to do. A command persists across ticks until it
// completes or is replaced; the unit's UnitState is what it does about it
// this tick. A UnitGroup accepts the same commands (except Group) for its
// whole formation.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include <cstdint> // uint8_t

namespace Nightmourn {

enum class UnitCommandType : std::uint8_t {
    Stop,       // stand still; engage enemies inside aggro_radius
    MoveTo,     // walk to `position`, ignoring enemies
    Attack,     // chase and attack `target` until it dies
    AttackMove, // walk to `position`, engaging enemies inside aggro_radius
    Group,      // follow the unit's UnitGroup (set by UnitGroup::add_member)
};

struct UnitCommand {
    UnitCommandType type     = UnitCommandType::Stop;
    Position2D      position = {}; // MoveTo, AttackMove
    EntityHandle    target   = {}; // Attack

    static constexpr UnitCommand stop() {
        return {};
    }

    static constexpr UnitCommand move_to(Position2D pos) {
        return { UnitCommandType::MoveTo, pos, {} };
    }

    static constexpr UnitCommand attack(EntityHandle tgt) {
        return { UnitCommandType::Attack, {}, tgt };
    }

    static constexpr UnitCommand attack_move(Position2D pos){
        return { UnitCommandType::AttackMove, pos, {} };
    }
};

} // namespace Nightmourn
