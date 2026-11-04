// =============================================================================
// Draw/SettlementDrawer.hpp
//
// Draws settlements' territory on the ground:
//
//   - the build area, outlined in the owner's color, always
//   - the build area filled with that color, faintly, while the player is
//     placing a building (`fill`), so it's clear where it may go
//   - the spawn area (build area + spawn_margin), dashed red, with
//     show-settlement-areas (or `show_spawn_area`)
//
// And the build area a banner would found, before it's placed, so the
// player sees the base they're laying out.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Geometry/TileRect.hpp"
#include <cstdint>     // uint8_t
#include <glm/glm.hpp> // vec4

namespace Nightmourn {

class Settlement;
class Surface;

class BuildingDrawer;

struct SettlementDrawStyle {
    const BuildingDrawer* filler = nullptr; // fills the area (fill_ground); none: outline only
    std::uint8_t          layer  = 1;       // DrawLayer::GroundOverlay; EntityOverlay draws over the night
    glm::vec4 color           = { 0.45f, 0.65f, 1.0f, 1.0f }; // owner color
    float     outline_pixels  = 2.0f;
    float     fill_alpha      = 0.12f; // while placing
    bool      fill            = false;
    bool      show_spawn_area = false;
};

// Every settlement on `surface`
void draw_settlements(DrawContext& ctx, const Surface& surface, const SettlementDrawStyle& style);

// One build area (e.g. the one a banner under the cursor would found)
void draw_build_area(DrawContext& ctx, const TileRect& area, const SettlementDrawStyle& style);

} // namespace Nightmourn
