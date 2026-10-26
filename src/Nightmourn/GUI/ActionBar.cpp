// =============================================================================
// GUI/ActionBar.cpp
// =============================================================================

#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nezumi/Input/KeyName.hpp"
#include "Nezumi/UI/Context.hpp"
#include <algorithm> // clamp, max, swap
#include <cctype>    // toupper
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
    constexpr vec2  PLATE_SIZE    = { 575.0f, 50.0f };
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
    constexpr vec2  BUTTON        = { 22.0f, 32.0f };
    constexpr float BUTTON_GAP    = 3.0f;
    constexpr float BUTTONS_X     = CONTENT_X + SLOTS_W + DIVIDER_W;
    constexpr float BUTTONS_Y     = SLOTS_Y + (SLOT_SIZE.y - BUTTON.y) * 0.5f;
    constexpr float ICON_SIZE     = 18.0f;

    static_assert(SLOTS_W + DIVIDER_W + MENU_COUNT * BUTTON.x + (MENU_COUNT - 1) * BUTTON_GAP == CONTENT_W,
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

void ActionBar::use_(std::size_t i) {
    flash_[i] = FLASH_FRAMES;
    if (slots_[i]) used_.push_back(i);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ActionBar::draw(Nezumi::UI::Context& ui, const Nezumi::InputMap& input,
                     GameMenus& menus, const Character* hero)
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
    ui.draw_nine_slice(at({ PLATE_X, PLATE_Y }, PLATE_SIZE), atlas_->plate);
    draw_hp_bar_(ui, at({ CONTENT_X, HP_Y }, { CONTENT_W, HP_H }), hero);
    ui.draw_image(at({ CONTENT_X + SLOTS_W, SLOTS_Y }, { DIVIDER_W, SLOT_SIZE.y }), atlas_->divider);
    draw_slots_(ui, input);
    draw_buttons_(ui, input, menus);

    ui.draw_image(at({ 0.0f, 0.0f }, EAGLE_SIZE), atlas_->eagle);
    ui.draw_image(at({ BAR_SIZE.x - EAGLE_SIZE.x, 0.0f }, EAGLE_SIZE), atlas_->eagle.flipped_x());

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

    for (std::uint8_t& f : flash_) if (f > 0) f--;
}

void ActionBar::draw_hp_bar_(Nezumi::UI::Context& ui, const Rect& r, const Character* hero) {
    ui.draw_rect(r, HP_TRACK, 2.0f, HP_BORDER, 1.0f);

    const float frac = hero && hero->max_health > 0
        ? std::clamp(static_cast<float>(hero->health) / static_cast<float>(hero->max_health), 0.0f, 1.0f)
        : 0.0f;

    const Rect inner = r.shrink({ 1.0f, 1.0f });
    if (frac > 0.0f) {
        const Rect fill = { inner.min, { inner.min.x + std::round(inner.width() * frac), inner.max.y } };
        ui.draw_image(fill, atlas_->fill, HP_FILL, 1.0f);
    }

    // Tenths, as stone ticks over the fill (2 points: a box any thinner is
    // mostly antialiased away)
    for (int i = 1; i < HP_SEGMENTS; i++) {
        const float x = std::round(r.min.x + r.width() * static_cast<float>(i) / HP_SEGMENTS);
        ui.draw_rect({ { x - 1.0f, r.min.y }, { x + 1.0f, r.max.y } }, HP_TICK);
    }
}

void ActionBar::draw_slots_(Nezumi::UI::Context& ui, const Nezumi::InputMap& input) {
    std::array<Nezumi::UI::Slot, SLOT_COUNT> slots;
    for (std::size_t i = 0; i < SLOT_COUNT; i++) {
        Nezumi::UI::Slot& s = slots[i];
        if (slots_[i]) {
            s.icon  = atlas_->item_icon(*slots_[i]);
            s.value = slots_[i]->value();
        }
        if (const auto key = input.first_binding(Action::use_slot(i)))
            s.label = Nezumi::input_name(*key);
        s.flash = flash_[i] > 0 || input.is_held(Action::use_slot(i));
    }

    const Nezumi::UI::SlotStyle style = {
        .slot_size   = SLOT_SIZE,
        .spacing     = { SLOT_GAP, SLOT_GAP },
        .frame       = atlas_->slot,
        .frame_over  = atlas_->rim,
        .icon_inset  = 2.0f,
        .icon_radius = 2.0f,
        .label_size  = 11.0f,
        .label_color = { 0.86f, 0.90f, 0.96f, 1.0f },
        .hover       = { 0.75f, 0.88f, 1.00f, 0.14f },
        .drop_hover  = { 0.75f, 0.88f, 1.00f, 0.28f },
        .highlight   = { 0.55f, 0.78f, 1.00f, 0.70f },
    };

    ui.set_next_pos({ CONTENT_X, SLOTS_Y });
    const Nezumi::UI::Context::ID grid = ui.get_id("slots");
    const Nezumi::UI::SlotGridResult r = ui.slot_grid("slots", slots, SLOT_COUNT, style, DRAG_TYPE);

    if (r.clicked >= 0) use_(static_cast<std::size_t>(r.clicked));

    // A drop from another slot swaps the two; from elsewhere (an inventory,
    // later) it puts that item here
    if (r.dropped >= 0) {
        const std::size_t to = static_cast<std::size_t>(r.dropped);
        if (r.payload.source == grid && r.payload.index >= 0) {
            std::swap(slots_[to], slots_[static_cast<std::size_t>(r.payload.index)]);
        } else {
            slots_[to] = Nezumi::ItemID{ static_cast<Nezumi::ItemID::value_t>(r.payload.value) };
        }
    }

    // Dragged off the bar and dropped where nothing took it: the slot empties
    if (const auto drop = ui.take_unclaimed_drop(DRAG_TYPE);
        drop && drop->payload.source == grid && drop->payload.index >= 0) {
        slots_[static_cast<std::size_t>(drop->payload.index)].reset();
    }

    if (r.hovered >= 0 && slots_[static_cast<std::size_t>(r.hovered)]) {
        std::string name(item_name(*slots_[static_cast<std::size_t>(r.hovered)]));
        if (!name.empty()) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        ui.tooltip(name);
    }
}

void ActionBar::draw_buttons_(Nezumi::UI::Context& ui, const Nezumi::InputMap& input, GameMenus& menus) {
    const vec2 origin = ui.content_rect().min;

    for (std::size_t i = 0; i < MENU_COUNT; i++) {
        const Menu menu = static_cast<Menu>(i);
        const Rect r = Rect::from_size(
            origin + vec2(BUTTONS_X + static_cast<float>(i) * (BUTTON.x + BUTTON_GAP), BUTTONS_Y),
            BUTTON);

        const auto it   = ui.interact(ui.get_id(menu_title(menu)), r);
        const bool open = menus.is_open(menu);
        if (it.clicked) menus.toggle(menu);

        const vec4 tint = it.held ? BUTTON_HELD : it.hovered ? BUTTON_HOVER : vec4(1.0f);
        ui.draw_nine_slice(r, atlas_->button, tint);
        if (open) ui.draw_rect(r.shrink({ 1.0f, 1.0f }), {}, 3.0f, OPEN_GLOW, 1.5f);

        const float push = it.held ? 1.0f : 0.0f;
        const Rect  icon = Rect::from_size(
            r.min + glm::round((BUTTON - vec2(ICON_SIZE)) * 0.5f) + vec2(0.0f, push), vec2(ICON_SIZE));
        ui.draw_image(icon, atlas_->menu_icon(menu), (it.hovered || open) ? ICON_HOVER : ICON);

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
