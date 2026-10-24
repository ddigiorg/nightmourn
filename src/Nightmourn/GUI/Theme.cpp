// =============================================================================
// GUI/Theme.cpp
// =============================================================================

#include "Nightmourn/GUI/Theme.hpp"
#include "Nezumi/UI/Style.hpp"

namespace Nightmourn {

void apply_ui_theme(Nezumi::UI::Style& s) {
    s.text             = { 0.90f, 0.93f, 0.97f, 1.00f };
    s.text_dim         = { 0.55f, 0.62f, 0.72f, 1.00f };
    s.heading          = { 0.72f, 0.85f, 1.00f, 1.00f };

    s.panel_bg         = { 0.05f, 0.08f, 0.14f, 0.96f };
    s.panel_border     = { 0.01f, 0.02f, 0.05f, 1.00f };
    s.title_bg         = { 0.08f, 0.13f, 0.22f, 1.00f };
    s.title_bg_front   = { 0.11f, 0.19f, 0.33f, 1.00f };

    s.frame_bg         = { 0.09f, 0.14f, 0.23f, 1.00f };
    s.frame_bg_hovered = { 0.13f, 0.20f, 0.32f, 1.00f };
    s.frame_bg_active  = { 0.17f, 0.26f, 0.41f, 1.00f };
    s.frame_border     = { 0.02f, 0.04f, 0.08f, 1.00f };

    s.button           = { 0.14f, 0.23f, 0.38f, 1.00f };
    s.button_hovered   = { 0.20f, 0.32f, 0.51f, 1.00f };
    s.button_active    = { 0.10f, 0.17f, 0.28f, 1.00f };

    s.accent           = { 0.45f, 0.72f, 1.00f, 1.00f };
    s.accent_dim       = { 0.22f, 0.40f, 0.62f, 1.00f };
    s.close_hovered    = { 0.70f, 0.20f, 0.25f, 1.00f };
    s.separator        = { 0.15f, 0.22f, 0.33f, 1.00f };
    s.scrollbar        = { 0.32f, 0.44f, 0.60f, 0.80f };
    s.selection        = { 0.16f, 0.28f, 0.46f, 1.00f };
}

} // namespace Nightmourn
