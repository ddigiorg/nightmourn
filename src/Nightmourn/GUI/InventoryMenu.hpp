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
// - Click it to send the whole stack to the other inventory, right click to
//   send half (Inventory::transfer). Whatever doesn't fit stays.
// - Drag it onto the action bar to make that slot a shortcut to the item
//   (ActionBar::DRAG_TYPE), only from a menu with `shortcuts`: the action
//   bar is for the player's own items.
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

class Inventory;

class InventoryMenu {
public:

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
    // swapped out first with set_inventory). A null inventory draws nothing.
    // -------------------------------------------------------------------------

    InventoryMenu(const UISheet& sheet, Inventory* inventory, const Config& config);
    InventoryMenu(const UISheet& sheet, Inventory& inventory, const Config& config)
        : InventoryMenu(sheet, &inventory, config) {}

    // -------------------------------------------------------------------------
    // Inventory
    // -------------------------------------------------------------------------

    Inventory* inventory() const { return inventory_; }
    void set_inventory(Inventory* inventory) { inventory_ = inventory; }

    const Config& config() const { return config_; }

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `other` is the inventory menu open beside it, if any: stacks
    // move to and from it. Moving stacks changes the inventories right away:
    // it isn't part of the World's sim.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, InventoryMenu* other = nullptr);

private:
    const UISheet* sheet_     = nullptr;
    Inventory*     inventory_ = nullptr;
    Config         config_    = {};
    std::uint32_t  grid_id_   = 0; // its slot grid's UI id, once drawn

    // Rebuilt every frame, kept to allocate nothing after the first
    std::vector<Nezumi::UI::Slot>    slots_  = {};
    std::vector<std::array<char, 8>> counts_ = {}; // slot count text, for Slot::count
};

} // namespace Nightmourn
