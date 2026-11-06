// =============================================================================
// GUI/ActionBar.cpp
// =============================================================================

#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nezumi/Input/KeyName.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // clamp, max, swap
#include <cmath>     // round
#include <cstdio>    // snprintf
#include <string>    // string

namespace Nightmourn {

using Nezumi::UI::Rect;
using glm::vec2;
using glm::vec4;

// -----------------------------------------------------------------------------
// Layout
//
// UI points, from the bar's top-left corner: the slots, a gap, then the menu
// buttons drawn as slots of the same size, all floating over the world with
// no frame of their own (Theme.cpp's slot colors).
// -----------------------------------------------------------------------------

namespace {
    constexpr vec2  SLOT_SIZE     = { 36.0f, 36.0f };
    constexpr float SLOT_GAP      = 4.0f;
    constexpr float SLOTS_W       = ActionBar::SLOT_COUNT * SLOT_SIZE.x + (ActionBar::SLOT_COUNT - 1) * SLOT_GAP;

    constexpr float BUTTONS_GAP   = 12.0f; // between the slots and the buttons
    constexpr float BUTTON_COUNT  = static_cast<float>(ActionBar::BUTTONS.size());
    constexpr float BUTTONS_X     = SLOTS_W + BUTTONS_GAP;
    constexpr float ICON_SIZE     = 22.0f;

    constexpr vec2  BAR_SIZE      = { BUTTONS_X + BUTTON_COUNT * SLOT_SIZE.x + (BUTTON_COUNT - 1) * SLOT_GAP, SLOT_SIZE.y };
    constexpr float BAR_MARGIN    = 8.0f;  // above the screen's bottom edge

    // Colors
    constexpr vec4 ICON          = { 0.80f, 0.85f, 0.92f, 1.00f };
    constexpr vec4 ICON_HOVER    = { 0.96f, 0.98f, 1.00f, 1.00f };

    constexpr std::uint8_t FLASH_FRAMES = 8; // a used slot shows pressed this long

