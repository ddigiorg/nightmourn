// =============================================================================
// GUI/InventoryMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nezumi/UI/Context.hpp"
#include <cctype> // toupper
#include <cstdio> // snprintf

namespace Nightmourn {

namespace {
    // Same slots as the action bar's, so an icon dragged between them
    // doesn't change size
    constexpr glm::vec2 SLOT_SIZE = { 36.0f, 36.0f };
    constexpr float     SLOT_GAP  = 3.0f;
}

InventoryMenu::InventoryMenu(const UIAtlas& atlas, Inventory& inventory)
    : atlas_(&atlas)
    , inventory_(&inventory)
    , slots_(inventory.size())
    , counts_(inventory.size())
{}

void InventoryMenu::draw(Nezumi::UI::Context& ui, bool& open) {
    using Nezumi::UI::PanelConfig;
    using Nezumi::UI::PanelFlags;

    // Centered the first time it opens; the player can move it from there
    const PanelConfig config = {
        .position = ui.screen_size() * 0.5f,
        .pivot    = { 0.5f, 0.5f },
        .flags    = PanelFlags::Movable | PanelFlags::AutoSize,
    };
    if (!ui.begin_panel("Inventory", &open, config)) return;

    Inventory&        inv = *inventory_;
    const std::size_t n   = inv.size();

    for (std::size_t i = 0; i < n; i++) {
        Nezumi::UI::Slot& s = slots_[i];
        s = {};

        const auto& stack = inv.slot(i);
        if (!stack) continue;
        std::snprintf(counts_[i].data(), counts_[i].size(), "%u", static_cast<unsigned>(stack->count));
        s.icon  = atlas_->item_icon(stack->item);
        s.count = counts_[i].data();
        s.value = stack->item.value(); // what an action bar slot points at
    }

    const Nezumi::UI::Context::ID    grid = ui.get_id("slots");
    const Nezumi::UI::SlotGridResult r    = ui.slot_grid(
        "slots", slots_, static_cast<int>(Player::INVENTORY_COLUMNS),
        atlas_->item_slot_style(SLOT_SIZE, SLOT_GAP), ActionBar::DRAG_TYPE);

    // Only stacks from this grid move; an action bar shortcut dropped here
    // is just a shortcut, with no stack behind it
    if (r.dropped >= 0 && r.payload.source == grid && r.payload.index >= 0)
        inv.move(static_cast<std::size_t>(r.payload.index), static_cast<std::size_t>(r.dropped));

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
