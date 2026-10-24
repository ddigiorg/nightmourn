// =============================================================================
// Draw/DrawContext.cpp
// =============================================================================

#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nezumi/Render/View/Camera.hpp"
#include "Nezumi/World/World.hpp"

namespace Nightmourn {

DrawContext DrawContext::make(
    Nezumi::Render::Renderer&     renderer,
    const Nezumi::World&          world,
    const Nezumi::Render::Camera& camera,
    const DebugFlags&             debug)
{
    const glm::vec4 bounds = camera.world_bounds();

    return DrawContext{
        .renderer   = renderer,
        .world      = world,
        .debug      = debug,
        .view       = Nezumi::Area2D(bounds.x, bounds.y, bounds.z, bounds.w),
        .pixel_size = 1.0f / camera.get_scale(),
        .tick       = world.get_tick(),
    };
}

} // namespace Nightmourn
