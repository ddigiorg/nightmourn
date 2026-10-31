// =============================================================================
// Draw/DrawContext.cpp
// =============================================================================

#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nezumi/Render/2D/Camera/Camera2D.hpp"
#include "Nezumi/World/World.hpp"

namespace Nightmourn {

DrawContext DrawContext::make(
    Nezumi::Render::Renderer2D&       renderer,
    const Nezumi::World&              world,
    const Nezumi::Render::Camera2D&   camera,
    const Nezumi::Projection&         proj,
    const DebugFlags&                 debug)
{
    // The camera only knows view space, so its "world" bounds are view bounds
    const float     overhang = VIEW_OVERHANG * proj.tile_size().x;
    const glm::vec4 view     = camera.world_bounds()
                             + glm::vec4{ -overhang, -overhang, overhang, overhang };
    const glm::vec4 world_view = proj.box_to_world_bounds(view);

    return DrawContext{
        .renderer   = renderer,
        .world      = world,
        .debug      = debug,
        .proj       = proj,
        .view       = Nezumi::Area2D(view.x, view.y, view.z, view.w),
        .world_view = Nezumi::Area2D(world_view.x, world_view.y, world_view.z, world_view.w),
        .pixel_size = 1.0f / camera.get_scale(),
        .tick       = world.get_tick(),
    };
}

} // namespace Nightmourn
