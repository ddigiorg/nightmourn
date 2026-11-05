// =============================================================================
// GUI/CraftRulesMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/CraftRulesMenu.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // sort, min
#include <cstdio>    // snprintf
#include <limits>    // numeric_limits

namespace Nightmourn {

namespace {
    constexpr std::size_t COLUMNS = 10;

    // Digits only, capped: "1x2" is 12, "" is 0
    std::uint32_t parse_count(const std::string& s) {
        std::uint64_t v = 0;
        for (char c : s) {
            if (c < '0' || c > '9') continue;
            v = std::min<std::uint64_t>(v * 10 + static_cast<std::uint64_t>(c - '0'), 1'000'000);
        }
        return static_cast<std::uint32_t>(v);
    }
}

CraftRulesMenu::CraftRulesMenu(const UISheet& sheet, const Configs& configs)
    : sheet_(&sheet), configs_(&configs)
{
    for (const ItemPrototype& item : configs.items) sorted_.push_back(&item);
    std::sort(sorted_.begin(), sorted_.end(),
              [](const ItemPrototype* a, const ItemPrototype* b) { return a->name < b->name; });
}

std::vector<CraftRuleChange> CraftRulesMenu::take_changes() {
    std::vector<CraftRuleChange> out;
    out.swap(changes_);
    return out;
}

// -----------------------------------------------------------------------------
// What it shows
// -----------------------------------------------------------------------------

void CraftRulesMenu::list_() {
    shown_.clear();
    const CraftCategoryMask offered = site_ ? site_->categories() : 0;
    const auto makes_here = [&](const ItemPrototype& item) {
        return item.recipe && item.recipe->can_craft_in(offered) && item.recipe->makes(item.id) > 0;
    };

    if (tab_ == Tab::Make) {
        for (const ItemPrototype* item : sorted_) if (makes_here(*item)) shown_.push_back(item);
        return;
    }

    // Reserves: whatever those recipes use, A to Z
    std::vector<bool> used(configs_->items.size(), false);
    for (const ItemPrototype& item : configs_->items) {
        if (!makes_here(item)) continue;
        for (const ItemAmount& in : item.recipe->ingredients)
            if (in.item.value() < used.size()) used[in.item.value()] = true;
    }
    for (const ItemPrototype* item : sorted_) if (used[item->id.value()]) shown_.push_back(item);
}

std::uint32_t CraftRulesMenu::value_(ItemID item) const {
    if (!site_) return 0;
    return tab_ == Tab::Make ? site_->target(item) : site_->floor(item);
}

void CraftRulesMenu::change_(ItemID item, std::uint32_t value) {
    const ItemPrototype* proto = configs_->item(item);
    if (!proto) return;
    if (tab_ == Tab::Make) {
        if (proto->recipe) changes_.push_back({ CraftRuleChange::Kind::Target, proto->recipe, item, value });
    } else {
        changes_.push_back({ CraftRuleChange::Kind::Floor, nullptr, item, value });
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void CraftRulesMenu::draw(Nezumi::UI::Context& ui, bool& open, const InventoryModifiers& mods) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    const PanelConfig config = {
        .position = ui.screen_size() * glm::vec2(0.5f, 0.32f),
        .pivot    = { 0.5f, 0.5f },
        .min_size = { InventoryMenu::panel_size(ui, COLUMNS, 1).x, 0.0f },
        .flags    = PanelFlags::Movable | PanelFlags::AutoSize,
    };
    if (!ui.begin_panel("Settlement Crafting", &open, config)) return;

    if (!site_) {
        ui.text("You have no settlement yet.");
        ui.text("Place your banner to found one.", ui.style().text_dim);
        ui.end_panel();
        return;
    }

    if (ui.button("Make"))     { tab_ = Tab::Make;     selected_ = {}; }
    ui.same_line();
    if (ui.button("Reserves")) { tab_ = Tab::Reserves; selected_ = {}; }
    draw_status_(ui);
    ui.heading(tab_ == Tab::Make ? "Keep in store" : "Never use below");

    list_();
    if (shown_.empty()) {
        ui.text(site_->machine_count() == 0 ? "Build a workshop to craft here."
                                            : "Nothing its workshops make uses anything.",
                ui.style().text_dim);
        ui.end_panel();
        return;
    }

    // A slot per item: its target (or floor) top right, what's in store
    // bottom right; dimmed while it has none. The one being edited is
    // drawn pressed.
    slots_.resize(shown_.size());
    labels_.resize(shown_.size());
    counts_.resize(shown_.size());
    for (std::size_t i = 0; i < shown_.size(); i++) {
        const ItemPrototype& item = *shown_[i];
        const std::uint32_t  v    = value_(item.id);
        Nezumi::UI::Slot&    s    = slots_[i];
        s = {};
        s.icon     = sheet_->item_icon(item.id);
        s.value    = item.id.value();
        s.disabled = v == 0;
        s.flash    = item.id == selected_;
        if (v > 0) {
            std::snprintf(labels_[i].data(), labels_[i].size(), "%u", static_cast<unsigned>(v));
            s.label = labels_[i].data();
        }
        std::snprintf(counts_[i].data(), counts_[i].size(), "%u",
                      static_cast<unsigned>(storage_ ? storage_->count(item.id) : 0));
        s.count = counts_[i].data();
    }

    const Nezumi::UI::SlotGridResult r = ui.slot_grid(
        tab_ == Tab::Make ? "make" : "reserves", slots_, static_cast<int>(COLUMNS),
        sheet_->item_slot_style(InventoryMenu::SLOT_SIZE, InventoryMenu::SLOT_GAP));
    if (r.clicked >= 0) selected_ = shown_[static_cast<std::size_t>(r.clicked)]->id;
    if (r.hovered >= 0) tooltip_(ui, *shown_[static_cast<std::size_t>(r.hovered)]);

    draw_editor_(ui, mods);
    ui.end_panel();
}

// "Workshops: 2 (1 working, 1 waiting for room)"
void CraftRulesMenu::draw_status_(Nezumi::UI::Context& ui) {
    std::uint32_t working = 0, full = 0;
    for (const Machine& m : site_->machines()) {
        if (!m.building) continue;
        if (m.state == MachineState::Working)    working++;
        if (m.state == MachineState::OutputFull) full++;
    }
    char line[128];
    if (full > 0)
        std::snprintf(line, sizeof line, "Workshops: %zu (%u working, %u waiting for room)",
                      site_->machine_count(), static_cast<unsigned>(working), static_cast<unsigned>(full));
    else
        std::snprintf(line, sizeof line, "Workshops: %zu (%u working)",
                      site_->machine_count(), static_cast<unsigned>(working));
    ui.text(line, ui.style().text_dim);
}

// "Wooden arrow: 12 in store, 5 being made
//  Keep  [-10] [ 100 ] [+10]"
void CraftRulesMenu::draw_editor_(Nezumi::UI::Context& ui, const InventoryModifiers& mods) {
    const ItemPrototype* item = nullptr;
    for (const ItemPrototype* p : shown_) if (p->id == selected_) item = p;
    if (!item) {
        selected_ = {};
        ui.text(editable_ ? "Click an item to set its number." : "Step inside the settlement to change its rules.",
                ui.style().text_dim);
        return;
    }

    char line[160];
    const std::string    name  = display_name(item->name);
    const std::uint32_t  have  = storage_ ? storage_->count(item->id) : 0;
    const std::uint32_t  value = value_(item->id);
    if (tab_ == Tab::Make)
        std::snprintf(line, sizeof line, "%s: %u in store, %u being made", name.c_str(),
                      static_cast<unsigned>(have), static_cast<unsigned>(site_->in_progress(item->id)));
    else
        std::snprintf(line, sizeof line, "%s: %u in store", name.c_str(), static_cast<unsigned>(have));
    ui.text(line);

    if (!editable_) {
        ui.text("Step inside the settlement to change its rules.", ui.style().text_dim);
        return;
    }

    // The typed number follows the rule unless it's being typed into
    if (field_for_ != item->id || (!ui.wants_keyboard() && field_was_ != value)) {
        field_     = std::to_string(value);
        field_for_ = item->id;
        field_was_ = value;
    }

    const std::uint32_t step = mods.move ? BIG_STEP : STEP;
    char dec[24], inc[24];
    std::snprintf(dec, sizeof dec, "-%u##dec", static_cast<unsigned>(step));
    std::snprintf(inc, sizeof inc, "+%u##inc", static_cast<unsigned>(step));

    ui.text(tab_ == Tab::Make ? "Keep" : "At least");
    ui.same_line();
    if (ui.button(dec)) change_(item->id, value > step ? value - step : 0);
    ui.same_line();
    if (ui.text_field("##value", field_, 7)) {
        field_was_ = parse_count(field_);
        change_(item->id, field_was_);
    }
    ui.same_line();
    if (ui.button(inc)) change_(item->id, std::min<std::uint32_t>(value + step, 1'000'000));
}

// "Wooden arrow
//  Made 5 at a time, 1 s each, from 1 wood
//  Click to set how many to keep"
//
// Nothing in it changes while it's hovered (the UI restarts a tooltip's
// delay when its text changes): the slot shows the live numbers
void CraftRulesMenu::tooltip_(Nezumi::UI::Context& ui, const ItemPrototype& item) {
    char line[160];
    tip_ = display_name(item.name);
    if (tab_ == Tab::Make && item.recipe) {
        const float seconds = static_cast<float>(item.recipe->ticks) / static_cast<float>(TICKS_PER_SECOND);
        std::snprintf(line, sizeof line, "\nMade %u at a time, %g s each", static_cast<unsigned>(item.recipe->makes(item.id)), seconds);
        tip_ += line;
        for (std::size_t k = 0; k < item.recipe->ingredients.size(); k++) {
            const ItemAmount&      in = item.recipe->ingredients[k];
            const std::string_view n  = item_name(in.item);
            std::snprintf(line, sizeof line, "%s%u %.*s", k == 0 ? ", from " : ", ",
                          static_cast<unsigned>(in.count), static_cast<int>(n.size()), n.data());
            tip_ += line;
        }
        tip_ += "\nTop right: how many to keep. Bottom right: in store.";
        tip_ += editable_ ? "\nClick to set how many to keep" : "";
    } else {
        tip_ += "\nTop right: never used below it. Bottom right: in store.";
        tip_ += editable_ ? "\nClick to set how much to keep back" : "";
    }
    ui.tooltip(tip_);
}

} // namespace Nightmourn
