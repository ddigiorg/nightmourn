// =============================================================================
// Draw/SettlementDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/SettlementDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nightmourn/Settlement/Settlement.hpp"
#include "Nightmourn/Surface/Surface.hpp"
#include "Nightmourn/World/World.hpp"
#include <array>     // array
#include <cmath>     // floor
#include <algorithm> // min

namespace Nightmourn {

namespace NR = Nezumi::Render;

static constexpr glm::vec4 SPAWN_AREA_COLOR = { 1.0f, 0.35f, 0.30f, 0.9f };
static constexpr float     DASH_TILES       = 1.0f; // dash and gap, tiles

// A dashed outline of `box`, on the ground
static void draw_dashed_outline(DrawContext& ctx, const Area2D& box, glm::vec4 color, float pixels) {
    const float width = pixels * ctx.pixel_size;
    const NR::DrawParams params = ctx.layer(DrawLayer::GroundOverlay);
    const std::array<glm::vec2, 4> c = {{
        box.min.to_vec2(), { box.max.x, box.min.y }, box.max.to_vec2(), { box.min.x, box.max.y },
    }};
    for (std::size_t i = 0; i < c.size(); i++) {
        const glm::vec2 a   = c[i];
        const glm::vec2 b   = c[(i + 1) % c.size()];
        const float     len = glm::length(b - a);
        const glm::vec2 dir = (b - a) / len;
        for (float t = 0.0f; t < len; t += 2.0f * DASH_TILES) {
            const glm::vec2 p = a + dir * t;
            const glm::vec2 q = a + dir * std::min(t + DASH_TILES, len);
            ctx.renderer.draw_line(
                NR::Line{ .position_a = ctx.to_view(p), .position_b = ctx.to_view(q), .color = color, .width = width },
                params);
        }
    }
}

void draw_build_area(DrawContext& ctx, const TileRect& area, const SettlementDrawStyle& style) {
    const Area2D box = area.bounds();
    if (style.fill && ctx.proj.is_top_down()) {
        ctx.renderer.draw_rectangle(
            NR::Rectangle{
                .pos_box = ctx.proj.box_to_view_bounds(box.to_vec4()),
                .color   = { style.color.r, style.color.g, style.color.b, style.fill_alpha },
            },
            ctx.layer(DrawLayer::GroundOverlay));
    }
    draw_outline(ctx, box, style.color, style.outline_pixels, DrawLayer::GroundOverlay);
}

void draw_settlements(DrawContext& ctx, const Surface& surface, const SettlementDrawStyle& style) {
    ctx.world.each_settlement([&](const Settlement& s) {
        if (s.surface() != surface.id()) return;
        draw_build_area(ctx, s.build_area(), style);
        if (style.show_spawn_area || ctx.show(DebugFlag::ShowSettlementAreas))
            draw_dashed_outline(ctx, s.spawn_area().bounds(), SPAWN_AREA_COLOR, style.outline_pixels);
    });
}

} // namespace Nightmourn
