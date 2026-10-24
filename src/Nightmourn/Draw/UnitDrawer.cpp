// =============================================================================
// Draw/UnitDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/UnitDrawer.hpp"
#include "Nezumi/Render/Renderer.hpp"
#include "Nezumi/World/Entities/Unit.hpp"
#include "Nezumi/World/World.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include <cassert> // assert

namespace Nightmourn {

// Behavior colors, by UnitState
static constexpr std::array<glm::vec4, 3> STATE_COLORS = {{
    { 0.7f, 0.7f, 0.7f, 1.0f }, // Idle
    { 1.0f, 0.9f, 0.2f, 1.0f }, // Moving
    { 1.0f, 0.2f, 0.1f, 1.0f }, // Attacking
}};

static constexpr float STATE_DOT_RADIUS  = 0.08f; // tiles
static constexpr float TARGET_LINE_WIDTH = 1.0f;  // pixels

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

UnitDrawer::UnitDrawer(const Config& config)
    : atlas_(config.atlas)
{
    assert(atlas_ && "UnitDrawer: config.atlas is null");
    for (std::size_t f = 0; f < Nezumi::MAX_FACTIONS; f++)
        frames_[f] = FigureFrames::from_row(*atlas_, config.faction_rows[f], config.frame_size);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void UnitDrawer::draw(DrawContext& ctx, const Nezumi::Unit& unit) const {
    const glm::vec2 pos = unit.position.to_vec2();

    // Units have one faction (faction_index asserts it); neutral ones use row 0
    const Nezumi::FactionIndex faction = (unit.factions_mask == Nezumi::EntityFactionsMask::None)
        ? 0 : unit.faction_index();

    draw_figure   (ctx, *atlas_, frames_[faction], pos, unit.facing());
    draw_healthbar(ctx, unit);

    if (!ctx.debug.any()) return;

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, unit);
    if (ctx.show(DebugFlag::ShowFacingArrows))        draw_facing_arrow (ctx, pos, unit.facing());
    if (ctx.show(DebugFlag::ShowUnitBehaviorInfo))    draw_behavior_    (ctx, unit);
}

void UnitDrawer::draw_behavior_(DrawContext& ctx, const Nezumi::Unit& unit) const {
    const glm::vec2 pos   = unit.position.to_vec2();
    const glm::vec4 color = STATE_COLORS[static_cast<std::size_t>(unit.state())];
    const Nezumi::Render::DrawParams params = world_layer(DrawLayer::Debug);

    if (const Nezumi::Entity* target = ctx.world.get_entity(unit.target()); target && target->is_alive()) {
        ctx.renderer.draw_line(
            Nezumi::Render::Line{
                .position_a = pos,
                .position_b = target->position.to_vec2(),
                .color      = color,
                .width      = TARGET_LINE_WIDTH * ctx.pixel_size,
            },
            params
        );
    }

    ctx.renderer.draw_circle(
        Nezumi::Render::Circle{ .position = pos, .color = color, .radius = STATE_DOT_RADIUS },
        params
    );
}

} // namespace Nightmourn
