// =============================================================================
// Input/Actions.hpp
//
// Nightmourn's input actions and their default key bindings. Game code reads
// actions from Nezumi's InputMap ("is MoveUp held?"), never keys, so a
// settings screen can rebind them later without touching gameplay code.
// =============================================================================

#pragma once
#include "Nezumi/Input/ActionID.hpp"

namespace Nezumi {
    class InputMap;
}

namespace Nightmourn {

namespace Action {
    // Character movement
    inline constexpr Nezumi::ActionID MoveLeft  = 0;
    inline constexpr Nezumi::ActionID MoveRight = 1;
    inline constexpr Nezumi::ActionID MoveDown  = 2;
    inline constexpr Nezumi::ActionID MoveUp    = 3;

    // Menus
    inline constexpr Nezumi::ActionID ToggleDebugMenu = 4;
}

// Bind every action to its default keys: W/A/S/D for movement, and the tilde
// key (the one left of 1, whatever the layout prints on it) for the debug menu
void bind_default_actions(Nezumi::InputMap& input);

} // namespace Nightmourn
