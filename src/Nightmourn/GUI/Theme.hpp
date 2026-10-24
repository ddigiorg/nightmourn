// =============================================================================
// GUI/Theme.hpp
//
// Nightmourn's look for Nezumi's UI: deep night blues with pale moonlit
// highlights, in place of the engine's default purple. Call once at startup:
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
