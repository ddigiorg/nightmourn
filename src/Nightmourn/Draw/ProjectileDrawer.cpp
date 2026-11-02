// =============================================================================
// Draw/ProjectileDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/ProjectileDrawer.hpp"
#include "Nightmourn/Projectile/Projectile.hpp"
#include "Nightmourn/Surface/Surface.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include <algorithm> // max
#include <cmath>     // sqrt

namespace Nightmourn {

// Upright tiles (times ctx.upright_scale() in view units)
static constexpr float ARROW_LENGTH = 0.45f;
static constexpr float ARROW_WIDTH  = 0.025f;
static constexpr float HEAD_LENGTH  = 0.1f;   // the pale tip, part of ARROW_LENGTH

// Ground tiles
static constexpr float SHADOW_LENGTH = 0.4f;

static constexpr glm::vec4 SHAFT_COLOR  = { 0.32f, 0.22f, 0.13f, 1.0f };
static constexpr glm::vec4 HEAD_COLOR   = { 0.78f, 0.78f, 0.82f, 1.0f };
static constexpr glm::vec4 SHADOW_COLOR = { 0.0f,  0.0f,  0.0f,  0.35f };

static constexpr glm::vec4 GROUND_AIM_COLOR   = { 1.0f, 1.0f, 1.0f, 0.6f };
static constexpr glm::vec4 TRACKING_AIM_COLOR = { 0.2f, 0.9f, 1.0f, 0.6f };

static glm::vec2 normalize_or(glm::vec2 v, glm::vec2 fallback) {
    const float len2 = v.x * v.x + v.y * v.y;
    return len2 > 0.0f ? v / std::sqrt(len2) : fallback;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ProjectileDrawer::draw(DrawContext& ctx, const Surface& surface) const {
    const bool aims = ctx.show(DebugFlag::ShowUnitBehaviorInfo);

    for (const Projectile& p : surface.projectiles()) {
        // Cull by the flight's world box: the arrow never leaves it on the
        // ground, and the view's overhang covers its height
        const Area2D flight(
            std::min(p.origin.x, p.destination.x), std::min(p.origin.y, p.destination.y),
            std::max(p.origin.x, p.destination.x), std::max(p.origin.y, p.destination.y));
        if (!flight.overlaps(ctx.world_view)) continue;

        draw_(ctx, p);
        if (aims) draw_aim_(ctx, p);
    }
}

void ProjectileDrawer::draw_(DrawContext& ctx, const Projectile& p) const {
    const float t      = p.progress(ctx.tick);
    const float scale  = ctx.upright_scale();
    const float height = p.height_at(t);

    const glm::vec2 feet = ctx.to_view(p.ground_at(t).to_vec2());
    const glm::vec2 pos  = feet + glm::vec2{ 0.0f, height * scale };

    // Way the flight goes on the ground, in view space (per whole flight)
    const glm::vec2 ground_dir = ctx.proj.dir_to_view((p.destination - p.origin).to_vec2());

    // Shadow: along the ground, fading as the arrow climbs
    {
        const glm::vec2 g    = normalize_or(ground_dir, { 1.0f, 0.0f });
        const glm::vec2 half = g * (0.5f * SHADOW_LENGTH * scale);

        glm::vec4 color = SHADOW_COLOR;
        color.a /= 1.0f + height;

        ctx.renderer.draw_line(
            Nezumi::Render::Line{
                .position_a = feet - half,
                .position_b = feet + half,
                .color      = color,
                .width      = std::max(ARROW_WIDTH * scale, ctx.pixel_size),
            },
            ctx.layer(DrawLayer::GroundOverlay));
    }

    // Arrow: tangent to the parabola on screen
    const glm::vec2 dir  = normalize_or(ground_dir + glm::vec2{ 0.0f, p.climb_at(t) * scale },
                                        { 1.0f, 0.0f });
    const glm::vec2 tail = pos - dir * (0.5f * ARROW_LENGTH * scale);
    const glm::vec2 nose = pos + dir * (0.5f * ARROW_LENGTH * scale);
    const glm::vec2 neck = nose - dir * (HEAD_LENGTH * scale);
    const float     wide = std::max(ARROW_WIDTH * scale, ctx.pixel_size);

    Nezumi::Render::DrawParams params = ctx.layer(DrawLayer::Entities);
    params.order = y_sort_order(feet.y);

    ctx.renderer.draw_line(
        Nezumi::Render::Line{ .position_a = tail, .position_b = neck, .color = SHAFT_COLOR, .width = wide },
        params);
    ctx.renderer.draw_line(
        Nezumi::Render::Line{ .position_a = neck, .position_b = nose, .color = HEAD_COLOR, .width = wide },
        params);
}

void ProjectileDrawer::draw_aim_(DrawContext& ctx, const Projectile& p) const {
    const bool tracking = p.config->guidance == ProjectileGuidance::Tracking;

    ctx.renderer.draw_line(
        Nezumi::Render::Line{
            .position_a = ctx.to_view(p.origin.to_vec2()),
            .position_b = ctx.to_view(p.destination.to_vec2()),
            .color      = tracking ? TRACKING_AIM_COLOR : GROUND_AIM_COLOR,
            .width      = ctx.pixel_size,
        },
        ctx.layer(DrawLayer::Debug));
}

} // namespace Nightmourn
