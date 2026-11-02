// =============================================================================
// Draw/UnitDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/UnitDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include <algorithm> // clamp
#include <cassert>   // assert

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
    : sheet_(config.sheet)
{
    assert(sheet_ && "UnitDrawer: config.sheet is null");
    assert(config.looks[0][0].unit && "UnitDrawer: faction 0 needs a look");
    for (std::size_t f = 0; f < MAX_FACTIONS; f++) {
        for (std::size_t s = 0; s < MAX_SUBTYPES; s++) {
            const Config::Look* look = &config.looks[f][s];
            if (!look->unit) look = &config.looks[f][0];
            if (!look->unit) look = &config.looks[0][0];
            looks_[f][s] = { FigureAnimations::find(*sheet_, look->unit), look->color };
        }
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void UnitDrawer::draw(DrawContext& ctx, const Unit& unit) const {
    const glm::vec2 pos = unit.position.to_vec2();

    // Units have one faction (faction_index asserts it); neutral ones use
    // faction 0's figure
    const FactionIndex faction = (unit.factions_mask == EntityFactionsMask::None)
        ? 0 : unit.faction_index();

    const EntitySubtypeID subtype = unit.subtype_id;
    const std::size_t     index   = (subtype.is_valid() && subtype.value() < MAX_SUBTYPES) ? subtype.value() : 0;
    const Resolved&         look   = looks_[faction][index];
    const FigureAnimations& figure = look.anims;

    // Idle and walk loop on the clock, each unit at its own phase; the
    // attack runs once per attack_interval, starting at the hit
    const float   now  = static_cast<float>(ctx.tick) * TICK_SECONDS;
    const std::uint32_t seed = unit.handle.index();
    Nezumi::Render::AnimationID anim = figure.idle;
    float seconds = 0.0f;
    switch (unit.state()) {
        case UnitState::Idle:
            anim    = figure.idle;
            seconds = figure_seconds(*sheet_, anim, now, seed);
            break;
        case UnitState::Moving:
            anim    = figure.walk;
            seconds = figure_seconds(*sheet_, anim, now, seed);
            break;
        case UnitState::Attacking: {
            anim = figure.attack;
            const Nezumi::Render::Animation& a = sheet_->anim(anim);
            const float interval = static_cast<float>(unit.config().attack_interval);
            if (interval > 0.0f && a.fps > 0.0f) {
                const float since = interval - static_cast<float>(unit.attack_cooldown());
                seconds = std::clamp(since / interval, 0.0f, 0.999f) * static_cast<float>(a.frames) / a.fps;
            }
            break;
        }
    }

    draw_figure   (ctx, *sheet_, anim, pos, unit.facing(), seconds, look.color);
    draw_healthbar(ctx, unit);

    if (!ctx.debug.any()) return;

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, unit);
    if (ctx.show(DebugFlag::ShowFacingArrows))        draw_facing_arrow (ctx, pos, unit.facing());
    if (ctx.show(DebugFlag::ShowUnitBehaviorInfo))    draw_behavior_    (ctx, unit);
}

void UnitDrawer::draw_behavior_(DrawContext& ctx, const Unit& unit) const {
    const glm::vec2 pos   = ctx.to_view(unit.position.to_vec2());
    const glm::vec4 color = STATE_COLORS[static_cast<std::size_t>(unit.state())];
    const Nezumi::Render::DrawParams params = ctx.layer(DrawLayer::Debug);

    if (const Entity* target = ctx.world.get_entity(unit.target()); target && target->is_alive()) {
        ctx.renderer.draw_line(
            Nezumi::Render::Line{
                .position_a = pos,
                .position_b = ctx.to_view(target->position.to_vec2()),
                .color      = color,
                .width      = TARGET_LINE_WIDTH * ctx.pixel_size,
            },
            params
        );
    }

    ctx.renderer.draw_circle(
        Nezumi::Render::Circle{ .position = pos, .color = color, .radius = STATE_DOT_RADIUS * ctx.upright_scale() },
        params
    );
}

} // namespace Nightmourn
