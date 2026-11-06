// =============================================================================
// GUI/Theme.cpp
// =============================================================================

#include "Nightmourn/GUI/Theme.hpp"
#include "Nezumi/UI/Style.hpp"

namespace Nightmourn {

void apply_ui_theme(Nezumi::UI::Style& s) {
    // Sizes: slightly rounded slots and controls, like Terraria's Stormdark
    // pack and WoW's action bars
    s.slot_rounding         = 5.0f;
    s.frame_rounding        = 4.0f;
    s.text_outline_width    = 1.25f; // text sits straight on the world
    s.panel_rounding        = 0.0f;  // the hover outline is square
    s.panel_border_width    = 3.0f;
    s.panel_padding         = { 16.0f, 10.0f }; // room for the scrollbar beside the slots
    s.scrollbar_width       = 8.0f;
    s.tab_padding           = 13.0f; // tab fills meet the topbar, labels meet the content

    // Text: pale ash, moonlit blue headings, dark outlines
    s.text                  = { 0.88f, 0.90f, 0.93f, 1.00f };
    s.text_dim              = { 0.62f, 0.66f, 0.72f, 1.00f };
    s.heading               = { 0.74f, 0.82f, 0.92f, 1.00f };
    s.text_outline          = { 0.03f, 0.04f, 0.06f, 0.95f };

    // Panels: a see-through grey and no header line, so menus
    // read as one group while the world still shows through. The outline
    // shows only while the cursor is over the menu; the topbar and tabs are
    // slate.
    s.panel_bg              = { 0.30f, 0.33f, 0.38f, 0.35f }; // a darker grey, less see-through
    s.panel_border          = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.panel_border_hovered  = { 0.51f, 0.57f, 0.67f, 0.70f };
    s.border_front          = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.border_front_end      = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.panel_shadow          = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.title_bg              = { 0.08f, 0.09f, 0.13f, 0.72f };
    s.title_bg_front        = { 0.08f, 0.09f, 0.13f, 0.72f };
    s.header_line           = { 0.00f, 0.00f, 0.00f, 0.00f };
    s.tab_bg                = { 0.08f, 0.09f, 0.13f, 0.72f };
    s.tab_hovered           = { 0.78f, 0.83f, 0.90f, 0.08f };
    s.tab_picked            = { 0.78f, 0.83f, 0.90f, 0.12f }; // and underlined in the accent

    // Slots: Stormdark's dark slate with a light grey-blue edge, both
    // see-through
    s.slot_bg               = { 0.08f, 0.09f, 0.13f, 0.72f }; // #14181F
    s.slot_border           = { 0.51f, 0.57f, 0.67f, 0.55f }; // #8292AA
    s.slot_border_hovered   = { 0.78f, 0.83f, 0.90f, 0.90f }; // #C8D4E6

    // Fields, tracks, buttons: the same slate and edge as the slots
    s.frame_bg              = { 0.08f, 0.09f, 0.13f, 0.72f };
    s.frame_bg_hovered      = { 0.16f, 0.19f, 0.25f, 0.80f };
    s.frame_bg_active       = { 0.22f, 0.26f, 0.33f, 0.85f };
    s.frame_border          = { 0.51f, 0.57f, 0.67f, 0.55f };
    s.frame_border_hovered  = { 0.78f, 0.83f, 0.90f, 0.90f };
    s.button                = { 0.08f, 0.09f, 0.13f, 0.72f };
    s.button_hovered        = { 0.16f, 0.19f, 0.25f, 0.80f };
    s.button_active         = { 0.22f, 0.26f, 0.33f, 0.85f };
    s.button_border         = { 0.51f, 0.57f, 0.67f, 0.55f };
    s.button_border_hovered = { 0.78f, 0.83f, 0.90f, 0.90f };
    s.button_text_hovered   = { 0.96f, 0.97f, 1.00f, 1.00f };

    // Tooltips: the slot's slate, a little more solid, with its edge
    s.tooltip_bg            = { 0.08f, 0.09f, 0.13f, 0.92f };
    s.tooltip_border        = { 0.51f, 0.57f, 0.67f, 0.80f };

    s.accent                = { 0.66f, 0.77f, 0.90f, 1.00f }; // moonlight
    s.accent_dim            = { 0.34f, 0.41f, 0.51f, 1.00f };
    s.close_hovered         = { 0.71f, 0.32f, 0.35f, 0.90f };
    s.close_text_hovered    = { 0.96f, 0.97f, 1.00f, 1.00f };
    s.separator             = { 0.51f, 0.57f, 0.67f, 0.40f };
    s.scrollbar             = { 0.51f, 0.57f, 0.67f, 0.85f }; // light grey thumb
    s.scrollbar_track       = { 0.08f, 0.09f, 0.13f, 0.72f }; // on a slate track
    s.selection             = { 0.66f, 0.77f, 0.90f, 0.25f };
}

} // namespace Nightmourn
