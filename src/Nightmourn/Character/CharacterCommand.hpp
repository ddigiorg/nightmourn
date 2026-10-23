// =============================================================================
// Character/CharacterCommand.hpp
//
// What a character is told to do this tick. Whoever controls the character
// (a Player from input, later an AI) writes it between ticks; the character
// carries it out while ticking.
// =============================================================================

#pragma once
#include "Nezumi/World/Physics/Position2D.hpp"

namespace Nightmourn {

struct CharacterCommand {
    // Direction to walk, length 0 (stand) to 1 (full speed)
    Nezumi::Position2D move_dir = {};

    // TODO: attack, use item, cast, interact
};

} // namespace Nightmourn