    // The crafting queue: smaller icons than the slots, in a row over the
    // bar's left end
    constexpr vec2  QUEUE_ICON    = { 26.0f, 26.0f };
    constexpr float QUEUE_GAP     = 3.0f;
    constexpr float QUEUE_RAISE   = 6.0f; // above the bar's top
    constexpr vec4  QUEUE_BLOCKED = { 0.85f, 0.12f, 0.10f, 0.45f };
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void ActionBar::handle_input(const Nezumi::InputMap& input) {
    for (std::size_t i = 0; i < SLOT_COUNT; i++)
        if (input.was_pressed(Action::use_slot(i))) use_(i);
}

std::vector<std::size_t> ActionBar::take_used() {
    std::vector<std::size_t> used;
    used.swap(used_);
    return used;
}

std::vector<CraftCancel> ActionBar::take_cancels() {
    std::vector<CraftCancel> out;
    out.swap(cancels_);
    return out;
}

bool ActionBar::add_shortcut(ItemID item) {
    for (const auto& s : slots_)
        if (s == item) return true;
    for (auto& s : slots_) {
        if (s) continue;
        s = item;
        return true;
    }
    return false;
}

void ActionBar::use_(std::size_t i) {
    flash_[i] = FLASH_FRAMES;
    if (slots_[i]) used_.push_back(i);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ActionBar::draw(Nezumi::UI::Context& ui, const Nezumi::InputMap& input,
                     GameMenus& menus, const Character* hero, const Inventory* inventory)
{
    using Nezumi::UI::PanelFlags;

    // Bottom-center, every frame, so it follows window resizes. The hero's
    // health isn't shown here for now.
    (void)hero;
    const Nezumi::UI::PanelConfig config = {
        .position = { ui.screen_size().x * 0.5f, ui.screen_size().y - BAR_MARGIN },
        .pivot    = { 0.5f, 1.0f },
        .size     = BAR_SIZE,
        .min_size = BAR_SIZE,
        .flags    = PanelFlags::NoTitleBar | PanelFlags::NoBackground | PanelFlags::PinPosition,
    };
    if (!ui.begin_panel("##action-bar", config)) return;

    rect_ = Rect::from_size(ui.content_rect().min, BAR_SIZE);
    draw_slots_(ui, input, inventory);
    draw_buttons_(ui, input, menus);

    ui.end_panel();

    draw_queue_(ui);

    for (std::uint8_t& f : flash_) if (f > 0) f--;
}

void ActionBar::draw_queue_(Nezumi::UI::Context& ui) {
    using Nezumi::UI::PanelFlags;
    if (!queue_ || queue_->empty()) return;

    const std::span<const CraftQueue::Entry> entries = queue_->entries();
    const std::size_t n = std::min(entries.size(), QUEUE_ICONS);
    const vec2 size = { static_cast<float>(n) * (QUEUE_ICON.x + QUEUE_GAP) - QUEUE_GAP, QUEUE_ICON.y };

    const Nezumi::UI::PanelConfig config = {
        .position = { rect_.min.x, rect_.min.y - QUEUE_RAISE },
        .pivot    = { 0.0f, 1.0f },
        .size     = size,
        .min_size = size,
        .flags    = PanelFlags::NoTitleBar | PanelFlags::NoBackground | PanelFlags::PinPosition,
    };
    if (!ui.begin_panel("##craft-queue", config)) return;
    const vec2 origin = ui.content_rect().min;

    // An icon per entry, by what it makes; the front one shaded for the
    // time it has left
    std::array<Nezumi::UI::Slot, QUEUE_ICONS> slots;
    for (std::size_t i = 0; i < n; i++) {
        const CraftQueue::Entry& e = entries[i];
        Nezumi::UI::Slot&        s = slots[i];
        s.icon = sheet_->item_icon(e.recipe->results.front().item);
        if (e.count > 1) {
            std::snprintf(queue_counts_[i].data(), queue_counts_[i].size(), "%u", static_cast<unsigned>(e.count));
            s.count = queue_counts_[i].data();
        }
        if (i == 0) s.cooldown = queue_->blocked() ? 0.0f : 1.0f - queue_->progress();
    }

    Nezumi::UI::SlotStyle style = sheet_->item_slot_style(QUEUE_ICON, QUEUE_GAP);
    style.label_size = 10.0f;
    const Nezumi::UI::SlotGridResult r =
        ui.slot_grid("queue", std::span(slots.data(), n), static_cast<int>(n), style);

    // Done but no room: red over it
    if (queue_->blocked()) ui.draw_rect(Rect::from_size(origin, QUEUE_ICON), QUEUE_BLOCKED, 3.0f);

    if (r.clicked       >= 0) cancels_.push_back({ static_cast<std::size_t>(r.clicked), 1 });
    if (r.right_clicked >= 0) cancels_.push_back({ static_cast<std::size_t>(r.right_clicked), CANCEL_MANY });

    // "Wooden arrow x10 (3 queued)
    //  Being made: 0.4 s left, 1 s each
    //  2.4 s for the whole queue
    //  Click: cancel 1   Right click: cancel 5"
    //
    // Counts down live while hovered (the UI times a tooltip per widget, so
    // it stays up as its text changes)
    if (r.hovered >= 0) {
        const std::size_t        i = static_cast<std::size_t>(r.hovered);
        const CraftQueue::Entry& e = entries[i];
        const ItemAmount&     made = e.recipe->results.front();
        char line[128];

        queue_tip_ = display_name(item_name(made.item));
        if (made.count > 1) {
            std::snprintf(line, sizeof line, " x%u", static_cast<unsigned>(made.count));
            queue_tip_ += line;
        }
        if (e.count > 1) {
            std::snprintf(line, sizeof line, " (%u queued)", static_cast<unsigned>(e.count));
            queue_tip_ += line;
        }

        const double each = static_cast<double>(CraftQueue::ticks_for(*e.recipe, craft_speed_)) / TICKS_PER_SECOND;
        if (i == 0 && queue_->blocked()) {
            queue_tip_ += "\nDone, but there's no room in your inventory";
        } else if (i == 0 && queue_->ticks_left() > 0) {
            const double left = static_cast<double>(queue_->ticks_left()) / TICKS_PER_SECOND;
            std::snprintf(line, sizeof line, "\nBeing made: %.1f s left, %g s each", left, each);
            queue_tip_ += line;
        } else {
            std::snprintf(line, sizeof line, "\n%g s each", each);
            queue_tip_ += line;
        }
        if (queue_->size() > 1) {
            const double all = static_cast<double>(queue_->total_ticks_left(craft_speed_)) / TICKS_PER_SECOND;
            std::snprintf(line, sizeof line, "\n%.1f s for the whole queue", all);
            queue_tip_ += line;
        }
        queue_tip_ += "\nClick: cancel 1   Right click: cancel 5";
        ui.tooltip(queue_tip_);
    }

    ui.end_panel();
}

void ActionBar::draw_slots_(Nezumi::UI::Context& ui, const Nezumi::InputMap& input,
                            const Inventory* inventory)
{
    std::array<Nezumi::UI::Slot, SLOT_COUNT> slots;
    for (std::size_t i = 0; i < SLOT_COUNT; i++) {
        Nezumi::UI::Slot& s = slots[i];
        if (slots_[i]) {
            s.icon  = sheet_->item_icon(*slots_[i]);
            s.value = slots_[i]->value();

            // How many the player carries; none: dimmed, but still a shortcut
            if (inventory) {
                const unsigned n = inventory->count(*slots_[i]);
                std::snprintf(counts_[i].data(), counts_[i].size(), "%u", n);
                s.count    = counts_[i].data();
                s.disabled = n == 0;
            }
        }
        if (const auto key = input.first_binding(Action::use_slot(i)))
            s.label = Nezumi::input_name(*key);
        s.flash = flash_[i] > 0 || input.is_held(Action::use_slot(i));
    }

    // A stack that can't be a shortcut doesn't light the slots up
    Nezumi::UI::SlotStyle style = sheet_->item_slot_style(SLOT_SIZE, SLOT_GAP);
    const Nezumi::UI::DragPayload* drag = ui.drag_payload();
    if (drag && drag->type == DRAG_TYPE && (drag->value & NOT_A_SHORTCUT)) style.drop_hover = {};

    ui.set_next_pos({ 0.0f, 0.0f });
    const Nezumi::UI::Context::ID grid = ui.get_id("slots");
    const Nezumi::UI::SlotGridResult r = ui.slot_grid("slots", slots, SLOT_COUNT, style, DRAG_TYPE);

    if (r.clicked >= 0) use_(static_cast<std::size_t>(r.clicked));

    // A drop from another slot swaps the two; from elsewhere (the inventory)
    // it points this slot at that item
    if (r.dropped >= 0) {
        const std::size_t to = static_cast<std::size_t>(r.dropped);
        if (r.payload.source == grid && r.payload.index >= 0) {
            std::swap(slots_[to], slots_[static_cast<std::size_t>(r.payload.index)]);
        } else if (!(r.payload.value & NOT_A_SHORTCUT)) {
            slots_[to] = ItemID{ static_cast<ItemID::value_t>(r.payload.value) };
        }
    }

    // Dragged off the bar and dropped where nothing took it: the slot empties.
    // A drop from anywhere else is taken and ignored: the inventory keeps a
    // stack dragged out of it, until items can be dropped on the ground.
    if (const auto drop = ui.take_unclaimed_drop(DRAG_TYPE);
        drop && drop->payload.source == grid && drop->payload.index >= 0) {
        slots_[static_cast<std::size_t>(drop->payload.index)].reset();
    }

    if (r.hovered >= 0 && slots_[static_cast<std::size_t>(r.hovered)])
        ui.tooltip(display_name(item_name(*slots_[static_cast<std::size_t>(r.hovered)])));
}

void ActionBar::draw_buttons_(Nezumi::UI::Context& ui, const Nezumi::InputMap& input, GameMenus& menus) {
    const vec2 origin = ui.content_rect().min;

    for (std::size_t i = 0; i < BUTTONS.size(); i++) {
        const Menu menu = BUTTONS[i];
        const Rect r = Rect::from_size(
            origin + vec2(BUTTONS_X + static_cast<float>(i) * (SLOT_SIZE.x + SLOT_GAP), 0.0f),
            SLOT_SIZE);

        const auto it   = ui.interact(ui.get_id(menu_title(menu)), r);
        const bool open = menus.is_open(menu);
        if (it.clicked) menus.toggle(menu);

        // A slot like the others; lit while hovered or its menu is open
        const Nezumi::UI::Style& style = ui.style();
        const bool lit = it.hovered || open;
        ui.draw_rect(r, lit ? style.frame_bg_hovered : style.slot_bg, style.slot_rounding,
                     lit ? style.slot_border_hovered : style.slot_border, style.border_width);

        const float push = it.held ? 1.0f : 0.0f;
        const Rect  icon = Rect::from_size(
            r.min + glm::round((SLOT_SIZE - vec2(ICON_SIZE)) * 0.5f) + vec2(0.0f, push), vec2(ICON_SIZE));
        ui.draw_image(icon, sheet_->menu_icon(menu), lit ? ICON_HOVER : ICON);

        // "Inventory (X)"
        char tip[64];
        const auto key = input.first_binding(menu_action(menu));
        const std::string_view title = menu_title(menu);
        const std::string_view name  = key ? Nezumi::input_name(*key) : std::string_view{};
        if (key) std::snprintf(tip, sizeof tip, "%.*s (%.*s)", int(title.size()), title.data(), int(name.size()), name.data());
        else     std::snprintf(tip, sizeof tip, "%.*s", int(title.size()), title.data());
        ui.tooltip(tip);
    }
}

} // namespace Nightmourn
