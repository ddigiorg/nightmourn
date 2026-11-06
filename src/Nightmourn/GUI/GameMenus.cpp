// =============================================================================
// GUI/GameMenus.cpp
// =============================================================================

#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/ItemsMenu.hpp"
#include "Nightmourn/GUI/SettlementMenu.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nezumi/UI/Context.hpp"
#include <cstddef> // size_t

namespace Nightmourn {

using Nezumi::UI::PanelConfig;
using Nezumi::UI::PanelFlags;

std::string_view menu_title(Menu menu) {
    switch (menu) {
        case Menu::Character: return "Character";
        case Menu::Inventory: return "Inventory";
        case Menu::Settlement: return SettlementMenu::TITLE;
        case Menu::Crafting:  return "Crafting";
        case Menu::Options:   return "Options";
        case Menu::Debug:     return "Debug options";
        case Menu::Main:      return "Main Menu";
    }
    return "Menu";
}

Nezumi::ActionID menu_action(Menu menu) {
    switch (menu) {
        case Menu::Character: return Action::ToggleCharacterMenu;
        case Menu::Inventory: return Action::ToggleInventoryMenu;
        case Menu::Settlement: return Action::ToggleSettlementMenu;
        case Menu::Crafting:  return Action::ToggleCraftingMenu;
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
    move_mod_     = input.is_held(Action::InventoryMoveMod);
    shortcut_mod_ = input.is_held(Action::InventoryShortcutMod);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void GameMenus::draw(Nezumi::UI::Context& ui, DebugFlags& flags, const GameMenuPanels& panels) {
    if (is_open(Menu::Character)) draw_stub_(ui, Menu::Character, "Your hero's skills, and what they wear and wield.");

    // The two inventories, each the other's place to send stacks while both
    // are open and in reach (the settlement's only on its Inventory tab)
    InventoryMenu* player = is_open(Menu::Inventory)  && panels.inventory  && panels.inventory->inventory()
                          ? panels.inventory : nullptr;
    InventoryMenu* stock  = is_open(Menu::Settlement) && panels.settlement
                         && panels.settlement->tab() == SettlementMenu::InventoryTab
                         && panels.settlement->inventory().inventory()
                          ? &panels.settlement->inventory() : nullptr;
    const InventoryMenu::Modifiers mods = { .move = move_mod_, .shortcut = shortcut_mod_ };
    if (is_open(Menu::Inventory)) {
        if (panels.inventory) panels.inventory->draw(ui, open_[static_cast<std::size_t>(Menu::Inventory)], stock, mods);
        else                  draw_stub_(ui, Menu::Inventory, "What your hero carries.");
    }
    if (is_open(Menu::Settlement)) {
        if (panels.settlement) panels.settlement->draw(ui, open_[static_cast<std::size_t>(Menu::Settlement)], player, mods);
        else                   draw_stub_(ui, Menu::Settlement, "What your settlement keeps in store, and makes.");
    }
    if (is_open(Menu::Crafting)) {
        if (panels.items) panels.items->draw(ui, open_[static_cast<std::size_t>(Menu::Crafting)], mods);
        else              draw_stub_(ui, Menu::Crafting, "Every item you have discovered, and how to make it.");
    }
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
