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
// UI points, from the bar's top-left corner. The slab sits along the bottom,
// with an eagle over each end; everything on the slab lines up with the
// grooves painted into it (tools/gen_ui.py, gen_plate).
// -----------------------------------------------------------------------------

namespace {
    // The eagles stand only a little taller than the slab, crouched over
    // its ends; the slab wastes no room around what it holds
    constexpr vec2  EAGLE_SIZE    = { 80.0f, 60.0f }; // the swept-back wing takes the outer 10
    constexpr float EAGLE_OVERLAP = 6.0f;               // over each end of the slab
    constexpr vec2  PLATE_SIZE    = { 546.0f, 50.0f };
    constexpr float PLATE_X       = EAGLE_SIZE.x - EAGLE_OVERLAP;
    constexpr float PLATE_Y       = EAGLE_SIZE.y - PLATE_SIZE.y;
    constexpr vec2  BAR_SIZE      = { PLATE_SIZE.x + 2.0f * PLATE_X, EAGLE_SIZE.y };

    constexpr float INSET         = 4.0f;               // slab edge to its contents
    constexpr float CONTENT_X     = PLATE_X + INSET;
    constexpr float CONTENT_W     = PLATE_SIZE.x - 2.0f * INSET;

    // A thin hp bar along the top, split into tenths like WoW's xp bar
    constexpr float HP_Y          = PLATE_Y + 3.0f;
    constexpr float HP_H          = 6.0f;
    constexpr int   HP_SEGMENTS   = 10;

    constexpr vec2  SLOT_SIZE     = { 36.0f, 36.0f };
    constexpr float SLOT_GAP      = 3.0f;
    constexpr float SLOTS_Y       = PLATE_Y + 11.0f;
    constexpr float SLOTS_W       = ActionBar::SLOT_COUNT * SLOT_SIZE.x + (ActionBar::SLOT_COUNT - 1) * SLOT_GAP;

    // Rectangular menu buttons, like WoW's micro menu
    constexpr float DIVIDER_W     = 8.0f;
    constexpr vec2  BUTTON        = { 27.0f, 32.0f };
    constexpr float BUTTON_GAP    = 2.0f;
    constexpr float BUTTON_COUNT  = static_cast<float>(ActionBar::BUTTONS.size());
    constexpr float BUTTONS_X     = CONTENT_X + SLOTS_W + DIVIDER_W;
    constexpr float BUTTONS_Y     = SLOTS_Y + (SLOT_SIZE.y - BUTTON.y) * 0.5f;
    constexpr float ICON_SIZE     = 18.0f;

    static_assert(SLOTS_W + DIVIDER_W + BUTTON_COUNT * BUTTON.x + (BUTTON_COUNT - 1) * BUTTON_GAP == CONTENT_W,
                  "slots, divider and buttons fill the slab");
    static_assert(SLOTS_Y + SLOT_SIZE.y + 3.0f == EAGLE_SIZE.y, "3 points under the slots");

    // Colors
    constexpr vec4 HP_TRACK      = { 0.03f, 0.04f, 0.07f, 1.00f };
    constexpr vec4 HP_TICK       = { 0.20f, 0.24f, 0.30f, 1.00f };
    constexpr vec4 HP_BORDER     = { 0.01f, 0.02f, 0.03f, 1.00f };
    constexpr vec4 HP_FILL       = { 0.74f, 0.13f, 0.12f, 1.00f };
    constexpr vec4 HP_TEXT       = { 0.95f, 0.95f, 0.97f, 1.00f };
    constexpr vec4 SHADOW        = { 0.00f, 0.00f, 0.00f, 0.90f };
    constexpr vec4 ICON          = { 0.78f, 0.86f, 0.95f, 1.00f };
    constexpr vec4 ICON_HOVER    = { 0.92f, 0.97f, 1.00f, 1.00f };
    constexpr vec4 OPEN_GLOW     = { 0.55f, 0.78f, 1.00f, 0.90f }; // ring around an open menu's button
    constexpr vec4 BUTTON_HOVER  = { 1.18f, 1.18f, 1.22f, 1.00f };
    constexpr vec4 BUTTON_HELD   = { 0.75f, 0.75f, 0.78f, 1.00f };

    constexpr std::uint8_t FLASH_FRAMES = 8; // a used slot shows pressed this long

    constexpr float HP_TEXT_SIZE = 10.0f;

    // The crafting queue: smaller icons than the slots, in a row over the
    // slab's left end, just clear of the eagles' heads
    constexpr vec2  QUEUE_ICON    = { 26.0f, 26.0f };
    constexpr float QUEUE_GAP     = 3.0f;
    constexpr float QUEUE_RAISE   = 4.0f; // above the bar's top
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

    // Bottom-center, every frame, so it follows window resizes. Only the
    // slab takes the mouse: clicks above it, between the eagles, reach the
    // game.
    const Nezumi::UI::PanelConfig config = {
        .position = { ui.screen_size().x * 0.5f, ui.screen_size().y },
        .pivot    = { 0.5f, 1.0f },
        .size     = BAR_SIZE,
        .min_size = BAR_SIZE,
        .flags    = PanelFlags::NoTitleBar | PanelFlags::NoBackground | PanelFlags::PinPosition,
        .hit_box  = Rect::from_size({ PLATE_X, PLATE_Y }, PLATE_SIZE),
    };
    if (!ui.begin_panel("##action-bar", config)) return;

