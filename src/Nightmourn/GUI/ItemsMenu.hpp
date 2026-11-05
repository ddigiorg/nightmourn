// =============================================================================
// GUI/ItemsMenu.hpp
//
// The Items menu (I): every item in the game in one grid, A to Z, and where
// items are crafted, like a recipe book. Hover an item for what it is and
// what it's made from; click one that has a recipe to craft it (right click:
// five), Ctrl + click to put it on the action bar.
//
//   [banner][barracks][crafter][food][gate][gold][house][stone][torch][wall]
//   [wood  ]
//
// Crafting takes the recipe (its item config's "recipe") from the
// settlement's stock first while it's in reach, then the player's, and puts
// the item in the player's inventory, all or nothing: with too little, or no
// room for the item, nothing changes. Like moving stacks, it happens right
// away, outside the World's sim. Crafting takes no time for now.
//
// Every item shows for now; later a slot opens once its item has been
// discovered.
//
//   items_menu_.set_settlement(stock_in_reach);   // each frame, null if none
//   menus_.draw(ui, flags, { ..., .items = &items_menu_ });
// =============================================================================

#pragma once
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <cstddef>     // size_t
#include <cstdint>     // uint32_t
#include <glm/glm.hpp> // vec2
#include <string>      // string
#include <vector>      // vector

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class ActionBar;
class Configs;
class Inventory;
struct ItemPrototype;
struct InventoryModifiers;

class ItemsMenu {
public:

    // How many a right click crafts
    static constexpr std::uint32_t CRAFT_MANY = 5;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet`, `configs` and `player` must outlive the menu.
    // -------------------------------------------------------------------------

    ItemsMenu(const UISheet& sheet, const Configs& configs, Inventory& player);

    // The settlement's stock while it's in reach, else null: crafting draws
    // on it first
    void set_settlement(Inventory* stock) { settlement_ = stock; }

    // Where Ctrl + click puts shortcuts; must outlive the menu
    void set_action_bar(ActionBar* bar) { action_bar_ = bar; }

    // -------------------------------------------------------------------------
    // Crafting
    // -------------------------------------------------------------------------

    // Crafts up to `count` of `item` (stopping when it runs short or out of
    // room). Returns how many it made.
    std::uint32_t craft(ItemID item, std::uint32_t count = 1);

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `mods` are the click modifiers (InventoryMenu's).
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods);

private:
    void tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item);

    const UISheet* sheet_      = nullptr;
    const Configs* configs_    = nullptr;
    Inventory*     player_     = nullptr;
    Inventory*     settlement_ = nullptr;
    ActionBar*     action_bar_ = nullptr;

    std::vector<const ItemPrototype*> sorted_ = {}; // A to Z

    // Rebuilt every frame, kept to allocate nothing after the first
    std::vector<Nezumi::UI::Slot> slots_ = {};
    std::string                   tip_   = {};
};

} // namespace Nightmourn
