// =============================================================================
// GUI/Theme.hpp
//
// Nightmourn's look for Nezumi's UI: see-through panels of blue-tinted ash
// grey that let the night world show behind them, pale moonlit highlights,
// and no extra highlight on the panel in front. Call once at startup:
//
//   Nightmourn::apply_ui_theme(engine.ui()->style());
//
// Only colors change; sizes stay the engine's.
// =============================================================================

#pragma once

namespace Nezumi::UI {
    struct Style;
}

namespace Nightmourn {

void apply_ui_theme(Nezumi::UI::Style& style);

} // namespace Nightmourn
