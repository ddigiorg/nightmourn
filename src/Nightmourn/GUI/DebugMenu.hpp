// =============================================================================
// GUI/DebugMenu.hpp
//
// Debug options menu, like Factorio's F4 menu: one checkbox per DebugFlag,
// listed by its kebab-case name. Opened and closed with the tilde key (see
// Action::ToggleDebugMenu), the action bar's debug button, or the X in its
// title bar, and moved by dragging the title. It sizes itself to its
// checkboxes, so it can't be resized. GameMenus owns it with the other menus:
//
//   on_tick:  if (input.was_pressed(Action::ToggleDebugMenu)) debug_menu_.toggle();
//   on_draw:  ... draw the world with debug_flags_ ...
//             debug_menu_.draw(*engine.ui(), debug_flags_);
//
// Drawn after the world, so a flag ticked this frame shows from the next
// frame on, and the drawers always see one consistent set of flags.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DebugFlags.hpp"

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class DebugMenu {
public:

    // -------------------------------------------------------------------------
    // Open / Close
    // -------------------------------------------------------------------------

    bool is_open() const      { return open_;   }
    void set_open(bool open)  { open_ = open;   }
    void toggle()             { open_ = !open_; }

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw. Draws nothing while closed.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, DebugFlags& flags);

private:
    bool open_ = false;
};

} // namespace Nightmourn
