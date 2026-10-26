// =============================================================================
// GUI/GameMenus.hpp
//
// The player's top-level menus, opened from the action bar's buttons or
// their hotkeys: skills, equipment, inventory, items (a codex of every item
// type discovered, with its recipes), options, debug and the main menu.
// Only the debug menu and the main menu do anything yet; the others open an
// empty panel saying what they will hold.
//
//   on_tick:  menus_.handle_input(engine.input());
//             if (menus_.quit_requested()) ...quit...
//   on_draw:  menus_.draw(*engine.ui(), debug_flags_);
//
// Escape closes every open menu, or opens the main menu when none is open,
// like most games.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/GUI/DebugMenu.hpp"
#include "Nightmourn/GUI/UIAtlas.hpp"
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

    void draw(Nezumi::UI::Context& ui, DebugFlags& flags);

private:
    void draw_stub_(Nezumi::UI::Context& ui, Menu menu, std::string_view about);
    void draw_main_(Nezumi::UI::Context& ui);

    std::array<bool, MENU_COUNT> open_  = {}; // all but Debug, which DebugMenu keeps
    DebugMenu                    debug_ = {};
    bool                         quit_  = false;
};

} // namespace Nightmourn
