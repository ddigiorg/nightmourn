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
// Figures are upright, so the ground point under the cursor is behind a
// figure's feet, not on them. pick_figure() looks for feet in the strip of
// ground a sprite can reach up from, tests each one's drawn sprite box
// (figure_view_box, the one the drawers use), and returns the front-most hit:
// the one drawn on top.
// =============================================================================

#pragma once
#include "Nezumi/Render/View/Projection.hpp"
#include "Nezumi/World/Entity/EntityID.hpp"
#include "Nezumi/World/Physics/TileCoord.hpp"
#include <glm/glm.hpp> // vec2

namespace Nezumi {
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

// Front-most drawable figure (unit or character) whose sprite covers screen
// position `screen`, or a null handle
Nezumi::EntityHandle pick_figure(const Nezumi::Surface&            surface,
                                 const Nezumi::Render::Camera&     camera,
                                 const Nezumi::Render::Projection& proj,
                                 glm::vec2                         screen);

} // namespace Nightmourn
