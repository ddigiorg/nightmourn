// =============================================================================
// Draw/DrawContext.hpp
//
// Everything a drawer needs for one frame: where to submit draws, what the
// world looks like, what the camera sees, and which debug overlays are on.
// Built once per frame by the game and passed down to every drawer.
//
// Drawers only read the world (const) and only write draws. That is the rule
// that lets drawing move to its own thread later, Factorio style: the draw
// stage reads a finished tick while the next one runs, and the visible chunks
// can be split across workers, each with its own command queue in place of
// `renderer`.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"

namespace Nezumi {
    class World;
}

namespace Nezumi::Render {
    class Camera;
    class Renderer;
}

namespace Nightmourn {

struct DrawContext {
    Nezumi::Render::Renderer& renderer;
    const Nezumi::World&      world;
    const DebugFlags&         debug;

    // World area the camera sees (tiles)
    Nezumi::Area2D view = {};

    // Size of one screen pixel in tiles, for lines that should stay N pixels
    // wide at any zoom: Line{ .width = 2.0f * ctx.pixel_size }
    float pixel_size = 1.0f;

    // The tick about to run (World::get_tick), e.g. for UnitGroup::plan
    Nezumi::Tick tick = 0;

    // Context for drawing `world` through `camera` (the World view's camera)
    static DrawContext make(
        Nezumi::Render::Renderer&     renderer,
        const Nezumi::World&          world,
        const Nezumi::Render::Camera& camera,
        const DebugFlags&             debug);

    bool show(DebugFlag flag) const { return debug.is_on(flag); }
};

} // namespace Nightmourn
