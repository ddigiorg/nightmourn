// =============================================================================
// GUI/CraftRulesMenu.hpp
//
// The settlement's crafting rules (K, or click a workshop): what its
// machines keep in store, and what they must never use up
// (crafting-design/DESIGN.md §6.5).
//
//   [Make] [Reserves]
//   Workshops: 2 (1 working, 1 waiting for room)
//   [arrow ][house ][wall  ] ...     the slot: target top right, in store bottom right
//   Wooden arrow: 12 in store, 5 being made
//   Keep  [-10] [ 100      ] [+10]
//
// Make lists what its machines can make between them, A to Z; Reserves the
// ingredients those recipes use. The wheel over a slot steps its number by
// 10 (Shift: 100); or click a slot to pick it, then use the buttons (the
// same steps) or type the number.
//
// Like the Items menu it only asks: edits become CraftRuleChanges, which the
// game applies to the settlement's CraftSite in on_tick, so the rules only
// change in the fixed tick. The rules can be changed only inside the
// settlement (`editable`); elsewhere the menu just shows them.
//
//   rules_menu_.set_site(&s->crafting, &s->inventory, in_reach);  // each frame
//   menus_.draw(ui, flags, { ..., .rules = &rules_menu_ });
//   ...on_tick:
//   for (const CraftRuleChange& c : rules_menu_.take_changes()) c.apply(s->crafting);
// =============================================================================

#pragma once
#include "Nightmourn/Crafting/CraftSite.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <array>   // array
#include <cstdint> // uint8_t, uint32_t
#include <string>  // string
#include <vector>  // vector

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class Configs;
class Inventory;
struct ItemPrototype;
struct InventoryModifiers;

// An edit in the rules menu: a target for `item` (made by `recipe`), or a
// floor under it
struct CraftRuleChange {
    enum class Kind : std::uint8_t { Target, Floor };

    Kind          kind   = Kind::Target;
    const Recipe* recipe = nullptr; // Target
    ItemID        item   = {};
    std::uint32_t value  = 0;

    void apply(CraftSite& site) const {
        if (kind == Kind::Floor) site.set_floor(item, value);
        else if (recipe)         site.set_target(*recipe, item, value);
    }
};

class CraftRulesMenu {
public:

    // Steps of the - / + buttons (Shift: the big one)
    static constexpr std::uint32_t STEP     = 10;
    static constexpr std::uint32_t BIG_STEP = 100;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet` and `configs` must outlive the menu.
    // -------------------------------------------------------------------------

    CraftRulesMenu(const UISheet& sheet, const Configs& configs);

    // The site it shows and its storage, and whether its rules can be
    // changed from here. Call each frame; null: there's no settlement.
    void set_site(const CraftSite* site, const Inventory* storage, bool editable) {
        site_ = site; storage_ = storage; editable_ = editable;
    }

    // Edits since the last call, oldest first
    std::vector<CraftRuleChange> take_changes();

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw while the menu is open; its close button clears
    // `open`. `mods.move` (Shift) makes the buttons take big steps.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods);

private:
    enum class Tab : std::uint8_t { Make, Reserves };

    void list_();                     // what the tab shows, from the site's categories
    void draw_status_(Nezumi::UI::Context& ui);
    void draw_editor_(Nezumi::UI::Context& ui, const InventoryModifiers& mods);
    void tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item);
    std::uint32_t value_(ItemID item) const; // the tab's number for it
    void change_(ItemID item, std::uint32_t value);

    const UISheet*   sheet_    = nullptr;
    const Configs*   configs_  = nullptr;
    const CraftSite* site_     = nullptr;
    const Inventory* storage_  = nullptr;
    bool             editable_ = false;

    Tab    tab_      = Tab::Make;
    ItemID selected_ = {};

    std::vector<CraftRuleChange>      changes_ = {};
    std::vector<const ItemPrototype*> sorted_  = {}; // A to Z

    // Rebuilt every frame, kept to allocate nothing after the first
    std::vector<const ItemPrototype*> shown_  = {};
    std::vector<Nezumi::UI::Slot>     slots_  = {};
    std::vector<std::array<char, 12>> labels_ = {}; // per slot: the target or floor
    std::vector<std::array<char, 12>> counts_ = {}; // per slot: in store
    std::string                       field_  = {}; // the number being typed
    ItemID                            field_for_ = {};
    std::uint32_t                     field_was_ = 0;
    std::string                       tip_    = {};
};

} // namespace Nightmourn
