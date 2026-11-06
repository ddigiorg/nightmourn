// =============================================================================
// GUI/ItemsMenu.hpp
//
// The Crafting menu (C): every item in the game in one grid, A to Z, and how
// each is made, like a recipe book. Hover an item for what it is, what it's
// made from, how long it takes and where it can be made; click one to queue
// crafting it, right click for five, Shift + click for as many as you can
// afford; Ctrl + click puts it on the action bar.
//
//   [Search                                                                     ]
//   [banner][barracks][food][gate][gold][house][stone][torch][wall][wooden-arrow]
//   [wood  ][workshop]
//
// The search bar above the grid shows only the items whose names hold what's
// typed, ignoring case.
//
// The menu only asks: clicks become CraftRequests, which the game hands to
// the player's CraftQueue in on_tick (it pays, and says no when it can't),
// so crafting stays in the fixed tick. An item is dimmed when one more
// can't be queued where the player stands: wrong place, too few
// ingredients, or no room for it. Its tooltip says which.
//
// Every item shows for now; later only the discovered ones.
//
//   items_menu_.set_crafting(here, &player_.crafting()); // each frame
//   menus_.draw(ui, flags, { ..., .items = &items_menu_ });
//   ...on_tick:
//   for (const CraftRequest& r : items_menu_.take_requests())
//       player_.crafting().enqueue(*r.recipe, r.count, here);
// =============================================================================

#pragma once
#include "Nightmourn/Crafting/CraftQueue.hpp"
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
struct ItemPrototype;
struct InventoryModifiers;

// A click in the Crafting menu: queue `count` crafts of `recipe`
// (CraftQueue::ALL: as many as can be paid for)
struct CraftRequest {
    const Recipe* recipe = nullptr;
    std::uint32_t count  = 1;
};

class ItemsMenu {
public:

    // How many a right click queues
    static constexpr std::uint32_t CRAFT_MANY = 5;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet` and `configs` must outlive the menu.
    // -------------------------------------------------------------------------

    ItemsMenu(const UISheet& sheet, const Configs& configs);

    // Where the player is crafting from and their queue, for dimming what
    // can't be queued and saying why. Call each frame; `queue` must
    // outlive the menu (null: nothing is dimmed).
    void set_crafting(const CraftPlace& place, const CraftQueue* queue) { place_ = place; queue_ = queue; }

    // Where Ctrl + click puts shortcuts; must outlive the menu
    void set_action_bar(ActionBar* bar) { action_bar_ = bar; }

    // -------------------------------------------------------------------------
    // Crafting
    // -------------------------------------------------------------------------

    // Crafts asked for since the last call, oldest first
    std::vector<CraftRequest> take_requests();

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `mods` are the click modifiers (InventoryMenu's).
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods);

private:
    void tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item);

    const UISheet*    sheet_      = nullptr;
    const Configs*    configs_    = nullptr;
    CraftPlace        place_      = {};
    const CraftQueue* queue_      = nullptr;
    ActionBar*        action_bar_ = nullptr;

    std::vector<CraftRequest> requests_ = {};

    std::vector<const ItemPrototype*> sorted_ = {}; // A to Z
    std::string                       search_ = {}; // the search bar's text


    // Rebuilt every frame, kept to allocate nothing after the first: the
    // items whose names match the search, and their slots
    std::vector<const ItemPrototype*> shown_ = {};
    std::vector<Nezumi::UI::Slot> slots_ = {};
    std::string                   tip_   = {};
};

} // namespace Nightmourn
