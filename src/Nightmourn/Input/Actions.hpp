// =============================================================================
// Input/Actions.hpp
//
// Nightmourn's input actions and their default key bindings. Game code reads
// actions from Nezumi's InputMap ("is MoveUp held?"), never keys, so a
// settings screen can rebind them later without touching gameplay code.
// =============================================================================

#pragma once
#include "Nezumi/Input/ActionID.hpp"
#include <cstddef> // size_t
#include <cstdint> // uint16_t

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

    // Menus (see GUI/GameMenus.hpp)
    inline constexpr Nezumi::ActionID ToggleDebugMenu     = 4;
    inline constexpr Nezumi::ActionID ToggleCharacterMenu = 5; // skills and equipment
    // 6: free (was the equipment menu, now part of the character menu)
    inline constexpr Nezumi::ActionID ToggleInventoryMenu = 7;
    inline constexpr Nezumi::ActionID ToggleItemsMenu     = 8;
    inline constexpr Nezumi::ActionID ToggleOptionsMenu   = 9;
    inline constexpr Nezumi::ActionID ToggleMainMenu      = 10; // Escape: also closes open menus

    // Swing at the resource under the cursor (held: keep swinging)
    inline constexpr Nezumi::ActionID Mine = 11;

    // The settlement's inventory (GameMenus: only inside its build area)
    inline constexpr Nezumi::ActionID ToggleSettlementMenu = 12;

    // Held while clicking an inventory stack: Shift sends it to the other
    // inventory open beside it, Ctrl puts it on the action bar
    inline constexpr Nezumi::ActionID InventoryMoveMod     = 13;
    inline constexpr Nezumi::ActionID InventoryShortcutMod = 14;

    // Action bar slots: one action per slot, contiguous from UseSlotFirst, so
    // a bar that grows (MAX_ACTION_SLOTS) needs no new ids
    inline constexpr std::size_t      MAX_ACTION_SLOTS = 48;
    inline constexpr std::uint16_t    UseSlotFirst     = 16;

    constexpr Nezumi::ActionID use_slot(std::size_t slot) {
        return Nezumi::ActionID{ static_cast<std::uint16_t>(UseSlotFirst + slot) };
    }
}

// Bind every action to its default keys: W/A/S/D for movement, the left
// mouse button for mining, 1-9 and 0 for
// the first ten action bar slots, C / B / I / O for the character,
// inventory, items and options menus, V for the settlement's inventory,
// Shift and Ctrl (either side) as the inventory click modifiers, Escape for
// the Main Menu,
// and the tilde key (the one left of 1, whatever the layout prints on it)
// for the debug menu
void bind_default_actions(Nezumi::InputMap& input);

} // namespace Nightmourn
