// =============================================================================
// GUI/SettlementMenu.hpp
//
// The Settlement menu (V, its action bar button, or a click on the banner
// or a workshop), with tabs along the top, Colony Survival style:
//
// - Inventory: the settlement's stock, the same slots as the player's
//   Inventory (InventoryMenu), 10 wide and scrolling.
// - Crafting: what the settlement makes (Crafting/CraftSite), a row per
//   item its machines can make and per ingredient those recipes use:
//
//     [icon]  [====|=======|======]  [ 20 ]  [ 500 ]  [ 0  ]
//             min . . . . . . . max    Min      Max   Priority
//
//   min: never used as an ingredient below it. max: made while there are
//   fewer (0: not made). priority: 0..255, higher first. Drag the
//   slider's two grabs, or type either end. Items it can't make (raw
//   materials) only have a min. Hover an icon for the item: what makes it
//   and from what, how many are in store and being made.
//
// Like the Items menu it only asks: edits become SettlementCraftEdits,
// which the game applies to the settlement's CraftSite in on_tick, so the
// limits only change in the fixed tick. They can be changed only inside
// the settlement (`editable`); elsewhere the tab just shows them.
//
//   settlement_menu_.set_site(&s->crafting, &s->inventory, in_reach); // each frame
//   settlement_menu_.inventory().set_inventory(in_reach ? &s->inventory : nullptr);
//   menus_.draw(ui, flags, { ..., .settlement = &settlement_menu_ });
//   ...on_tick:
//   for (const SettlementCraftEdit& e : settlement_menu_.take_edits()) e.apply(s->crafting);
// =============================================================================

#pragma once
#include "Nightmourn/Crafting/CraftSite.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <array>       // array
#include <cstdint>     // uint32_t
#include <string>      // string
#include <string_view> // string_view
#include <vector>      // vector

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class Configs;
struct ItemPrototype;

// An edit in the Crafting tab: `item`'s limits, all three at once. `recipe`
// is what makes it (null: it can't be made here, so only `min` counts).
struct SettlementCraftEdit {
    const Recipe* recipe = nullptr;
    ItemID        item   = {};
    CraftLimits   limits = {};

    void apply(CraftSite& site) const {
        site.set_min(item, limits.min);
        site.set_priority(item, limits.priority);
        if (recipe) site.set_max(*recipe, item, limits.max);
    }
};

class SettlementMenu {
public:
    enum Tab : int { InventoryTab = 0, CraftingTab = 1 };

    // The panel's title, and so its id (Context::set_panel_tab)
    static constexpr std::string_view TITLE = "Settlement";

    // The sliders' and fields' range
    static constexpr int LIMIT_MAX = 65535;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet` and `configs` must outlive the menu.
    // -------------------------------------------------------------------------

    SettlementMenu(const UISheet& sheet, const Configs& configs);

    // The Inventory tab's slots: set its inventory (null while out of
    // reach, with a note), take its picked stacks
    InventoryMenu&       inventory()       { return inventory_; }
    const InventoryMenu& inventory() const { return inventory_; }

    // The Crafting tab's site and its storage, and whether its limits can
    // be changed from here. Call each frame; null: there's no settlement.
    void set_site(const CraftSite* site, const Inventory* storage, bool editable) {
        site_ = site; storage_ = storage; editable_ = editable;
    }

    // Edits since the last call, oldest first
    std::vector<SettlementCraftEdit> take_edits();

    // Opens on `tab` next time it's drawn
    void show_tab(Nezumi::UI::Context& ui, Tab tab);

    // The tab it showed last (GameMenus: stacks only move between the
    // inventories while the Inventory tab is up)
    Tab tab() const { return tab_; }

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `other` is the player's inventory menu if it's open beside
    // it (stacks move between them); `mods` are the click modifiers.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, InventoryMenu* other, const InventoryModifiers& mods);

private:
    // A row's limits as shown and typed: what was asked for until the tick
    // applies it, else the site's
    struct RowText {
        std::string   min = {}, max = {};
        CraftLimits   want = {}; // shown
        CraftLimits   seen = {}; // the site's, last frame
        bool          synced = false;
    };

    static bool same_(const CraftLimits& a, const CraftLimits& b) {
        return a.min == b.min && a.max == b.max && a.priority == b.priority;
    }

    void list_();
    void draw_crafting_(Nezumi::UI::Context& ui);
    void draw_row_(Nezumi::UI::Context& ui, const ItemPrototype& item, float y, bool makes);
    void tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item, bool makes);
    bool makes_here_(const ItemPrototype& item) const;

    const UISheet*   sheet_    = nullptr;
    const Configs*   configs_  = nullptr;
    InventoryMenu    inventory_;
    const CraftSite* site_     = nullptr;
    const Inventory* storage_  = nullptr;
    bool             editable_ = false;
    Tab              tab_      = InventoryTab;

    std::vector<SettlementCraftEdit>  edits_  = {};
    std::vector<const ItemPrototype*> sorted_ = {}; // A to Z
    std::vector<const ItemPrototype*> shown_  = {}; // the Crafting tab's rows, rebuilt each frame
    std::vector<RowText>              text_   = {}; // by ItemID
    std::string                       tip_    = {};
    std::array<char, 12>              count_  = {};
};

} // namespace Nightmourn
