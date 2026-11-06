// =============================================================================
// GUI/Theme.cpp
// =============================================================================

#include "Nightmourn/GUI/Theme.hpp"
#include "Nezumi/UI/Style.hpp"

namespace Nightmourn {

void apply_ui_theme(Nezumi::UI::Style& s) {
    // Text: pale ash, with moonlit blue headings
    s.text                  = { 0.83f, 0.85f, 0.88f, 1.00f }; // #D4D9E1
    s.text_dim              = { 0.54f, 0.58f, 0.64f, 1.00f }; // #8A93A3
    s.heading               = { 0.66f, 0.76f, 0.86f, 1.00f }; // #A9C1DC

    // Panels: see-through ash, one outline, the same whichever is in front
    s.panel_bg              = { 0.11f, 0.12f, 0.15f, 0.78f }; // #1C1F26
    s.panel_border          = { 0.23f, 0.25f, 0.30f, 1.00f }; // #3A404C
    s.border_front          = { 0.00f, 0.00f, 0.00f, 0.00f }; // none: panel_border
    s.border_front_end      = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.title_bg              = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.title_bg_front        = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.tab_hovered           = { 0.83f, 0.85f, 0.88f, 0.06f };
    s.tab_picked            = { 0.56f, 0.69f, 0.84f, 0.10f };

    // Fields, tracks, slots: darker ash, tinted rather than solid
    s.frame_bg              = { 0.07f, 0.08f, 0.10f, 0.50f };
    s.frame_bg_hovered      = { 0.18f, 0.20f, 0.25f, 0.60f };
    s.frame_bg_active       = { 0.24f, 0.27f, 0.33f, 0.70f };
    s.frame_border          = { 0.20f, 0.23f, 0.27f, 1.00f }; // #343A45
    s.frame_border_hovered  = { 0.35f, 0.39f, 0.45f, 1.00f }; // #5A6474

    // Buttons: outlined, lit in moonlight on hover
    s.button                = { 0.07f, 0.08f, 0.10f, 0.35f };
    s.button_hovered        = { 0.56f, 0.69f, 0.84f, 0.12f };
    s.button_active         = { 0.56f, 0.69f, 0.84f, 0.24f };
    s.button_border         = { 0.20f, 0.23f, 0.27f, 1.00f };
    s.button_border_hovered = { 0.56f, 0.69f, 0.84f, 1.00f };
    s.button_text_hovered   = { 0.75f, 0.84f, 0.93f, 1.00f };

    s.accent                = { 0.56f, 0.69f, 0.84f, 1.00f }; // #8FB1D6: moonlight
    s.accent_dim            = { 0.30f, 0.38f, 0.48f, 1.00f };
    s.close_hovered         = { 0.71f, 0.32f, 0.35f, 1.00f }; // #B5525A
    s.close_text_hovered    = { 0.08f, 0.09f, 0.11f, 1.00f };
    s.separator             = { 0.23f, 0.25f, 0.30f, 1.00f };
    s.scrollbar             = { 0.40f, 0.44f, 0.52f, 0.90f };
    s.selection             = { 0.56f, 0.69f, 0.84f, 0.25f };
    s.tooltip_bg            = { 0.07f, 0.08f, 0.10f, 0.94f };
}

} // namespace Nightmourn
