// =============================================================================
// Draw/CharacterDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/CharacterDrawer.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include <algorithm> // min
#include <cassert>   // assert
#include <cmath>     // atan2, cos, sin

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Swing
//
// A pale arc sweeping over the figure toward what it hits, drawn for a few
// ticks after each swing: the hit lands at once, so the arc follows it.
// Upright, like the figure: tiles of upright size around its chest.
// -----------------------------------------------------------------------------

namespace {
    constexpr Nezumi::Tick SWING_TICKS    = 10;
    constexpr float        SWING_RADIUS   = 0.55f;
    constexpr float        SWING_CHEST    = 0.35f;  // above the feet
    constexpr float        SWING_HALF_ARC = 1.20f;  // radians either side of facing
    constexpr int          SWING_SEGMENTS = 12;
    constexpr float        SWING_PIXELS   = 4.0f;
    constexpr glm::vec4    SWING_COLOR    = { 1.0f, 1.0f, 0.92f, 1.0f };

    void draw_swing(DrawContext& ctx, const Character& character) {
        if (!character.has_swung() || ctx.tick < character.last_swing_tick()) return;
        const Nezumi::Tick age = ctx.tick - character.last_swing_tick();
        if (age >= SWING_TICKS) return;

        // 0 .. 1 over the swing: the arc's head sweeps from behind-above to
        // in front, its tail following, and fades as it goes
        const float t     = static_cast<float>(age + 1) / static_cast<float>(SWING_TICKS);
        const float head  = std::min(1.0f, t * 1.6f);
        const float tail  = std::max(0.0f, t * 1.6f - 0.6f);
        if (head <= tail) return;

        const float     scale  = ctx.upright_scale();
        const glm::vec2 dir    = ctx.proj.dir_to_view(character.facing().to_vec2());
        const float     facing = std::atan2(dir.y, dir.x);
        const glm::vec2 center = ctx.to_view(character.position.to_vec2()) + glm::vec2(0.0f, SWING_CHEST * scale);

        // Over the top: start on the side the figure faces away from, upward
        const float side  = dir.x < 0.0f ? -1.0f : 1.0f;
        const float start = facing + side * SWING_HALF_ARC;
        const float sweep = -side * 2.0f * SWING_HALF_ARC;

        glm::vec4 color = SWING_COLOR;
        color.a *= 1.0f - t * 0.7f;

        const Nezumi::Render::DrawParams params = world_layer(DrawLayer::EntityOverlay);
        const float width = SWING_PIXELS * ctx.pixel_size;
        const auto at = [&](float u) {
            const float a = start + sweep * u;
            return center + SWING_RADIUS * scale * glm::vec2(std::cos(a), std::sin(a));
        };
        glm::vec2 prev = at(tail);
        for (int i = 1; i <= SWING_SEGMENTS; i++) {
            const glm::vec2 next = at(tail + (head - tail) * static_cast<float>(i) / SWING_SEGMENTS);
            ctx.renderer.draw_line(
                Nezumi::Render::Line{ .position_a = prev, .position_b = next, .color = color, .width = width },
                params);
            prev = next;
        }
    }
}

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

CharacterDrawer::CharacterDrawer(const Config& config)
    : atlas_(config.atlas)
{
    assert(atlas_ && "CharacterDrawer: config.atlas is null");
    frames_ = FigureFrames::from_row(*atlas_, config.row, config.frame_size);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void CharacterDrawer::draw(DrawContext& ctx, const Character& character) const {
    const glm::vec2 pos = character.position.to_vec2();

    draw_figure   (ctx, *atlas_, frames_, pos, character.facing());
    draw_healthbar(ctx, character);
    draw_swing    (ctx, character);

    if (!ctx.debug.any()) return;

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, character);
    if (ctx.show(DebugFlag::ShowFacingArrows))        draw_facing_arrow (ctx, pos, character.facing());
}

} // namespace Nightmourn
