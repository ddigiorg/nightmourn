// =============================================================================
// Character/CharacterCommand.hpp
//
// What a character is told to do this tick. Whoever controls the character
// (a Player from input, later an AI) writes it between ticks; the character
// carries it out while ticking.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityID.hpp"
#include "Nezumi/World/Physics/Position2D.hpp"

namespace Nightmourn {

struct CharacterCommand {
    // Direction to walk, length 0 (stand) to 1 (full speed)
    Nezumi::Position2D move_dir = {};

    // Resource to swing at (a tree): mined whenever the swing is off
    // cooldown and the resource is within CharacterConfig::mine_reach. Null:
    // don't mine. A Player sets it while the mine button is held over one.
    Nezumi::EntityHandle mine_target = {};

    // TODO: attack, use item, cast, interact
};

} // namespace Nightmourn
