// =============================================================================
// Input/Picking.hpp
//
// What is under the mouse. The world camera looks at view space (projected,
// see Nezumi::Render::Projection), so a screen position goes screen -> view
// (Camera::screen_to_world) -> world (Projection::to_world):
//
//   const Nezumi::TileCoord tile = tile_at(camera, proj, mouse);
//
// Tiles are exact: the inverse projection lands on the one tile under the
// cursor, with no "which half of the diamond" lookup.
//
// Sprites are upright, so the ground point under the cursor is behind an
// entity's feet, not on them. pick_entity() looks for feet in the strip of
// ground the tallest sprite can reach up from, tests each one's drawn sprite
// box (sprite_bounds, the one the drawers use), and returns the front-most
// hit: the one drawn on top.
// =============================================================================

#pragma once
#include "Nezumi/Render/View/Projection.hpp"
#include "Nezumi/World/Entity/EntityID.hpp"
#include "Nezumi/World/Physics/TileCoord.hpp"
#include <glm/glm.hpp> // vec2

namespace Nezumi {
    class Entity;
    class Surface;
}

namespace Nezumi::Render {
    class Camera;
}

namespace Nightmourn {

// World position (tiles) of the ground under screen position `screen`
// (window pixels, y down, as the mouse reports it)
glm::vec2 screen_to_world(const Nezumi::Render::Camera&     camera,
                          const Nezumi::Render::Projection& proj,
                          glm::vec2                         screen);

// Tile under screen position `screen`
Nezumi::TileCoord tile_at(const Nezumi::Render::Camera&     camera,
                          const Nezumi::Render::Projection& proj,
                          glm::vec2                         screen);

// Front-most drawable entity with a sprite (unit, character, tree) that
// covers screen position `screen`, or a null handle. With `accept`, only
// entities it accepts count, so e.g. a tree is picked through the hero
// standing in front of it:
//
//   pick_entity(surface, camera, proj, mouse,
//               [](const Nezumi::Entity& e) { return e.is_resource(); });
Nezumi::EntityHandle pick_entity(const Nezumi::Surface&            surface,
                                 const Nezumi::Render::Camera&     camera,
                                 const Nezumi::Render::Projection& proj,
                                 glm::vec2                         screen,
                                 bool (*accept)(const Nezumi::Entity&) = nullptr);

} // namespace Nightmourn