    const vec2 origin = ui.content_rect().min;
    rect_ = Rect::from_size(origin, BAR_SIZE);
    const auto at = [&](vec2 pos, vec2 size) { return Rect::from_size(origin + pos, size); };

    // Back to front: the slab, what sits on it, then the eagles over its ends
    ui.draw_nine_slice(at({ PLATE_X, PLATE_Y }, PLATE_SIZE), sheet_->plate);
    draw_hp_bar_(ui, at({ CONTENT_X, HP_Y }, { CONTENT_W, HP_H }), hero);
    ui.draw_image(at({ CONTENT_X + SLOTS_W, SLOTS_Y }, { DIVIDER_W, SLOT_SIZE.y }), sheet_->divider);
    draw_slots_(ui, input, inventory);
    draw_buttons_(ui, input, menus);

    ui.draw_image(at({ 0.0f, 0.0f }, EAGLE_SIZE), sheet_->eagle);
    ui.draw_image(at({ BAR_SIZE.x - EAGLE_SIZE.x, 0.0f }, EAGLE_SIZE), sheet_->eagle.flipped_x());

    // The hp text last, so the bar's text is one run with nothing after it
    char hp[32];
    const unsigned cur = hero ? hero->health : 0u;
    if (hero) last_max_hp_ = hero->max_health;
    std::snprintf(hp, sizeof hp, "%u / %u", cur, static_cast<unsigned>(last_max_hp_));
    const Rect hp_rect = at({ CONTENT_X, HP_Y }, { CONTENT_W, HP_H });
    const float w = ui.text_width(hp, HP_TEXT_SIZE);
    const vec2  p = { std::round(hp_rect.center().x - w * 0.5f),
                      std::round(hp_rect.center().y - ui.text_height(HP_TEXT_SIZE) * 0.5f) };
    ui.draw_text(p + vec2(1.0f), hp, SHADOW, HP_TEXT_SIZE);
    ui.draw_text(p, hp, HP_TEXT, HP_TEXT_SIZE);

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
        .position = { rect_.min.x + CONTENT_X, rect_.min.y - QUEUE_RAISE },
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
    //  Being made, 1 s each
    //  Click: cancel 1   Right click: cancel 5"
    //
    // Nothing in it changes while it's hovered (the UI restarts a tooltip's
    // delay when its text changes), so no live countdown: the shade shows that
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
        if (i == 0 && queue_->blocked()) queue_tip_ += "\nDone, but there's no room in your inventory";
        else {
            std::snprintf(line, sizeof line, "\n%s%g s each", i == 0 ? "Being made, " : "", each);
            queue_tip_ += line;
        }
        queue_tip_ += "\nClick: cancel 1   Right click: cancel 5";
        ui.tooltip(queue_tip_);
    }

    ui.end_panel();
}

void ActionBar::draw_hp_bar_(Nezumi::UI::Context& ui, const Rect& r, const Character* hero) {
    ui.draw_rect(r, HP_TRACK, 2.0f, HP_BORDER, 1.0f);

    const float frac = hero && hero->max_health > 0
        ? std::clamp(static_cast<float>(hero->health) / static_cast<float>(hero->max_health), 0.0f, 1.0f)
        : 0.0f;

    const Rect inner = r.shrink({ 1.0f, 1.0f });
    if (frac > 0.0f) {
        const Rect fill = { inner.min, { inner.min.x + std::round(inner.width() * frac), inner.max.y } };
        ui.draw_image(fill, sheet_->fill, HP_FILL, 1.0f);
    }

    // Tenths, as stone ticks over the fill (2 points: a box any thinner is
    // mostly antialiased away)
    for (int i = 1; i < HP_SEGMENTS; i++) {
        const float x = std::round(r.min.x + r.width() * static_cast<float>(i) / HP_SEGMENTS);
        ui.draw_rect({ { x - 1.0f, r.min.y }, { x + 1.0f, r.max.y } }, HP_TICK);
    }
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

    ui.set_next_pos({ CONTENT_X, SLOTS_Y });
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
            origin + vec2(BUTTONS_X + static_cast<float>(i) * (BUTTON.x + BUTTON_GAP), BUTTONS_Y),
            BUTTON);

        const auto it   = ui.interact(ui.get_id(menu_title(menu)), r);
        const bool open = menus.is_open(menu);
        if (it.clicked) menus.toggle(menu);

        const vec4 tint = it.held ? BUTTON_HELD : it.hovered ? BUTTON_HOVER : vec4(1.0f);
        ui.draw_nine_slice(r, sheet_->button, tint);
        if (open) ui.draw_rect(r.shrink({ 1.0f, 1.0f }), {}, 3.0f, OPEN_GLOW, 1.5f);

        const float push = it.held ? 1.0f : 0.0f;
        const Rect  icon = Rect::from_size(
            r.min + glm::round((BUTTON - vec2(ICON_SIZE)) * 0.5f) + vec2(0.0f, push), vec2(ICON_SIZE));
        ui.draw_image(icon, sheet_->menu_icon(menu), (it.hovered || open) ? ICON_HOVER : ICON);

        // "Inventory (B)"
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
