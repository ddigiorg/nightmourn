// =============================================================================
// GUI/UISheet.hpp
//
// Nightmourn's UI art: one sprite sheet, assets/graphics/ui.png and its
// manifest ui.json, both made by tools/gen_ui.py (placeholder art until
// hand-drawn sprites replace it). Pieces are looked up by name ("eagle",
// "menu/skills", "item/wood", ...). Painted at 2x, so every image is drawn at
// density 2: a 160x120 px eagle is 80x60 UI points.
//
//   ui_sheet_ = Nightmourn::UISheet::load(*engine.renderer2d());
//   ui.draw_image(rect, ui_sheet_.eagle);
// =============================================================================

#pragma once
#include "Nezumi/UI/Image.hpp"
#include "Nezumi/UI/SlotGrid.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <cstdint>     // uint8_t
#include <glm/glm.hpp> // vec2

namespace Nezumi::Render {
    class Renderer2D;
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

struct UISheet {
    static constexpr float DENSITY = 2.0f; // sheet pixels per UI point

    // Sprite names of the menu glyphs, by Menu, and of the item icons, by
    // ItemID (Items::Wood, Stone, Gold, Food)
    static constexpr std::array<const char*, MENU_COUNT> MENU_ICON_NAMES = {
        "menu/skills", "menu/equipment", "menu/inventory", "menu/items",
        "menu/options", "menu/debug", "menu/main",
    };
    static constexpr std::array<const char*, 4> ITEM_ICON_NAMES = {
        "item/wood", "item/stone", "item/gold", "item/food",
    };

    Nezumi::UI::Image     eagle   = {}; // crouched, faces right
    Nezumi::UI::NineSlice plate   = {};
    Nezumi::UI::NineSlice slot    = {}; // socket, under a slot's icon
    Nezumi::UI::NineSlice rim     = {}; // bevel, over a slot's icon
    Nezumi::UI::NineSlice button  = {};
    Nezumi::UI::Image     divider = {};
    Nezumi::UI::Image     fill    = {}; // white bar fill, tinted when drawn

    std::array<Nezumi::UI::Image, MENU_COUNT>                     menu_icons = {};
    std::array<Nezumi::UI::Image, ITEM_ICON_NAMES.size()>        item_icons = {};

    const Nezumi::UI::Image& menu_icon(Menu menu) const {
        return menu_icons[static_cast<std::size_t>(menu)];
    }

    // No image for an item without an icon yet
    Nezumi::UI::Image item_icon(ItemID item) const {
        return item.value() < item_icons.size() ? item_icons[item.value()] : Nezumi::UI::Image{};
    }

    // How item slots look (the action bar's, the inventory's): the socket
    // and rim frames, with Nightmourn's hover colors
    Nezumi::UI::SlotStyle item_slot_style(glm::vec2 slot_size, float gap) const;

    // Creates the sheet through the renderer (which owns it) and fills in
    // every image. Throws if ui.json or ui.png can't be loaded, or lacks a
    // piece.
    static UISheet load(Nezumi::Render::Renderer2D& renderer);
};

} // namespace Nightmourn
