// =============================================================================
// GUI/UIAtlas.cpp
// =============================================================================

#include "Nightmourn/GUI/UIAtlas.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"

namespace Nightmourn {

UIAtlas UIAtlas::load(Nezumi::Render::Renderer2D& renderer) {
    using Nezumi::UI::Image;
    namespace P = UIAtlasPixels;

    const Nezumi::Render::Atlas& atlas = *renderer.create_atlas(
        Nezumi::Render::AtlasConfig{
            .name     = "ui-atlas",
            .filepath = "assets/graphics/ui.png",
        }
    ).object;

    const auto image = [&](glm::vec4 box) { return Image::from(atlas, box, DENSITY); };

    UIAtlas a;
    a.eagle   = image(P::Eagle);
    a.plate   = { image(P::Plate),  { 8.0f, 8.0f, 8.0f, 8.0f } };
    a.slot    = { image(P::Slot),   { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.rim     = { image(P::Rim),    { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.button  = { image(P::Button), { 4.0f, 4.0f, 4.0f, 4.0f } };
    a.divider = image(P::Divider);
    a.fill    = image(P::Fill);

    for (std::size_t i = 0; i < a.menu_icons.size(); i++) {
        const glm::vec2 at = P::MenuIcons + glm::vec2(48.0f * static_cast<float>(i), 0.0f);
        a.menu_icons[i] = image({ at, at + glm::vec2(40.0f) });
    }
    for (std::size_t i = 0; i < a.item_icons.size(); i++) {
        const glm::vec2 at = P::ItemIcons + glm::vec2(80.0f * static_cast<float>(i), 0.0f);
        a.item_icons[i] = image({ at, at + glm::vec2(72.0f) });
    }
    return a;
}

Nezumi::UI::SlotStyle UIAtlas::item_slot_style(glm::vec2 slot_size, float gap) const {
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
