// =============================================================================
// GUI/UISheet.cpp
// =============================================================================

#include "Nightmourn/GUI/UISheet.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"

namespace Nightmourn {

UISheet UISheet::load(Nezumi::Render::Renderer2D& renderer) {
    using Nezumi::UI::Image;

    const Nezumi::Render::SpriteSheet& sheet = *renderer.create_sprite_sheet(
        Nezumi::Render::SpriteSheetConfig{
            .name     = "ui",
            .manifest = "assets/graphics/ui.json",
            // Drawn at DENSITY sheet pixels per point, so usually minified
            // 2x: one extra level. Pieces are at least 8 px apart.
            .mip_levels = 2,
        }
    ).object;

    const auto image = [&](const char* name) { return Image::from(sheet, sheet.sprite(name), DENSITY); };

    UISheet a;
    a.eagle   = image("eagle");
    a.plate   = { image("plate"),  { 8.0f, 8.0f, 8.0f, 8.0f } };
    a.slot    = { image("slot"),   { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.rim     = { image("rim"),    { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.button  = { image("button"), { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.divider = image("divider");
    a.fill    = image("fill");

    for (std::size_t i = 0; i < a.menu_icons.size(); i++) a.menu_icons[i] = image(MENU_ICON_NAMES[i]);
    for (std::size_t i = 0; i < a.item_icons.size(); i++) a.item_icons[i] = image(ITEM_ICON_NAMES[i]);
    return a;
}

Nezumi::UI::SlotStyle UISheet::item_slot_style(glm::vec2 slot_size, float gap) const {
    return {
        .slot_size   = slot_size,
        .spacing     = { gap, gap },
        .frame       = slot,
        .frame_over  = rim,
        .icon_inset  = 2.0f,
        .icon_radius = 2.0f,
        .label_size  = 11.0f,
        .label_color = { 0.86f, 0.90f, 0.96f, 1.0f },
        .hover       = { 0.75f, 0.88f, 1.00f, 0.14f },
        .drop_hover  = { 0.75f, 0.88f, 1.00f, 0.28f },
        .highlight   = { 0.55f, 0.78f, 1.00f, 0.70f },
    };
}

} // namespace Nightmourn
