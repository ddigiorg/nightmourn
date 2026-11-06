// =============================================================================
// GUI/UISheet.cpp
// =============================================================================

#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nezumi/Render/2D/Sprite/SpritePacker.hpp"
#include "Nezumi/Render/Core/Image/Image.hpp"
#include <stdexcept> // runtime_error

namespace Nightmourn {

UISheet UISheet::load(Nezumi::Render::Renderer2D& renderer, const Configs& configs) {
    using Nezumi::UI::Image;
    namespace NR = Nezumi::Render;

    // Every sprite config, untrimmed: pieces are measured by their full
    // image (nine-slice borders, icon sizes)
    NR::SpritePacker packer;
    for (const SpriteConfig& s : configs.sprites) {
        packer.add_sprite(s.name, NR::load_image(s.filename), { .pivot = s.pivot, .trim = false });
    }
    // Item icons, as "item/<name>"
    for (const ItemPrototype& item : configs.items) {
        if (!item.icon.empty())
            packer.add_sprite("item/" + item.name, NR::load_image(item.icon), { .trim = false });
    }

    // Drawn at DENSITY sheet pixels per point, so usually minified 2x: one
    // extra level (the packer leaves 2 px between pieces for it)
    const NR::SpriteSheet& sheet = *renderer.create_sprite_sheet({ .name = "ui", .mip_levels = 2 }, packer).object;

    const auto image = [&](const char* name) { return Image::from(sheet, sheet.sprite(name), DENSITY); };
    const auto slice = [&](const char* name) -> Nezumi::UI::NineSlice {
        return { image(name), configs.find_sprite(name)->border };
    };

    UISheet a;
    a.eagle   = image("eagle");
    a.plate   = slice("plate");
    a.slot    = slice("slot");
    a.rim     = slice("rim");
    a.button  = slice("button");
    a.divider = image("divider");
    a.fill    = image("fill");

    for (std::size_t i = 0; i < a.menu_icons.size(); i++) a.menu_icons[i] = image(MENU_ICON_NAMES[i]);
    for (const ItemPrototype& item : configs.items) {
        a.item_icons.push_back(item.icon.empty() ? Image{} : image(("item/" + item.name).c_str()));
    }
    return a;
}

Nezumi::UI::SlotStyle UISheet::item_slot_style(glm::vec2 slot_size, float gap) const {
    return {
        .slot_size   = slot_size,
        .spacing     = { gap, gap },
        // No frame: Nezumi draws the theme's see-through rounded slot
        // (Theme.cpp), its border lit while hovered
        .icon_inset  = 3.0f,
        .label_size  = 11.0f,
        .label_color = { 0.86f, 0.90f, 0.96f, 1.0f },
        .hover       = { 0.78f, 0.83f, 0.90f, 0.10f },
        .drop_hover  = { 0.78f, 0.83f, 0.90f, 0.22f },
    };
}

} // namespace Nightmourn
