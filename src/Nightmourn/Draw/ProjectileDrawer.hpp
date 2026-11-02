// =============================================================================
// Draw/ProjectileDrawer.hpp
//
// Draws a surface's projectiles in flight (Surface::projectiles) as line
// arrows, Age of Empires style: the arrow rides its parabola, tilted along
// it (nose up on release, level at the peak, nose down on landing), over a
// shadow on the ground that stays under it. The shadow is what reads as
// height, in iso as much as top-down.
//
// Height is drawn as a lift straight up the screen of
// height * ctx.upright_scale(), the same scale figures stand at, so an arrow
// that peaks 1 tile up flies over a 0.75-tile figure.
//
//   ShowUnitBehaviorInfo     the ground line from launch to landing point
//                            (white: Ground, cyan: Tracking)
//
// TODO: pick a sprite (arrow, bolt, rock) by ProjectileConfig::look
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"

namespace Nightmourn {

class  Surface;
struct Projectile;

class ProjectileDrawer {
public:

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    // Every projectile of `surface` near the view
    void draw(DrawContext& ctx, const Surface& surface) const;

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    void draw_(DrawContext& ctx, const Projectile& projectile) const;
    void draw_aim_(DrawContext& ctx, const Projectile& projectile) const;
};

} // namespace Nightmourn
