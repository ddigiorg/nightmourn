// =============================================================================
// GUI/ItemsMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/ItemsMenu.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/Item/Inventory.hpp" // can_add, pay, shortfall
#include "Nightmourn/Item/Items.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // sort
#include <cctype>    // toupper
#include <cstdio>    // snprintf

namespace Nightmourn {

namespace {
    constexpr std::size_t COLUMNS      = 10;
    constexpr std::size_t VISIBLE_ROWS = 4;

    std::string capitalized(std::string_view name) {
        std::string s(name);
        if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        return s;
    }
}

ItemsMenu::ItemsMenu(const UISheet& sheet, const Configs& configs, Inventory& player)
    : sheet_(&sheet), configs_(&configs), player_(&player)
{
    for (const ItemPrototype& item : configs.items) sorted_.push_back(&item);
    std::sort(sorted_.begin(), sorted_.end(),
              [](const ItemPrototype* a, const ItemPrototype* b) { return a->name < b->name; });
    slots_.resize(sorted_.size());
}

// -----------------------------------------------------------------------------
// Crafting
// -----------------------------------------------------------------------------

std::uint32_t ItemsMenu::craft(ItemID item, std::uint32_t count) {
    const ItemPrototype* p = item.value() < configs_->items.size() ? &configs_->items[item.value()] : nullptr;
    if (!p || p->recipe.empty()) return 0;

    Inventory* const from[] = { settlement_, player_ };
    std::uint32_t made = 0;
    for (; made < count; made++) {
        // Room first, so the recipe is only spent on an item that fits. An
        // ingredient used up can free its slot, but needing that would only
        // matter for a full inventory, and then crafting one less is fine.
        if (!player_->can_add(item) || !pay(p->recipe, from)) break;
        player_->add(item, 1);
    }
    return made;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ItemsMenu::draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    const PanelConfig config = {
        .position    = ui.screen_size() * glm::vec2(0.5f, 0.3f),
        .pivot       = { 0.5f, 0.5f },
        .size        = InventoryMenu::panel_size(ui, COLUMNS, VISIBLE_ROWS),
        .min_size    = InventoryMenu::panel_size(ui, COLUMNS, VISIBLE_ROWS),
        .flags       = PanelFlags::Movable,
        .scroll_step = InventoryMenu::SLOT_SIZE.y + InventoryMenu::SLOT_GAP,
    };
    if (!ui.begin_panel("Items", &open, config)) return;

    // Each kind of item; a recipe you can't pay for dimmed
    Inventory* const have[] = { settlement_, player_ };
    for (std::size_t i = 0; i < sorted_.size(); i++) {
        const ItemPrototype& item = *sorted_[i];
        Nezumi::UI::Slot&    s    = slots_[i];
        s = {};
        s.icon  = sheet_->item_icon(item.id);
        s.value = item.id.value();
        s.disabled = !item.recipe.empty() && shortfall(item.recipe, have).has_value();
    }

    // Not draggable: these are the kinds of items, not stacks
    const Nezumi::UI::SlotGridResult r = ui.slot_grid(
        "items", slots_, static_cast<int>(COLUMNS),
        sheet_->item_slot_style(InventoryMenu::SLOT_SIZE, InventoryMenu::SLOT_GAP));

    if (r.clicked >= 0) {
        const ItemPrototype& item = *sorted_[static_cast<std::size_t>(r.clicked)];
        if (mods.shortcut) { if (action_bar_) action_bar_->add_shortcut(item.id); }
        else               craft(item.id, 1);
    }
    if (r.right_clicked >= 0) craft(sorted_[static_cast<std::size_t>(r.right_clicked)]->id, CRAFT_MANY);

    if (r.hovered >= 0) tooltip_(ui, *sorted_[static_cast<std::size_t>(r.hovered)]);

    ui.end_panel();
}

// "House
//  Crafted from 4 wood
//  Builds a house, 2 x 2
//  Stacks to 20
//  Click: craft 1   Right click: craft 5   Ctrl + click: action bar"
void ItemsMenu::tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item) {
    char line[128];
    tip_ = capitalized(item.name);

    if (!item.recipe.empty()) {
        tip_ += "\nCrafted from";
        for (std::size_t k = 0; k < item.recipe.size(); k++) {
            const std::string_view n = item_name(item.recipe[k].item);
            std::snprintf(line, sizeof line, "%s %u %.*s", k == 0 ? "" : ",",
                          static_cast<unsigned>(item.recipe[k].count), static_cast<int>(n.size()), n.data());
            tip_ += line;
        }
    } else {
        bool chopped = false;
        for (const TreeConfig& t : configs_->trees)
            if (t.resource.item == item.id) chopped = true;
        tip_ += chopped ? "\nChopped from trees" : "\nCan't be made yet";
    }

    if (const BuildingPrototype* b = configs_->building_for_item(item.id)) {
        std::snprintf(line, sizeof line, "\nBuilds a %s, %d x %d", b->name.c_str(),
                      b->config.footprint.x, b->config.footprint.y);
        tip_ += line;
    }
    std::snprintf(line, sizeof line, "\nStacks to %u", static_cast<unsigned>(item.stack_size));
    tip_ += line;


    tip_ += item.recipe.empty() ? "\nCtrl + click: action bar"
                                : "\nClick: craft 1   Right click: craft 5   Ctrl + click: action bar";
    ui.tooltip(tip_);
}

} // namespace Nightmourn
