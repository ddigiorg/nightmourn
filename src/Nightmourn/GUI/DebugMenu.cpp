// =============================================================================
// GUI/DebugMenu.cpp
// =============================================================================

#include "Nightmourn/GUI/DebugMenu.hpp"
#include "Nezumi/UI/Context.hpp"
#include <cstddef> // size_t

namespace Nightmourn {

void DebugMenu::draw(Nezumi::UI::Context& ui, DebugFlags& flags) {
    using Nezumi::UI::PanelFlags;

    // Top-right corner the first time it opens; after that it stays where
    // the player dragged it
    const Nezumi::UI::PanelConfig config = {
        .position = { ui.screen_size().x - 16.0f, 16.0f },
        .pivot    = { 1.0f, 0.0f },
        .flags    = PanelFlags::Movable | PanelFlags::AutoSize,
    };

    if (!ui.begin_panel("Debug options", &open_, config)) return;

    for (std::size_t i = 0; i < DEBUG_FLAG_COUNT; i++) {
        const auto flag = static_cast<DebugFlag>(i);
        bool on = flags.is_on(flag);
        if (ui.checkbox(DebugFlags::name(flag), on)) flags.set(flag, on);
    }

    ui.separator();
    if (ui.button("Clear all")) flags.clear();

    ui.end_panel();
}

} // namespace Nightmourn
