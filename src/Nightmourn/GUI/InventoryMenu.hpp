// =============================================================================
// GUI/InventoryMenu.hpp
//
// The player's inventory panel (B, or the action bar's bag button): the
// inventory's slots in a grid, Player::INVENTORY_COLUMNS wide, each stack
// showing its count in the bottom-right corner like Factorio's.
//
//   [wood ][stone][     ][     ] ...
//   [     ][     ][     ][     ] ...
//
// Dragging a stack onto another slot moves it there: into an empty slot,
// onto a stack of the same item (as much as fits), or swapped with another
// item. Dragging a stack onto the action bar makes that slot a shortcut to
// the item (ActionBar::DRAG_TYPE); the stack itself stays here. A stack
// dropped anywhere else stays where it was, until items can be dropped on
// the ground.
//
// GameMenus opens and closes it with the other menus:
//
//   menus_.draw(*engine.ui(), debug_flags_, &inventory_menu_);
// =============================================================================

#pragma once
#include "Nightmourn/GUI/UIAtlas.hpp"
#include <array>   // array
#include <cstddef> // size_t
#include <vector>  // vector

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class Inventory;

class InventoryMenu {
public:

    // -------------------------------------------------------------------------
    // Construction
    //
    // `atlas` and `inventory` must outlive the menu.
    // -------------------------------------------------------------------------

    InventoryMenu(const UIAtlas& atlas, Inventory& inventory);

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. Moving stacks changes the inventory right away: it isn't part
    // of the World's sim.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open);

private:
    const UIAtlas* atlas_     = nullptr;
    Inventory*     inventory_ = nullptr;

    // Rebuilt every frame, kept to allocate nothing after the first
    std::vector<Nezumi::UI::Slot>    slots_  = {};
    std::vector<std::array<char, 8>> counts_ = {}; // slot count text, for Slot::count
};

} // namespace Nightmourn
