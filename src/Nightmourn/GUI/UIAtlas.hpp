// =============================================================================
// GUI/UIAtlas.hpp
//
// Nightmourn's UI art: one atlas, assets/graphics/ui.png, made by
// tools/gen_ui.py (placeholder art until hand-drawn sprites replace it at
// the same pixel boxes). Painted at 2x, so every image is drawn at density 2:
// a 160x120 px eagle is 80x60 UI points.
//
//   ui_atlas_ = Nightmourn::UIAtlas::load(*engine.renderer());
//   ui.draw_image(rect, ui_atlas_.eagle);
//
// The pixel boxes below and the ones in tools/gen_ui.py must match.
// =============================================================================

#pragma once
#include "Nezumi/UI/Image.hpp"
#include "Nezumi/World/Item/ItemID.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <cstdint>     // uint8_t
#include <glm/glm.hpp> // vec4

namespace Nezumi::Render {
    class Renderer;
}

namespace Nightmourn {

// The top-level menus, in the order of their buttons on the action bar
enum class Menu : std::uint8_t {
    Skills,
    Equipment,
    Inventory,
    Items,     // codex of every item type discovered, with recipes
    Options,
    Debug,
    Main,
};
inline constexpr std::size_t MENU_COUNT = 7;

namespace UIAtlasPixels {
    // { x0, y0, x1, y1 } in assets/graphics/ui.png, y down
    constexpr glm::vec4 Eagle   = {   0.0f,   0.0f,  160.0f, 120.0f }; // crouched, faces right
    constexpr glm::vec4 Plate   = { 168.0f,   0.0f, 1318.0f, 100.0f };
    constexpr glm::vec4 Slot    = {   0.0f, 128.0f,   72.0f, 200.0f };
    constexpr glm::vec4 Rim     = {  80.0f, 128.0f,  152.0f, 200.0f };
    constexpr glm::vec4 Button  = { 160.0f, 128.0f,  204.0f, 192.0f }; // rectangular, 22x32 pt
    constexpr glm::vec4 Divider = { 212.0f, 128.0f,  228.0f, 200.0f };
    constexpr glm::vec4 Fill    = { 236.0f, 128.0f,  252.0f, 142.0f };

    // One 40x40 px glyph per Menu, 48 px apart; one 72x72 px icon per item
    // (Items::Wood, Stone, Gold, Food), 80 px apart
    constexpr glm::vec2 MenuIcons = { 260.0f, 128.0f };
    constexpr glm::vec2 ItemIcons = { 604.0f, 128.0f };
    constexpr std::size_t ITEM_ICON_COUNT = 4;
}

struct UIAtlas {
    static constexpr float DENSITY = 2.0f; // atlas pixels per UI point

    Nezumi::UI::Image     eagle   = {};
    Nezumi::UI::NineSlice plate   = {};
    Nezumi::UI::NineSlice slot    = {}; // socket, under a slot's icon
    Nezumi::UI::NineSlice rim     = {}; // bevel, over a slot's icon
    Nezumi::UI::NineSlice button  = {};
    Nezumi::UI::Image     divider = {};
    Nezumi::UI::Image     fill    = {}; // white bar fill, tinted when drawn

    std::array<Nezumi::UI::Image, MENU_COUNT>                     menu_icons = {};
    std::array<Nezumi::UI::Image, UIAtlasPixels::ITEM_ICON_COUNT> item_icons = {};

    const Nezumi::UI::Image& menu_icon(Menu menu) const {
        return menu_icons[static_cast<std::size_t>(menu)];
    }

    // No image for an item without an icon yet
    Nezumi::UI::Image item_icon(Nezumi::ItemID item) const {
        return item.value() < item_icons.size() ? item_icons[item.value()] : Nezumi::UI::Image{};
    }

    // Creates the atlas through the renderer (which owns it) and fills in
    // every image. Throws if ui.png can't be loaded.
    static UIAtlas load(Nezumi::Render::Renderer& renderer);
};

} // namespace Nightmourn
