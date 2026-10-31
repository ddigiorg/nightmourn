// =============================================================================
// Input/Picking.hpp
//
// What is under the mouse. The world camera looks at view space (projected,
// see Projection), so a screen position goes screen -> view
// (Camera2D::screen_to_world) -> world (Projection::to_world):
//
//   const TileCoord tile = tile_at(camera, proj, mouse);
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
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Geometry/Projection.hpp"
#include <glm/glm.hpp> // vec2

namespace Nezumi::Render {
    class Camera2D;
}

namespace Nightmourn {

class Entity;
class Surface;

// World position (tiles) of the ground under screen position `screen`
// (window pixels, y down, as the mouse reports it)
glm::vec2 screen_to_world(const Nezumi::Render::Camera2D&   camera,
                          const Projection&         proj,
                          glm::vec2                         screen);

// Tile under screen position `screen`
TileCoord tile_at(const Nezumi::Render::Camera2D&   camera,
                          const Projection&         proj,
                          glm::vec2                         screen);

// Front-most drawable entity with a sprite (unit, character, tree) that
// covers screen position `screen`, or a null handle. With `accept`, only
// entities it accepts count, so e.g. a tree is picked through the hero
// standing in front of it:
//
//   pick_entity(surface, camera, proj, mouse,
//               [](const Entity& e) { return e.is_resource(); });
EntityHandle pick_entity(const Surface&            surface,
                                 const Nezumi::Render::Camera2D&   camera,
                                 const Projection&         proj,
                                 glm::vec2                         screen,
                                 bool (*accept)(const Entity&) = nullptr);

} // namespace Nightmourn
