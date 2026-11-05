// =============================================================================
// GUI/InventoryMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // max, min
#include <cctype>    // toupper
#include <cmath>     // ceil
#include <cstdio>    // snprintf

namespace Nightmourn {

namespace {
    // Same slots as the action bar's, so an icon dragged between them
    // doesn't change size
    constexpr glm::vec2 SLOT_SIZE = { 36.0f, 36.0f };
    constexpr float     SLOT_GAP  = 3.0f;
}

InventoryMenu::InventoryMenu(const UISheet& sheet, Inventory* inventory, const Config& config)
    : sheet_(&sheet)
    , inventory_(inventory)
    , config_(config)
{
    config_.columns      = std::max<std::size_t>(config_.columns, 1);
    config_.visible_rows = std::max<std::size_t>(config_.visible_rows, 1);
}

void InventoryMenu::draw(Nezumi::UI::Context& ui, bool& open, InventoryMenu* other, Modifiers mods) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    if (!inventory_) return;
    Inventory&        inv  = *inventory_;
    const std::size_t n    = inv.size();
    const std::size_t cols = config_.columns;
    if (other && !other->inventory_) other = nullptr;

    // Sized to show visible_rows rows; the panel scrolls the rest. Not
    // AutoSize, since an AutoSize panel grows to fit instead of scrolling.
    // The scrollbar sits in the right padding, so it needs no room of its own.
    const auto  grid_size = [&](std::size_t count) {
        return static_cast<float>(count) * SLOT_SIZE.x + static_cast<float>(count - 1) * SLOT_GAP;
    };
    const Nezumi::UI::Style& style = ui.style();
    const float title_h = std::ceil(ui.text_height() + 2.0f * style.title_padding);
    const glm::vec2 size = {
        grid_size(cols) + 2.0f * style.panel_padding.x,
        grid_size(config_.visible_rows) + 2.0f * style.panel_padding.y + title_h + 1.0f,
    };

    // Placed the first time it opens; the player can move it from there
    const PanelConfig config = {
        .position = ui.screen_size() * config_.anchor,
        .pivot    = config_.pivot,
        .size     = size,
        .min_size = size,
        .flags    = PanelFlags::Movable,
        .scroll_step = SLOT_SIZE.y + SLOT_GAP, // a whole row at a time
    };
    if (!ui.begin_panel(config_.title, &open, config)) return;

    if (slots_.size() < n) {
        slots_.resize(n);
        counts_.resize(n);
    }
    for (std::size_t i = 0; i < n; i++) {
        Nezumi::UI::Slot& s = slots_[i];
        s = {};

        const auto& stack = inv.slot(i);
        if (!stack) continue;
        std::snprintf(counts_[i].data(), counts_[i].size(), "%u", static_cast<unsigned>(stack->count));
        s.icon  = sheet_->item_icon(stack->item);
        s.count = counts_[i].data();
        s.value = stack->item.value(); // what an action bar slot points at
        if (!config_.shortcuts) s.value |= ActionBar::NOT_A_SHORTCUT;
    }

    grid_id_ = ui.get_id("slots");
    const Nezumi::UI::SlotGridResult r = ui.slot_grid(
        "slots", std::span<const Nezumi::UI::Slot>(slots_.data(), n), static_cast<int>(cols),
        sheet_->item_slot_style(SLOT_SIZE, SLOT_GAP), ActionBar::DRAG_TYPE);

    // Only stacks move: from this grid, or the other inventory's. An action
    // bar shortcut dropped here is just a shortcut, with no stack behind it.
    if (r.dropped >= 0 && r.payload.index >= 0) {
        const std::size_t from = static_cast<std::size_t>(r.payload.index);
        const std::size_t to   = static_cast<std::size_t>(r.dropped);
        if (r.payload.source == grid_id_)
            inv.move(from, to);
        else if (other && r.payload.source == other->grid_id_)
            Inventory::move(*other->inventory_, from, inv, to);
    }

    // Ctrl + click: the whole stack to the other inventory; Ctrl + right
    // click: half
    if (other && mods.move) {
        if (r.clicked >= 0)
            Inventory::transfer(inv, static_cast<std::size_t>(r.clicked), *other->inventory_);
        if (r.right_clicked >= 0) {
            if (const auto& stack = inv.slot(static_cast<std::size_t>(r.right_clicked)))
                Inventory::transfer(inv, static_cast<std::size_t>(r.right_clicked), *other->inventory_,
                                    (stack->count + 1u) / 2u);
        }
    }

    // Shift + click: onto the action bar
    if (mods.shortcut && !mods.move && r.clicked >= 0 && config_.shortcuts && action_bar_) {
        if (const auto& stack = inv.slot(static_cast<std::size_t>(r.clicked)))
            action_bar_->add_shortcut(stack->item);
    }

    // "Wood (37)"
    if (r.hovered >= 0) {
        if (const auto& stack = inv.slot(static_cast<std::size_t>(r.hovered))) {
            const std::string_view name = item_name(stack->item);
            char tip[48];
            std::snprintf(tip, sizeof tip, "%.*s (%u)", static_cast<int>(name.size()), name.data(),
                          static_cast<unsigned>(inv.count(stack->item)));
            tip[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(tip[0])));
            ui.tooltip(tip);
        }
    }

    ui.end_panel();
}

} // namespace Nightmourn
