// =============================================================================
// Draw/CharacterDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/CharacterDrawer.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include <cassert>   // assert

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

CharacterDrawer::CharacterDrawer(const Config& config)
    : sheet_(config.sheet)
{
    assert(sheet_ && "CharacterDrawer: config.sheet is null");
    figure_ = FigureAnimations::find(*sheet_, config.unit);
    color_  = config.color;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void CharacterDrawer::draw(DrawContext& ctx, const Character& character) const {
    const glm::vec2 pos = character.position.to_vec2();

    // A swing plays the attack once from its hit (frame 0); otherwise walk
    // while moving and idle while not, on the clock
    const float now = static_cast<float>(ctx.tick) * TICK_SECONDS;
    Nezumi::Render::AnimationID anim = character.velocity.is_zero() ? figure_.idle : figure_.walk;
    float seconds = figure_seconds(*sheet_, anim, now, character.handle.index());
    if (character.has_swung() && ctx.tick >= character.last_swing_tick()) {
        const Nezumi::Render::Animation& a = sheet_->anim(figure_.attack);
        const float age = static_cast<float>(ctx.tick - character.last_swing_tick()) * TICK_SECONDS;
        if (a.fps > 0.0f && age < static_cast<float>(a.frames) / a.fps) {
            anim    = figure_.attack;
            seconds = age;
        }
    }

    draw_figure   (ctx, *sheet_, anim, pos, character.facing(), seconds, color_);
    draw_healthbar(ctx, character);

    if (!ctx.debug.any()) return;

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, character);
    if (ctx.show(DebugFlag::ShowFacingArrows))        draw_facing_arrow (ctx, pos, character.facing());
}

} // namespace Nightmourn
