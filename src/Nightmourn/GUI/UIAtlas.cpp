// =============================================================================
// GUI/UIAtlas.cpp
// =============================================================================

#include "Nightmourn/GUI/UIAtlas.hpp"
#include "Nezumi/Render/Renderer.hpp"

namespace Nightmourn {

UIAtlas UIAtlas::load(Nezumi::Render::Renderer& renderer) {
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
    a.plate   = { image(P::Plate),  { 24.0f, 20.0f, 24.0f, 20.0f } };
    a.slot    = { image(P::Slot),   {  8.0f,  8.0f,  8.0f,  8.0f } };
    a.rim     = { image(P::Rim),    {  8.0f,  8.0f,  8.0f,  8.0f } };
    a.button  = { image(P::Button), {  6.0f,  6.0f,  6.0f,  6.0f } };
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

} // namespace Nightmourn
