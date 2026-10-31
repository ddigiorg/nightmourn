// =============================================================================
// Input/Picking.cpp
// =============================================================================

#include "Nightmourn/Input/Picking.hpp"
#include "Nezumi/Render/2D/Camera/Camera2D.hpp"
#include "Nezumi/World/Surface/Surface.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"

namespace Nightmourn {

glm::vec2 screen_to_world(const Nezumi::Render::Camera2D&   camera,
                          const Nezumi::Projection&         proj,
                          glm::vec2                         screen)
{
    // The camera only knows view space
    return proj.to_world(camera.screen_to_world(screen));
}

Nezumi::TileCoord tile_at(const Nezumi::Render::Camera2D&   camera,
                          const Nezumi::Projection&         proj,
                          glm::vec2                         screen)
{
    const glm::vec2 world = screen_to_world(camera, proj, screen);
    return Nezumi::TileCoord::from_position({ world.x, world.y });
}

Nezumi::EntityHandle pick_entity(const Nezumi::Surface&            surface,
                                 const Nezumi::Render::Camera2D&   camera,
                                 const Nezumi::Projection&         proj,
                                 glm::vec2                         screen,
                                 bool (*accept)(const Nezumi::Entity&))
{
    const glm::vec2 cursor = camera.screen_to_world(screen); // view space

    // A sprite box is feet + bounds, so it covers the cursor only if the feet
    // are within cursor - bounds: up to the tallest sprite's height below the
    // cursor on screen. Its world box (a diamond's bounds in iso) is what to
    // query.
    const glm::vec4 reach = TREE_SPRITE_BOUNDS * proj.tile_size().x;
    const glm::vec4 feet  = { cursor.x - reach.z, cursor.y - reach.w,
                              cursor.x - reach.x, cursor.y - reach.y };
    const glm::vec4 area  = proj.box_to_world_bounds(feet);

    Nezumi::EntityHandle best   = {};
    float                best_y = 0.0f;

    surface.visit_entities_in(Nezumi::Area2D(area.x, area.y, area.z, area.w), [&](const Nezumi::Entity& entity) {
        if (!entity.is_drawable()) return true;
        if (accept && !accept(entity)) return true;

        const glm::vec4* bounds = sprite_bounds(entity);
        if (!bounds) return true;

        const glm::vec2 pos = entity.position.to_vec2();
        const glm::vec4 box = sprite_view_box(proj, pos, *bounds);
        if (cursor.x < box.x || cursor.x > box.z || cursor.y < box.y || cursor.y > box.w) return true;

        // Front-most: the lowest feet on screen, which draw last (y_sort_order)
        const float y = proj.to_view(pos).y;
        if (!best || y < best_y) { best = entity.handle; best_y = y; }
        return true;
    });

    return best;
}

} // namespace Nightmourn
