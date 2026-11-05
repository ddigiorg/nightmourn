// =============================================================================
// GUI/CraftOverlay.cpp
// =============================================================================

#include "Nightmourn/GUI/CraftOverlay.hpp"
#include "Nightmourn/Crafting/CraftSite.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nezumi/Render/2D/Camera/Camera2D.hpp"
#include "Nezumi/UI/Context.hpp"
#include <cstdio> // snprintf

namespace Nightmourn {

namespace {
    using Nezumi::UI::Rect;

    constexpr glm::vec4 BLOCKED = { 0.85f, 0.12f, 0.10f, 0.45f }; // the action bar queue's red
}

void CraftOverlay::draw(Nezumi::UI::Context& ui, const World& world, const CraftSite& site, Nezumi::Tick now,
                        const Nezumi::Render::Camera2D& camera, const Projection& proj, bool show_all)
{
    using Nezumi::UI::PanelFlags;

    // The whole screen, see-through, and its hit box off it: clicks reach
    // the game
    const glm::vec2 screen = ui.screen_size();
    const Nezumi::UI::PanelConfig config = {
        .position = { 0.0f, 0.0f },
        .size     = screen,
        .min_size = screen,
        .flags    = PanelFlags::NoTitleBar | PanelFlags::NoBackground | PanelFlags::PinPosition,
        .hit_box  = Rect::from_size({ -4.0f, -4.0f }, { 1.0f, 1.0f }),
    };
    if (!ui.begin_panel("##craft-overlay", config)) return;

    const Nezumi::UI::SlotStyle style = sheet_->item_slot_style(ICON, 0.0f);
    const float scale = ui.scale();
    char label[32];

    const std::span<const Machine> machines = site.machines();
    for (std::size_t i = 0; i < machines.size(); i++) {
        const Machine& m = machines[i];
        if (!m.building) continue;
        if (m.state == MachineState::Idle && !show_all) continue;
        const Entity* b = world.get_entity(m.building);
        if (!b) continue;

        // Over the middle of its footprint, in UI points; skipped off screen
        const glm::vec2 at = camera.world_to_screen(proj.to_view(b->position.to_vec2())) / scale;
        const glm::vec2 corner = at - ICON * 0.5f;
        if (corner.x > screen.x || corner.y > screen.y || corner.x + ICON.x < 0.0f || corner.y + ICON.y < 0.0f) continue;

        Nezumi::UI::Slot slot;
        if (m.recipe) {
            slot.icon     = sheet_->item_icon(m.recipe->results.front().item);
            slot.cooldown = m.state == MachineState::Working ? 1.0f - m.progress(now) : 0.0f;
        }

        ui.set_next_pos(corner);
        std::snprintf(label, sizeof label, "##job%zu", i);
        ui.slot_grid(label, std::span(&slot, 1), 1, style);
        const Rect r = Rect::from_size(corner, ICON);
        if (m.state == MachineState::OutputFull) ui.draw_rect(r, BLOCKED, 3.0f);
    }
    ui.end_panel();
}

} // namespace Nightmourn
