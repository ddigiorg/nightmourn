// =============================================================================
// GUI/Theme.hpp
//
// Nightmourn's look for Nezumi's UI, after Terraria's Stormdark pack: no
// panels at all, just see-through slate slots, outlined text and controls
// floating over the world, with pale moonlit highlights. Call once at startup:
//
//   Nightmourn::apply_ui_theme(engine.ui()->style());
//
// Colors, plus the slot and control rounding and the text outline.
// =============================================================================

#pragma once

namespace Nezumi::UI {
    struct Style;
}

namespace Nightmourn {

void apply_ui_theme(Nezumi::UI::Style& style);

} // namespace Nightmourn
