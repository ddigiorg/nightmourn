// =============================================================================
// Draw/CharacterDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/CharacterDrawer.hpp"
#include "Nightmourn/Character/Character.hpp"
#include <cassert> // assert

namespace Nightmourn {

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

    if (!ctx.debug.any()) return;

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, character);
    if (ctx.show(DebugFlag::ShowFacingArrows))        draw_facing_arrow (ctx, pos, character.facing());
}

} // namespace Nightmourn
