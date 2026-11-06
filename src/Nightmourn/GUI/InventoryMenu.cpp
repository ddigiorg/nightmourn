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

glm::vec2 InventoryMenu::panel_size(const Nezumi::UI::Context& ui, std::size_t columns, std::size_t rows) {
    // Not AutoSize, since an AutoSize panel grows to fit instead of
    // scrolling. The scrollbar sits in the right padding, so it needs no room
    // of its own.
    const auto grid = [](std::size_t count) {
        return static_cast<float>(count) * SLOT_SIZE.x + static_cast<float>(count - 1) * SLOT_GAP;
    };
    const Nezumi::UI::Style& style = ui.style();
    return {
        grid(columns) + 2.0f * style.panel_padding.x,
        grid(rows) + 2.0f * style.panel_padding.y + header_height(ui) + 1.0f,
    };
}

float InventoryMenu::header_height(const Nezumi::UI::Context& ui) {
    // Nezumi's header row (and a tab strip is as tall): the text, padded by
    // title_padding and frame_padding above and below
    const Nezumi::UI::Style& style = ui.style();
    return std::ceil(ui.text_height() + 2.0f * style.title_padding + 2.0f * style.frame_padding.y);
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

    const std::size_t cols = config_.columns;

    // Sized to show visible_rows rows; the panel scrolls the rest
    const glm::vec2 size = panel_size(ui, cols, config_.visible_rows);

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
    draw_contents(ui, other, mods);
    ui.end_panel();
}

void InventoryMenu::draw_contents(Nezumi::UI::Context& ui, InventoryMenu* other, Modifiers mods) {
    const std::size_t cols = config_.columns;
    if (other && !other->inventory_) other = nullptr;

    if (!inventory_) {
        for (std::size_t at = 0; at <= note_.size();) {
            const std::size_t end = std::min(note_.find('\n', at), note_.size());
            ui.text(note_.substr(at, end - at), at == 0 ? ui.style().text : ui.style().text_dim);
            at = end + 1;
        }
        return;
    }
    Inventory&        inv = *inventory_;
    const std::size_t n   = inv.size();

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

    // Shift + click: the whole stack to the other inventory; Shift + right
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

    // A plain click picks the stack
    if (!mods.move && !mods.shortcut && r.clicked >= 0) {
        if (const auto& stack = inv.slot(static_cast<std::size_t>(r.clicked))) picked_ = stack->item;
    }

    // Ctrl + click: onto the action bar
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
}

} // namespace Nightmourn
