// =============================================================================
// Draw/DrawContext.hpp
//
// Everything a drawer needs for one frame: where to submit draws, what the
// world looks like, how it is projected, what the camera sees, and which
// debug overlays are on. Built once per frame by the game and passed down to
// every drawer.
//
// Drawers submit in VIEW space: the world camera looks at projected
// positions (Projection, top-down or 2:1 isometric). The
// sim stays on the square tile grid, so a drawer projects what it reads:
//
//   ground things (tiles, grid, footprints, routes) project every point:
//       ctx.to_view(a), ctx.to_view(b)
//   upright things (figures, healthbars) project their anchor and keep a
//   screen-aligned box around it, scaled by ctx.upright_scale():
//       ctx.to_view(feet) + FIGURE_SPRITE_BOUNDS * ctx.upright_scale()
//
// Drawers only read the world (const) and only write draws. That is the rule
// that lets drawing move to its own thread later, Factorio style: the draw
// stage reads a finished tick while the next one runs, and the visible chunks
// can be split across workers, each with its own command queue in place of
// `renderer`.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Projection.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"

namespace Nezumi::Render {
    class Camera2D;
    class Renderer2D;
}

namespace Nightmourn {

class World;

struct DrawContext {
    Nezumi::Render::Renderer2D& renderer;
    const World&        world;
    const DebugFlags&           debug;

    // World to view space
    Projection proj = {};

    // View-space rectangle to draw: what the camera sees, grown by
    // VIEW_OVERHANG so figures standing just off screen still poke in
    Area2D view = {};

    // World-space bounds of `view` (tiles). Iso sees a diamond of the world,
    // so this box holds about twice the area on screen: use it to pick
    // chunks, then test against `view`.
    Area2D world_view = {};

    // Size of one screen pixel in view units, for lines that should stay N
    // pixels wide at any zoom: Line{ .width = 2.0f * ctx.pixel_size }
    float pixel_size = 1.0f;

    // The tick about to run (World::get_tick), e.g. for UnitGroup::plan
    Nezumi::Tick tick = 0;

    // How far (tiles of upright size) the view is grown on every side: the
    // tallest sprite above its feet. A figure is 0.75 tiles tall, a tree 3.
    static constexpr float VIEW_OVERHANG = 3.0f;

    // The projection the game draws with: 2:1 isometric, or top-down while
    // use-top-down-view is on. Input and picking use the same one, so call
    // this wherever the game needs it rather than caching it across frames.
    static Projection projection(const DebugFlags& debug) {
        return debug.is_on(DebugFlag::UseTopDownView) ? Projection::top_down()
                                                      : Projection::iso_2to1();
    }

    // Context for drawing `world` through `camera` (the World view's camera)
    // and `proj`
    static DrawContext make(
        Nezumi::Render::Renderer2D&       renderer,
        const World&              world,
        const Nezumi::Render::Camera2D&   camera,
        const Projection&         proj,
        const DebugFlags&                 debug);

    bool show(DebugFlag flag) const { return debug.is_on(flag); }

    glm::vec2 to_view(glm::vec2 world_pos) const { return proj.to_view(world_pos); }

    // View units per tile of upright size (see the header comment)
    float upright_scale() const { return proj.tile_size().x; }
};

} // namespace Nightmourn
