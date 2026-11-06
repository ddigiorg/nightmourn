// =============================================================================
// GUI/SettlementMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/SettlementMenu.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // clamp, max, min, sort
#include <cmath>     // ceil, floor
#include <cstdio>    // snprintf
#include <span>      // span

namespace Nightmourn {

namespace {
    using glm::vec2;

    constexpr std::string_view TABS[] = { "Inventory", "Crafting" };

    // The Inventory tab: 10 wide, 6 rows in view
    constexpr std::size_t COLUMNS      = 10;
    constexpr std::size_t VISIBLE_ROWS = 6;

    // As wide as the inventory's 10 slots
    constexpr float ROWS_W = static_cast<float>(COLUMNS) * (InventoryMenu::SLOT_SIZE.x + InventoryMenu::SLOT_GAP)
                           - InventoryMenu::SLOT_GAP;

    // The Crafting tab's columns, from the content's left edge, and its rows
    constexpr vec2  ICON       = InventoryMenu::SLOT_SIZE;
    constexpr float ROW_PITCH  = ICON.y + 4.0f;
    constexpr float GAP        = 6.0f;
    constexpr float FIELD_W    = 64.0f; // 5 digits
    constexpr float PRIORITY_W = 44.0f;
    constexpr float PRIORITY_X = ROWS_W - PRIORITY_W;
    constexpr float MAX_X      = PRIORITY_X - GAP - FIELD_W;
    constexpr float MIN_X      = MAX_X - GAP - FIELD_W;
    constexpr float RANGE_X    = ICON.x + GAP;
    constexpr float RANGE_W    = MIN_X - GAP - RANGE_X;

    // The range slider's curve: most of the track for the low numbers
    constexpr float RANGE_POWER = 3.0f;

