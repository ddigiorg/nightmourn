// =============================================================================
// GUI/ActionBar.hpp
//
// The player's action bar, centered along the bottom of the screen, after
// World of Warcraft's: two stone eagles flanking a stone slab that holds a
// thin hp bar, a row of item slots, and the menu buttons.
//
//   [eagle] [ hp bar ...................................... ] [eagle]
//           [ 1 ][ 2 ][ 3 ] ... [ 0 ]  |  [sk][eq][in][it][op][db][mm]
//
// Slots hold items (ItemID, empty for none). Each slot is an input
// action (Action::use_slot), shown by its key in the slot's corner, so
// rebinding a key relabels its slot. The player uses a slot with its key or
// a click, rearranges slots by dragging icons between them, and empties one
// by dragging its icon off the bar. The menu buttons toggle GameMenus.
//
// A slot is a shortcut to an item, not the item itself (Factorio's
// quickbar): dragging a stack from the inventory onto a slot points the slot
// at that item, and the stack stays in the inventory. Given the inventory,
// each slot shows how many of its item the player carries, and dims when
// there are none left.
//
//   on_tick:  action_bar_.handle_input(engine.input());
//             for (std::size_t slot : action_bar_.take_used()) ...use it...
//   on_draw:  action_bar_.draw(*engine.ui(), engine.input(), menus_, hero, &inventory);
//
// Fixed size, in UI points: it grows with the display scale like the rest
// of the UI, not with the window.
// =============================================================================

#pragma once
#include "Nightmourn/GUI/UIAtlas.hpp"
#include "Nezumi/UI/Rect.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <array>    // array
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t, uint32_t
#include <optional> // optional
#include <span>     // span
#include <vector>   // vector

namespace Nezumi {
    class InputMap;
}

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class Character;
class GameMenus;
class Inventory;

class ActionBar {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr std::size_t SLOT_COUNT = 10;

    // UI drag type of anything that can go on the bar (items for now). The
    // inventory menu drags with it too, so its items drop onto slots.
    static constexpr std::uint32_t DRAG_TYPE = 1;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `atlas` must outlive the bar.
    // -------------------------------------------------------------------------

    explicit ActionBar(const UIAtlas& atlas) : atlas_(&atlas) {}

    // -------------------------------------------------------------------------
    // Slots
    // -------------------------------------------------------------------------

    std::optional<ItemID> slot(std::size_t i) const { return slots_[i]; }
    void set_slot(std::size_t i, std::optional<ItemID> item) { slots_[i] = item; }

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    // Call from on_tick: slots whose key was pressed are used
    void handle_input(const Nezumi::InputMap& input);

    // Slots used since the last call, by key or click, oldest first. Empty
    // slots are never reported.
    std::vector<std::size_t> take_used();

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw. `hero` is null while the hero is dead. Clicking a
    // menu button toggles its menu in `menus`. With an `inventory`, slots
    // show their item's count in it.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, const Nezumi::InputMap& input,
              GameMenus& menus, const Character* hero,
              const Inventory* inventory = nullptr);

    // Where the bar was last drawn, UI units (empty before its first draw)
    Nezumi::UI::Rect rect() const { return rect_; }

private:
    void use_(std::size_t i);
    void draw_hp_bar_  (Nezumi::UI::Context& ui, const Nezumi::UI::Rect& r, const Character* hero);
    void draw_slots_   (Nezumi::UI::Context& ui, const Nezumi::InputMap& input, const Inventory* inventory);
    void draw_buttons_ (Nezumi::UI::Context& ui, const Nezumi::InputMap& input, GameMenus& menus);

    const UIAtlas* atlas_ = nullptr;

    std::array<std::optional<ItemID>, SLOT_COUNT> slots_  = {};
    std::array<std::uint8_t, SLOT_COUNT>                  flash_  = {}; // frames left drawn pressed
    std::vector<std::size_t>                              used_   = {};
    std::array<std::array<char, 8>, SLOT_COUNT>           counts_ = {}; // slot count text, for Slot::count

    std::uint16_t    last_max_hp_ = 0; // shown while the hero is dead
    Nezumi::UI::Rect rect_        = {};
};

} // namespace Nightmourn
