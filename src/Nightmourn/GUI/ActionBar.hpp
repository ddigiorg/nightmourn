// =============================================================================
// GUI/ActionBar.hpp
//
// The player's action bar, centered along the bottom of the screen: a row of
// item slots and the menu buttons, drawn as the same see-through slots as
// every menu (Theme.cpp), with no frame of their own.
//
//   [ 1 ][ 2 ][ 3 ] ... [ 0 ]   [ch][in][st][it][mm]
//
// Slots hold items (ItemID, empty for none). Each slot is an input
// action (Action::use_slot), shown by its key in the slot's corner, so
// rebinding a key relabels its slot. The player uses a slot with its key or
// a click, rearranges slots by dragging icons between them, and empties one
// by dragging its icon off the bar. The menu buttons toggle GameMenus.
//
// A slot is a shortcut to an item, not the item itself (Factorio's
// quickbar): dragging a stack from the player's inventory onto a slot points
// the slot at that item, and the stack stays in the inventory. Stacks from
// any other inventory (a settlement's) aren't taken (NOT_A_SHORTCUT). Given the inventory,
// each slot shows how many of its item the player carries, and dims when
// there are none left.
//
//   on_tick:  action_bar_.handle_input(engine.input());
//             for (std::size_t slot : action_bar_.take_used()) ...use it...
//   on_draw:  action_bar_.draw(*engine.ui(), engine.input(), menus_, hero, &inventory);
//
// Above the bar's left end, the crafting queue: a small icon per queued
// recipe (with how many, when more than one), the one being made shaded
// for the time it has left and red when it's done but there's no room for
// it. Click an icon to cancel one craft, right click for five; the game
// takes the cancels and hands them to the queue.
//
//   action_bar_.set_crafting(&player_.crafting(), craft_speed); // each frame
//   ...on_tick:
//   for (const CraftCancel& c : action_bar_.take_cancels()) queue.cancel(c.entry, c.count, here);
//
// Fixed size, in UI points: it grows with the display scale like the rest
// of the UI, not with the window.
// =============================================================================

#pragma once
#include "Nightmourn/Crafting/CraftQueue.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nezumi/UI/Rect.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <array>    // array
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t, uint32_t, uint64_t
#include <optional> // optional
#include <span>     // span
#include <string>   // string
#include <vector>   // vector

namespace Nezumi {
    class InputMap;
}

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class Character;
class GameMenus;
class Inventory;

// A click on the crafting queue: cancel `count` crafts of entry `entry`
struct CraftCancel {
    std::size_t   entry = 0;
    std::uint32_t count = 1;
};

class ActionBar {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr std::size_t SLOT_COUNT = 10;

    // Queue entries shown above the bar (any more wait unseen), and how
    // many a right click on one cancels
    static constexpr std::size_t   QUEUE_ICONS   = 16;
    static constexpr std::uint32_t CANCEL_MANY   = 5;

    // The menus with a button, left to right: options open from the main
    // menu, and the debug menu from its key only
    static constexpr std::array<Menu, 5> BUTTONS = {
        Menu::Character, Menu::Inventory, Menu::Settlement, Menu::Items, Menu::Main,
    };

    // UI drag type of anything that can go on the bar (items for now). The
    // inventory menu drags with it too, so its items drop onto slots.
    static constexpr std::uint32_t DRAG_TYPE = 1;

    // Set in a dragged item's DragPayload::value when it can't become a
    // shortcut: the settlement's stock drags between inventories like the
    // player's, but the bar only points at what the player carries
    static constexpr std::uint64_t NOT_A_SHORTCUT = std::uint64_t{ 1 } << 63;

    // -------------------------------------------------------------------------
    // Construction
    //
    // `sheet` must outlive the bar.
    // -------------------------------------------------------------------------

    explicit ActionBar(const UISheet& sheet) : sheet_(&sheet) {}

    // -------------------------------------------------------------------------
    // Slots
    // -------------------------------------------------------------------------

    std::optional<ItemID> slot(std::size_t i) const { return slots_[i]; }
    void set_slot(std::size_t i, std::optional<ItemID> item) { slots_[i] = item; }

    // Puts `item` in the first empty slot, unless a slot already has it.
    // False if it's on the bar nowhere afterwards (no empty slot).
    bool add_shortcut(ItemID item);

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    // Call from on_tick: slots whose key was pressed are used
    void handle_input(const Nezumi::InputMap& input);

    // Slots used since the last call, by key or click, oldest first. Empty
    // slots are never reported.
    std::vector<std::size_t> take_used();

    // -------------------------------------------------------------------------
    // Crafting queue
    // -------------------------------------------------------------------------

    // The queue to show (null: none) and the crafter's speed, for its
    // times. `queue` must outlive the bar.
    void set_crafting(const CraftQueue* queue, float speed = 1.0f) { queue_ = queue; craft_speed_ = speed; }

    // Cancels clicked since the last call, oldest first
    std::vector<CraftCancel> take_cancels();

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw. `hero` is null while the hero is dead. Clicking a
    // menu button toggles its menu in `menus`. With an `inventory`, slots
    // show their item's count in it.
    // -------------------------------------------------------------------------

    void draw(Nezumi::UI::Context& ui, const Nezumi::InputMap& input,
              GameMenus& menus, const Character* hero,
              const Inventory* inventory = nullptr);

    // Where the bar was last drawn, UI units (empty before its first draw)
    Nezumi::UI::Rect rect() const { return rect_; }

private:
    void use_(std::size_t i);
    void draw_slots_   (Nezumi::UI::Context& ui, const Nezumi::InputMap& input, const Inventory* inventory);
    void draw_buttons_ (Nezumi::UI::Context& ui, const Nezumi::InputMap& input, GameMenus& menus);
    void draw_queue_   (Nezumi::UI::Context& ui);

    const UISheet* sheet_ = nullptr;

    std::array<std::optional<ItemID>, SLOT_COUNT> slots_  = {};
    std::array<std::uint8_t, SLOT_COUNT>                  flash_  = {}; // frames left drawn pressed
    std::vector<std::size_t>                              used_   = {};
    std::array<std::array<char, 8>, SLOT_COUNT>           counts_ = {}; // slot count text, for Slot::count

    Nezumi::UI::Rect rect_ = {};

    const CraftQueue*        queue_       = nullptr;
    float                    craft_speed_ = 1.0f;
    std::vector<CraftCancel> cancels_     = {};
    std::array<std::array<char, 8>, QUEUE_ICONS> queue_counts_ = {};
    std::string              queue_tip_   = {};
};

} // namespace Nightmourn
