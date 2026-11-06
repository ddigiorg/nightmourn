// =============================================================================
// GUI/ItemsMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/ItemsMenu.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // clamp, sort
#include <cctype>    // tolower
#include <cmath>     // ceil
#include <cstdio>    // snprintf
#include <optional>  // optional
#include <span>      // span

namespace Nightmourn {

namespace {
    constexpr std::size_t COLUMNS      = 10;
    constexpr std::size_t VISIBLE_ROWS = 4;

    // Whether `name` holds `query`, ignoring case ("" matches everything)
    bool name_matches(std::string_view name, std::string_view query) {
        const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        if (query.size() > name.size()) return false;
        for (std::size_t at = 0; at + query.size() <= name.size(); at++) {
            std::size_t k = 0;
            while (k < query.size() && lower(name[at + k]) == lower(query[k])) k++;
            if (k == query.size()) return true;
        }
        return false;
    }

    // "4 wood, 1 stone"
    void append_amounts(std::string& out, std::span<const ItemAmount> amounts) {
        char part[96];
        for (std::size_t k = 0; k < amounts.size(); k++) {
            const std::string_view n = item_name(amounts[k].item);
            std::snprintf(part, sizeof part, "%s%u %.*s", k == 0 ? "" : ", ",
                          static_cast<unsigned>(amounts[k].count), static_cast<int>(n.size()), n.data());
            out += part;
        }
    }
}

ItemsMenu::ItemsMenu(const UISheet& sheet, const Configs& configs)
    : sheet_(&sheet), configs_(&configs)
{
    for (const ItemPrototype& item : configs.items) sorted_.push_back(&item);
    std::sort(sorted_.begin(), sorted_.end(),
              [](const ItemPrototype* a, const ItemPrototype* b) { return a->name < b->name; });
    shown_.reserve(sorted_.size());
    slots_.reserve(sorted_.size());
}

// -----------------------------------------------------------------------------
// Crafting
// -----------------------------------------------------------------------------

std::vector<CraftRequest> ItemsMenu::take_requests() {
    std::vector<CraftRequest> out;
    out.swap(requests_);
    return out;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ItemsMenu::draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    // As tall as every item needs, up to VISIBLE_ROWS rows (it scrolls the
    // rest), plus the search bar above them. The same while searching, so
    // the menu doesn't jump about as you type.
    const Nezumi::UI::Style& style = ui.style();
    const float       search_h = std::ceil(ui.text_height() + 2.0f * style.frame_padding.y) + style.item_spacing.y;
    const std::size_t rows     = std::clamp<std::size_t>((sorted_.size() + COLUMNS - 1) / COLUMNS, 1, VISIBLE_ROWS);
    const glm::vec2   size     = InventoryMenu::panel_size(ui, COLUMNS, rows) + glm::vec2(0.0f, search_h);

    const PanelConfig config = {
        .position    = ui.screen_size() * glm::vec2(0.5f, 0.3f),
        .pivot       = { 0.5f, 0.5f },
        .size        = size,
        .min_size    = size,
        .flags       = PanelFlags::Movable,
        .scroll_step = InventoryMenu::SLOT_SIZE.y + InventoryMenu::SLOT_GAP,
    };
    if (!ui.begin_panel("Crafting", &open, config)) return;

    // The search bar, as wide as the grid: filters by name as you type
    const float grid_w = static_cast<float>(COLUMNS) * InventoryMenu::SLOT_SIZE.x
                       + static_cast<float>(COLUMNS - 1) * InventoryMenu::SLOT_GAP;
    ui.set_next_width(grid_w);
    ui.text_field("##search", search_, 64, "Search");

    shown_.clear();
    for (const ItemPrototype* item : sorted_)
        if (name_matches(display_name(item->name), search_)) shown_.push_back(item);
    if (shown_.empty()) {
        ui.text("No items match.", style.text_dim);
        ui.end_panel();
        return;
    }

    // Each kind of item; one that can't be queued here and now dimmed
    slots_.resize(shown_.size());
    for (std::size_t i = 0; i < shown_.size(); i++) {
        const ItemPrototype& item = *shown_[i];
        Nezumi::UI::Slot&    s    = slots_[i];
        s = {};
        s.icon  = sheet_->item_icon(item.id);
        s.value = item.id.value();
        s.disabled = item.recipe && queue_ && queue_->can_enqueue(*item.recipe, place_) != CraftRefusal::None;
    }

    // Not draggable: these are the kinds of items, not stacks
    const Nezumi::UI::SlotGridResult r = ui.slot_grid(
        "items", slots_, static_cast<int>(COLUMNS),
        sheet_->item_slot_style(InventoryMenu::SLOT_SIZE, InventoryMenu::SLOT_GAP));

    // Click: 1, Shift + click: all you can afford, Ctrl + click: action bar
    if (r.clicked >= 0) {
        const ItemPrototype& item = *shown_[static_cast<std::size_t>(r.clicked)];
        if (mods.shortcut)  { if (action_bar_) action_bar_->add_shortcut(item.id); }
        else if (item.recipe) requests_.push_back({ item.recipe, mods.move ? CraftQueue::ALL : 1u });
    }
    if (r.right_clicked >= 0) {
        const ItemPrototype& item = *shown_[static_cast<std::size_t>(r.right_clicked)];
        if (item.recipe) requests_.push_back({ item.recipe, CRAFT_MANY });
    }

    if (r.hovered >= 0) tooltip_(ui, *shown_[static_cast<std::size_t>(r.hovered)]);

    ui.end_panel();
}

// "House
//  Makes 1 in 2 s from 4 wood
//  Made inside your settlement
//  Short 3 wood
//  Builds a house, 2 x 2
//  Stacks to 10
//  Click: craft 1   Right click: 5   Shift + click: all   Ctrl + click: action bar"
void ItemsMenu::tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item) {
    char line[160];
    tip_ = display_name(item.name);

    if (const Recipe* recipe = item.recipe) {
        const float seconds = static_cast<float>(recipe->ticks) / static_cast<float>(TICKS_PER_SECOND);
        std::snprintf(line, sizeof line, "\nMakes %u in %g s", static_cast<unsigned>(recipe->makes(item.id)), seconds);
        tip_ += line;
        if (!recipe->ingredients.empty()) {
            tip_ += " from ";
            append_amounts(tip_, recipe->ingredients);
        }
        if (recipe->results.size() > 1) {
            tip_ += "\n(and makes ";
            append_amounts(tip_, recipe->results);
            tip_ += ")";
        }
        if (const RecipeCategoryPrototype* where = configs_->recipe_category(recipe->category);
            where && !where->description.empty()) {
            tip_ += "\nMade " + where->description;
        }

        // Why it can't be queued, if it can't
        std::optional<ItemAmount> short_of;
        switch (queue_ ? queue_->can_enqueue(*recipe, place_, &short_of) : CraftRefusal::None) {
            case CraftRefusal::None:       break;
            case CraftRefusal::WrongPlace: tip_ += "\nYou can't make it here"; break;
            case CraftRefusal::NoRoom:     tip_ += "\nNo room in your inventory"; break;
            case CraftRefusal::CantAfford:
                if (short_of) {
                    const std::string_view n = item_name(short_of->item);
                    std::snprintf(line, sizeof line, "\nShort %u %.*s", static_cast<unsigned>(short_of->count),
                                  static_cast<int>(n.size()), n.data());
                    tip_ += line;
                }
                break;
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

    tip_ += item.recipe ? "\nClick: craft 1   Right click: 5   Shift + click: all   Ctrl + click: action bar"
                        : "\nCtrl + click: action bar";
    ui.tooltip(tip_);
}

} // namespace Nightmourn