    // Digits only, capped: "1x2" is 12, "" is 0
    std::uint32_t parse_count(const std::string& s, std::uint32_t cap) {
        std::uint64_t v = 0;
        for (char c : s) {
            if (c < '0' || c > '9') continue;
            v = std::min<std::uint64_t>(v * 10 + static_cast<std::uint64_t>(c - '0'), cap);
        }
        return static_cast<std::uint32_t>(v);
    }
}

SettlementMenu::SettlementMenu(const UISheet& sheet, const Configs& configs)
    : sheet_(&sheet)
    , configs_(&configs)
    , inventory_(sheet, nullptr, InventoryMenu::Config{ .title = "##settlement-stock", .columns = COLUMNS,
                                                         .visible_rows = VISIBLE_ROWS, .shortcuts = false })
{
    for (const ItemPrototype& item : configs.items) sorted_.push_back(&item);
    std::sort(sorted_.begin(), sorted_.end(),
              [](const ItemPrototype* a, const ItemPrototype* b) { return a->name < b->name; });
    text_.resize(configs.items.size());
}

std::vector<SettlementCraftEdit> SettlementMenu::take_edits() {
    std::vector<SettlementCraftEdit> out;
    out.swap(edits_);
    return out;
}

void SettlementMenu::show_tab(Nezumi::UI::Context& ui, Tab tab) {
    ui.set_panel_tab(TITLE, tab);
    tab_ = tab;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void SettlementMenu::draw(Nezumi::UI::Context& ui, bool& open, InventoryMenu* other, const InventoryModifiers& mods) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    // The inventory's size, 10 by VISIBLE_ROWS slots, its title bar tall
    // enough for the tabs (the icon in place of the title, browser-style);
    // both tabs scroll the rest
    const Nezumi::UI::Style& style = ui.style();
    const vec2 size = InventoryMenu::panel_size(ui, COLUMNS, VISIBLE_ROWS) + vec2(0.0f, 2.0f * style.frame_padding.y);

    const PanelConfig config = {
        .position    = ui.screen_size() * vec2(0.495f, 0.66f), // beside the player's, centers level
        .pivot       = { 1.0f, 0.5f },
        .size        = size,
        .min_size    = size,
        .flags       = PanelFlags::Movable,
        .scroll_step = tab_ == InventoryTab ? InventoryMenu::SLOT_SIZE.y + InventoryMenu::SLOT_GAP : ROW_PITCH,
        .tabs        = TABS,
        .icon        = sheet_->menu_icon(Menu::Settlement),
    };
    if (!ui.begin_panel(TITLE, &open, config)) return;
    tab_ = ui.panel_tab() == CraftingTab ? CraftingTab : InventoryTab;

    if (tab_ == InventoryTab) {
        inventory_.draw_contents(ui, other, mods);
    } else {
        draw_crafting_(ui);
    }
    ui.end_panel();
}

// -----------------------------------------------------------------------------
// Crafting tab
// -----------------------------------------------------------------------------

bool SettlementMenu::makes_here_(const ItemPrototype& item) const {
    const CraftCategoryMask offered = site_ ? site_->categories() : 0;
    return item.recipe && item.recipe->can_craft_in(offered) && item.recipe->makes(item.id) > 0;
}

// What its machines can make, and what those recipes use, A to Z
void SettlementMenu::list_() {
    shown_.clear();
    std::vector<bool> used(configs_->items.size(), false);
    for (const ItemPrototype& item : configs_->items) {
        if (!makes_here_(item)) continue;
        used[item.id.value()] = true;
        for (const ItemAmount& in : item.recipe->ingredients)
            if (in.item.value() < used.size()) used[in.item.value()] = true;
    }
    for (const ItemPrototype* item : sorted_) if (used[item->id.value()]) shown_.push_back(item);
}

void SettlementMenu::draw_crafting_(Nezumi::UI::Context& ui) {
    const Nezumi::UI::Style& style = ui.style();
    if (!site_) {
        ui.text("You have no settlement yet.");
        ui.text("Place your banner to found one.", style.text_dim);
        return;
    }

    list_();
    if (shown_.empty()) {
        ui.text("Nothing can be made here yet.", style.text_dim);
        return;
    }

    // Column heads, or why nothing can be changed
    float y = 0.0f;
    if (!editable_) {
        ui.set_next_pos({ 0.0f, y });
        ui.text("Step inside the settlement to change these.", style.text_dim);
        y += ui.text_height() + style.item_spacing.y;
    }
    // Centred over their columns
    const auto head = [&](float x, float w, std::string_view text) {
        ui.set_next_pos({ std::floor(x + (w - ui.text_width(text)) * 0.5f), y });
        ui.text(text, style.text_dim);
    };
    head(RANGE_X,    RANGE_W,    "Min - Max");
    head(MIN_X,      FIELD_W,    "Min");
    head(MAX_X,      FIELD_W,    "Max");
    head(PRIORITY_X, PRIORITY_W, "Prio");
    y += ui.text_height() + style.item_spacing.y;

    for (const ItemPrototype* item : shown_) {
        draw_row_(ui, *item, y, makes_here_(*item));
        y += ROW_PITCH;
    }
}

void SettlementMenu::draw_row_(Nezumi::UI::Context& ui, const ItemPrototype& item, float y, bool makes) {
    const Nezumi::UI::Style& style = ui.style();
    const unsigned id = item.id.value();
    char label[32];

    // The icon, with what's in store; hover for the item
    Nezumi::UI::Slot slot;
    slot.icon = sheet_->item_icon(item.id);
    std::snprintf(count_.data(), count_.size(), "%u", static_cast<unsigned>(storage_ ? storage_->count(item.id) : 0));
    slot.count = count_.data();
    ui.set_next_pos({ 0.0f, y });
    std::snprintf(label, sizeof label, "##icon%u", id);
    const Nezumi::UI::SlotGridResult r = ui.slot_grid(label, std::span(&slot, 1), 1,
        sheet_->item_slot_style(ICON, 0.0f));
    if (r.hovered >= 0) tooltip_(ui, item, makes);

    // The controls, centered on the icon
    const float fy = y + std::floor((ICON.y - (ui.text_height() + 2.0f * style.frame_padding.y)) * 0.5f);
    const CraftLimits now = site_->limits(item.id);

    // `want` is what the row shows: the site's limits, or what was just
    // asked for until the tick applies it. A change from elsewhere (or
    // being outside the settlement) takes the site's back.
    RowText& t = text_[id];
    if (!t.synced || !editable_ || !same_(now, t.seen)) {
        if (!t.synced || !editable_ || !same_(now, t.want)) {
            t.want = now;
            if (!t.synced || !ui.wants_keyboard()) {
                t.min = std::to_string(now.min);
                t.max = std::to_string(now.max);
            }
        }
        t.seen   = now;
        t.synced = true;
    }
    CraftLimits& next = t.want;
    const CraftLimits before = next;

    if (makes) {
        int lo = static_cast<int>(std::min<std::uint32_t>(next.min, LIMIT_MAX));
        int hi = static_cast<int>(std::min<std::uint32_t>(next.max, LIMIT_MAX));
        if (hi < lo) hi = lo; // a min over the max shows as both at the min
        ui.set_next_pos({ RANGE_X, fy });
        ui.set_next_width(RANGE_W);
        std::snprintf(label, sizeof label, "##range%u", id);
        if (ui.range_slider(label, lo, hi, 0, LIMIT_MAX, RANGE_POWER)) {
            next.min = static_cast<std::uint32_t>(lo);
            next.max = static_cast<std::uint32_t>(hi);
            t.min    = std::to_string(next.min);
            t.max    = std::to_string(next.max);
        }
    } else {
        ui.set_next_pos({ RANGE_X, fy + style.frame_padding.y });
        ui.text("Not made here", style.text_dim);
    }

    ui.set_next_pos({ MIN_X, fy });
    ui.set_next_width(FIELD_W);
    std::snprintf(label, sizeof label, "##min%u", id);
    if (ui.text_field(label, t.min, 5)) {
        next.min = parse_count(t.min, LIMIT_MAX);
        if (makes && next.max < next.min) { next.max = next.min; t.max = std::to_string(next.max); }
    }

    if (makes) {
        ui.set_next_pos({ MAX_X, fy });
        ui.set_next_width(FIELD_W);
        std::snprintf(label, sizeof label, "##max%u", id);
        if (ui.text_field(label, t.max, 5)) {
            next.max = parse_count(t.max, LIMIT_MAX);
            if (next.min > next.max) { next.min = next.max; t.min = std::to_string(next.min); }
        }

        int priority = next.priority;
        ui.set_next_pos({ PRIORITY_X, fy });
        ui.set_next_width(PRIORITY_W);
        std::snprintf(label, sizeof label, "##priority%u", id);
        if (ui.slider(label, priority, 0, 255)) next.priority = static_cast<std::uint8_t>(priority);
    }

    if (editable_ && !same_(next, before)) edits_.push_back({ makes ? item.recipe : nullptr, item.id, next });
}

// "Wooden arrow
//  Made 10 at a time, 1 s each, from 1 wood, in a settlement with a workshop
//  12 in store, 20 being made
//  Min: never used below it. Max: made up to it. Priority: higher first."
void SettlementMenu::tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item, bool makes) {
    char line[160];
    tip_ = display_name(item.name);

    if (makes) {
        const Recipe& recipe  = *item.recipe;
        const float   seconds = static_cast<float>(recipe.ticks) / static_cast<float>(TICKS_PER_SECOND);
        std::snprintf(line, sizeof line, "\nMade %u at a time, %g s each", static_cast<unsigned>(recipe.makes(item.id)), seconds);
        tip_ += line;
        for (std::size_t k = 0; k < recipe.ingredients.size(); k++) {
            const ItemAmount&      in = recipe.ingredients[k];
            const std::string_view n  = item_name(in.item);
            std::snprintf(line, sizeof line, "%s%u %.*s", k == 0 ? ", from " : ", ",
                          static_cast<unsigned>(in.count), static_cast<int>(n.size()), n.data());
            tip_ += line;
        }
        if (const RecipeCategoryPrototype* where = configs_->recipe_category(recipe.category);
            where && !where->description.empty()) {
            tip_ += "\nMade " + where->description;
        }
    } else {
        tip_ += "\nNot made here: only kept back for crafting";
    }

    const unsigned have   = static_cast<unsigned>(storage_ ? storage_->count(item.id) : 0);
    const unsigned making = static_cast<unsigned>(site_ ? site_->in_progress(item.id) : 0);
    if (making > 0) std::snprintf(line, sizeof line, "\n%u in store, %u being made", have, making);
    else            std::snprintf(line, sizeof line, "\n%u in store", have);
    tip_ += line;

    tip_ += makes ? "\nMin: never used below it. Max: made up to it. Priority: higher first."
                  : "\nMin: never used below it.";
    ui.tooltip(tip_);
}

} // namespace Nightmourn
