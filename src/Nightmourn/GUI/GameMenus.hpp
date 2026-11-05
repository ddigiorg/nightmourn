// =============================================================================
// GUI/GameMenus.hpp
//
// The player's top-level menus, opened from the action bar's buttons or
// their hotkeys: skills, equipment, the player's inventory, items (a codex
// of every item type discovered, with its recipes), options, debug and the
// main menu. The debug menu, the main menu and (given an InventoryMenu) the
// inventory do something; the others open an empty panel saying what they
// will hold.
//
// The settlement's inventory (V, Action::ToggleSettlementMenu) is one more,
// with no action bar button: it can only be open while the game says it's
// available (the hero stands in the settlement's build area). Walking out
// hides it, and walking back in shows it again if it was open; closing it
// (its X, V or Escape) forgets that.
//
//   on_tick:  menus_.set_settlement_available(hero_in_build_area);
//             menus_.handle_input(engine.input());
//             if (menus_.quit_requested()) ...quit...
//   on_draw:  menus_.draw(*engine.ui(), debug_flags_, &player_menu_, &settlement_menu_);
//
// While both inventories are open, stacks move between them (InventoryMenu).
//
// Escape closes every open menu, or opens the main menu when none is open,
// like most games.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/GUI/DebugMenu.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nezumi/Input/ActionID.hpp"
#include <array>       // array
#include <string_view> // string_view

namespace Nezumi {
    class InputMap;
}

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

// Title of `menu` ("Inventory"), and the action that toggles it
std::string_view menu_title (Menu menu);
Nezumi::ActionID menu_action(Menu menu);

class InventoryMenu;

class GameMenus {
public:

    // -------------------------------------------------------------------------
    // Open / Close
    // -------------------------------------------------------------------------

    bool is_open (Menu menu) const;
    void set_open(Menu menu, bool open);
    void toggle  (Menu menu) { set_open(menu, !is_open(menu)); }
    bool any_open() const;
    void close_all();

    // Escape: close every open menu, or open the main menu if none was open
    void back();

    // -------------------------------------------------------------------------
    // Settlement Inventory
    // -------------------------------------------------------------------------

    // Whether it can be shown now; set every tick, before handle_input
    void set_settlement_available(bool available) { settlement_available_ = available; }
    bool settlement_available() const { return settlement_available_; }

    // Shown: the player opened it, and it's available
    bool settlement_open() const { return settlement_wanted_ && settlement_available_; }

    // Opens it (only while available) or closes it
    void set_settlement_open(bool open);
    void toggle_settlement() { set_settlement_open(!settlement_open()); }

    // -------------------------------------------------------------------------
    // Tick
    //
    // Call from on_tick: the menu hotkeys and Escape.
    // -------------------------------------------------------------------------

    void handle_input(const Nezumi::InputMap& input);

    // The main menu's Quit button was clicked
    bool quit_requested() const { return quit_; }

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw. Draws the open menus. Without an `inventory`, the
    // player's inventory menu is a placeholder; without a `settlement`, the
    // settlement's never shows.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, DebugFlags& flags, InventoryMenu* inventory = nullptr,
              InventoryMenu* settlement = nullptr);

private:
    void draw_stub_(Nezumi::UI::Context& ui, Menu menu, std::string_view about);
    void draw_main_(Nezumi::UI::Context& ui);

    std::array<bool, MENU_COUNT> open_  = {}; // all but Debug, which DebugMenu keeps
    DebugMenu                    debug_ = {};
    bool                         quit_  = false;

    bool settlement_wanted_    = false; // opened by the player (shown while available)
    bool settlement_available_ = false;
};

} // namespace Nightmourn
