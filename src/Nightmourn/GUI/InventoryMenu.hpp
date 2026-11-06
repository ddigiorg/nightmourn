// =============================================================================
// GUI/InventoryMenu.hpp
//
// A panel showing one Inventory's slots in a grid, each stack with its count
// in the bottom-right corner like Factorio's. The player's inventory (B) and
// a settlement's stock (V) are both InventoryMenus, set up by their Config:
//
//   InventoryMenu player    (sheet, player.inventory(), { .title = "Player Inventory" });
//   InventoryMenu settlement(sheet, nullptr,            { .title = "Settlement Inventory",
//                                                         .shortcuts = false });
//
//   [wood ][stone][     ][     ] ...
//   [     ][     ][     ][     ] ...
//
// The grid is `columns` wide and shows `visible_rows` rows; an inventory
// with more rows than that scrolls (mouse wheel, with a scrollbar), so one
// that grows later needs no new layout.
//
// What the player can do with a stack:
//
// - Drag it onto another slot of the same grid: into an empty slot, onto a
//   stack of the same item (as much as fits), or swapped with another item.
// - Drag it onto a slot of the `other` menu (the one open beside it): the
//   same, between the two inventories (Inventory::move).
// - Shift + click it to send the whole stack to the other inventory, Shift +
//   right click to send half (Inventory::transfer). Whatever doesn't fit
//   stays.
// - Click it to pick it: take_picked() hands the game its item, e.g. to
//   start placing the building it is (Factorio's picking a stack up).
// - Drag it onto the action bar to make that slot a shortcut to the item
//   (ActionBar::DRAG_TYPE), or Ctrl + click it to put it in the bar's
//   first empty slot. Only from a menu with `shortcuts`: the action bar is
//   for the player's own items.
//
// More rows than fit scroll a whole row at a time, with the mouse wheel or
// by dragging the scrollbar.
//
// With no inventory (a settlement's, while it's out of reach) the panel
// stays the same size and shows its note instead.
//
// A stack dropped anywhere else stays where it was, until items can be
// dropped on the ground.
//
// GameMenus opens and closes them with the other menus:
//
//   menus_.draw(*engine.ui(), debug_flags_, &player_menu_, &settlement_menu_);
// =============================================================================

#pragma once
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <optional>    // optional
#include <utility>     // exchange
#include <array>       // array
#include <cstddef>     // size_t
#include <cstdint>     // uint32_t
#include <glm/glm.hpp> // vec2
#include <string_view> // string_view
#include <vector>      // vector

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class ActionBar;
class Inventory;

// Modifier keys held this frame, for an InventoryMenu's clicks (GameMenus
// reads them from input)
struct InventoryModifiers {
    bool move     = false; // Shift: to the other inventory
    bool shortcut = false; // Ctrl: onto the action bar
};

class InventoryMenu {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    // Same slots as the action bar's, so an icon dragged between them
    // doesn't change size
    static constexpr glm::vec2 SLOT_SIZE = { 36.0f, 36.0f };
    static constexpr float     SLOT_GAP  = 3.0f;

    // A panel showing `rows` rows of `columns` slots (and scrolling the
    // rest), title bar and padding included
    static glm::vec2 panel_size(const Nezumi::UI::Context& ui, std::size_t columns, std::size_t rows);

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        std::string_view title        = "Inventory"; // also its panel's id: unique
        std::size_t      columns      = 10;
        std::size_t      visible_rows = 4;           // more rows scroll

        // Where it first opens: `anchor` is a point on the screen as a
        // fraction of its size, and `pivot` the point of the panel put there
        glm::vec2 anchor = { 0.5f, 0.5f };
        glm::vec2 pivot  = { 0.5f, 0.5f };

        // Its stacks can be dragged onto the action bar as shortcuts
        bool shortcuts = true;
    };

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet` and `inventory` must outlive the menu (or `inventory` be
    // swapped out first with set_inventory). A null inventory shows the
    // note (set_note).
    // -------------------------------------------------------------------------

    InventoryMenu(const UISheet& sheet, Inventory* inventory, const Config& config);
    InventoryMenu(const UISheet& sheet, Inventory& inventory, const Config& config)
        : InventoryMenu(sheet, &inventory, config) {}

    // -------------------------------------------------------------------------
    // Inventory
    // -------------------------------------------------------------------------

    Inventory* inventory() const { return inventory_; }
    void set_inventory(Inventory* inventory) { inventory_ = inventory; }

    // Shown instead of the slots while there's no inventory; '\n' breaks
    // lines. Must outlive the menu (a string literal).
    void set_note(std::string_view note) { note_ = note; }

    // The item of the stack the player plainly clicked since the last call
    std::optional<ItemID> take_picked() { return std::exchange(picked_, std::nullopt); }

    const Config& config() const { return config_; }

    // Where Ctrl + click puts shortcuts (a menu with `shortcuts` only).
    // Must outlive the menu, or be cleared first.
    void set_action_bar(ActionBar* bar) { action_bar_ = bar; }

    using Modifiers = InventoryModifiers;

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `other` is the inventory menu open beside it, if any: stacks
    // move to and from it. Moving stacks changes the inventories right away:
    // it isn't part of the World's sim.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, InventoryMenu* other = nullptr,
              Modifiers mods = {});

    // The same inside a panel someone else began (a tab of the Settlement
    // menu): the slots, or the note, at the layout cursor
    void draw_contents(Nezumi::UI::Context& ui, InventoryMenu* other = nullptr, Modifiers mods = {});

private:
    const UISheet* sheet_     = nullptr;
    Inventory*     inventory_ = nullptr;
    Config         config_    = {};
    ActionBar*     action_bar_ = nullptr;
    std::uint32_t  grid_id_   = 0; // its slot grid's UI id, once drawn
    std::string_view      note_   = {};
    std::optional<ItemID> picked_ = {};

    // Rebuilt every frame, kept to allocate nothing after the first
    std::vector<Nezumi::UI::Slot>    slots_  = {};
    std::vector<std::array<char, 8>> counts_ = {}; // slot count text, for Slot::count
};

} // namespace Nightmourn
