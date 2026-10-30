// =============================================================================
// GUI/GameMenus.cpp
// =============================================================================

#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nezumi/UI/Context.hpp"
#include <cstddef> // size_t

namespace Nightmourn {

using Nezumi::UI::PanelConfig;
using Nezumi::UI::PanelFlags;

std::string_view menu_title(Menu menu) {
    switch (menu) {
        case Menu::Skills:    return "Skills";
        case Menu::Equipment: return "Equipment";
        case Menu::Inventory: return "Inventory";
        case Menu::Items:     return "Items";
        case Menu::Options:   return "Options";
        case Menu::Debug:     return "Debug options";
        case Menu::Main:      return "Main menu";
    }
    return "Menu";
}

Nezumi::ActionID menu_action(Menu menu) {
    switch (menu) {
        case Menu::Skills:    return Action::ToggleSkillsMenu;
        case Menu::Equipment: return Action::ToggleEquipmentMenu;
        case Menu::Inventory: return Action::ToggleInventoryMenu;
        case Menu::Items:     return Action::ToggleItemsMenu;
        case Menu::Options:   return Action::ToggleOptionsMenu;
        case Menu::Debug:     return Action::ToggleDebugMenu;
        case Menu::Main:      return Action::ToggleMainMenu;
    }
    return Nezumi::ActionID{};
}

// -----------------------------------------------------------------------------
// Open / Close
// -----------------------------------------------------------------------------

bool GameMenus::is_open(Menu menu) const {
    if (menu == Menu::Debug) return debug_.is_open();
    return open_[static_cast<std::size_t>(menu)];
}

void GameMenus::set_open(Menu menu, bool open) {
    if (menu == Menu::Debug) debug_.set_open(open);
    else                     open_[static_cast<std::size_t>(menu)] = open;
}

bool GameMenus::any_open() const {
    for (std::size_t i = 0; i < MENU_COUNT; i++)
        if (is_open(static_cast<Menu>(i))) return true;
    return false;
}

void GameMenus::close_all() {
    for (std::size_t i = 0; i < MENU_COUNT; i++) set_open(static_cast<Menu>(i), false);
}

void GameMenus::back() {
    if (any_open()) close_all();
    else            set_open(Menu::Main, true);
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void GameMenus::handle_input(const Nezumi::InputMap& input) {
    for (std::size_t i = 0; i < MENU_COUNT; i++) {
        const Menu menu = static_cast<Menu>(i);
        if (!input.was_pressed(menu_action(menu))) continue;
        if (menu == Menu::Main) back();
        else                    toggle(menu);
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void GameMenus::draw(Nezumi::UI::Context& ui, DebugFlags& flags, InventoryMenu* inventory) {
    if (is_open(Menu::Skills))    draw_stub_(ui, Menu::Skills,    "Skills you have learned, and the ones still to learn.");
    if (is_open(Menu::Equipment)) draw_stub_(ui, Menu::Equipment, "What your hero wears and wields.");
    if (is_open(Menu::Inventory)) {
        if (inventory) inventory->draw(ui, open_[static_cast<std::size_t>(Menu::Inventory)]);
        else           draw_stub_(ui, Menu::Inventory, "What your hero carries.");
    }
    if (is_open(Menu::Items))     draw_stub_(ui, Menu::Items,     "Every item you have discovered, and how to make it.");
    if (is_open(Menu::Options))   draw_stub_(ui, Menu::Options,   "Video, sound and key bindings.");
    debug_.draw(ui, flags);
    if (is_open(Menu::Main))      draw_main_(ui);
}

void GameMenus::draw_stub_(Nezumi::UI::Context& ui, Menu menu, std::string_view about) {
    // Cascaded from the top-left the first time, so several open at once
    // don't stack exactly on top of each other
    const float k = static_cast<float>(menu);
    const PanelConfig config = {
        .position = { 48.0f + 28.0f * k, 120.0f + 24.0f * k },
        .flags    = PanelFlags::Movable | PanelFlags::AutoSize,
    };

    bool& open = open_[static_cast<std::size_t>(menu)];
    if (!ui.begin_panel(menu_title(menu), &open, config)) return;
    ui.text(about);
    ui.text("Coming soon.", ui.style().text_dim);
    ui.end_panel();
}

void GameMenus::draw_main_(Nezumi::UI::Context& ui) {
    const PanelConfig config = {
        .position = ui.screen_size() * 0.5f,
        .pivot    = { 0.5f, 0.5f },
        .min_size = { 220.0f, 0.0f },
        .flags    = PanelFlags::AutoSize | PanelFlags::PinPosition,
    };

    bool& open = open_[static_cast<std::size_t>(Menu::Main)];
    if (!ui.begin_panel("Nightmourn", &open, config)) return;
    if (ui.button("Resume",  -1.0f)) open = false;
    if (ui.button("Options", -1.0f)) { open = false; set_open(Menu::Options, true); }
    ui.separator();
    if (ui.button("Quit",    -1.0f)) quit_ = true;
    ui.end_panel();
}

} // namespace Nightmourn
