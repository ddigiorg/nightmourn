// =============================================================================
// Input/Actions.hpp
//
// Nightmourn's input actions and their default key bindings. Game code reads
// actions from Nezumi's InputMap ("is MoveUp held?"), never keys, so a
// settings screen can rebind them later without touching gameplay code.
// =============================================================================

#pragma once
#include "Nezumi/Input/ActionID.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"

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

    // Debug flag toggles: DebugToggleFirst + DebugFlag, one per flag
    inline constexpr Nezumi::ActionID DebugToggleFirst = 100;
}

// The action that toggles `flag`
constexpr Nezumi::ActionID debug_toggle_action(DebugFlag flag) {
    return Nezumi::ActionID(static_cast<Nezumi::ActionID::value_t>(
        Action::DebugToggleFirst.value() + static_cast<Nezumi::ActionID::value_t>(flag)));
}

// Bind every action to its default keys: W/A/S/D for movement, and F1, F2,
// ... for the debug flags in DebugFlag order (until a debug menu exists)
void bind_default_actions(Nezumi::InputMap& input);

// Toggle each debug flag whose action was pressed. Call from on_tick, where
// presses are reported (see InputMap).
void toggle_debug_flags(const Nezumi::InputMap& input, DebugFlags& flags);

} // namespace Nightmourn
