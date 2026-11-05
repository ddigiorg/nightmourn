// =============================================================================
// GUI/GameMenus.hpp
//
// The player's top-level menus, opened from the action bar's buttons or
// their hotkeys: the character (skills and equipment), the player's inventory, the
// settlement's inventory, items (every item in the game, and crafting),
// the settlement's crafting rules, options, debug and the Main Menu. Options and debug have no action bar
// button: the main menu opens options, and the tilde key the debug menu.
// The debug menu, the main menu and (given their panels) the inventories,
// items and crafting rules do something; the others open an empty panel saying what they
// will hold.
//
// The player opens and closes the settlement's inventory like any other.
// While its stock is out of reach (the hero is outside the build area, or
// there's no settlement) it stays open and says so instead (InventoryMenu's
// note).
//
//   on_tick:  menus_.handle_input(engine.input());
//             if (menus_.quit_requested()) ...quit...
//   on_draw:  menus_.draw(*engine.ui(), debug_flags_,
//                         { .inventory = &player_menu_, .settlement = &settlement_menu_,
//                           .items = &items_menu_ });
//
// While both inventories are open and in reach, stacks move between them
// (InventoryMenu).
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

class CraftRulesMenu;
class InventoryMenu;
class ItemsMenu;

// The panels GameMenus draws for the menus that do something. Any left
// null shows a placeholder.
struct GameMenuPanels {
    InventoryMenu* inventory  = nullptr; // Menu::Inventory
    InventoryMenu* settlement = nullptr; // Menu::Settlement
    ItemsMenu*     items      = nullptr; // Menu::Items
    CraftRulesMenu* rules     = nullptr; // Menu::Crafting
};

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
    // Call from on_draw. Draws the open menus.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, DebugFlags& flags, const GameMenuPanels& panels = {});

private:
    void draw_stub_(Nezumi::UI::Context& ui, Menu menu, std::string_view about);
    void draw_main_(Nezumi::UI::Context& ui);

    std::array<bool, MENU_COUNT> open_  = {}; // all but Debug, which DebugMenu keeps
    DebugMenu                    debug_ = {};
    bool                         quit_  = false;
    bool                         move_mod_     = false; // Shift held (InventoryMenu)
    bool                         shortcut_mod_ = false; // Ctrl held
};

} // namespace Nightmourn
