// =============================================================================
// Draw/DrawLayers.hpp
//
// Painter's order for the world view, in one place so drawers can't disagree
// about what goes on top of what. Lower layers draw first.
//
// Within a layer, leave DrawParams::order at 0 unless the order matters (it
// is what lets the renderer batch). Entity sprites are the exception: they
// sort by the view-space y of their feet, so figures further down the screen
// overlap the ones behind them (in iso: lower x + y draws on top).
// =============================================================================

#pragma once
#include "Nezumi/Render/2D/DrawParams.hpp"
#include <cstdint> // int32_t, uint8_t

namespace Nightmourn {

namespace DrawLayer {
    // World view
    inline constexpr std::uint8_t Ground        = 0; // tiles
    inline constexpr std::uint8_t GroundOverlay = 1; // tile grid, chunk borders, group routes and slots
    inline constexpr std::uint8_t Entities      = 2; // unit and character sprites, sorted by y
    inline constexpr std::uint8_t EntityOverlay = 3; // healthbars
    inline constexpr std::uint8_t Debug         = 4; // collision boxes, arrows, target lines

    // Screen view
    inline constexpr std::uint8_t Hud = 0;
}

// DrawParams for a world layer
constexpr Nezumi::Render::DrawParams world_layer(std::uint8_t layer) {
    return { .view = Nezumi::Render::View::World, .layer = layer };
}

// Sort order for an entity sprite whose feet are at view-space `y`: lower on
// screen draws later (on top). 1/16 view unit steps; the order's +/- 2^23
// range covers +/- 524k view units.
constexpr std::int32_t y_sort_order(float y) {
    return -static_cast<std::int32_t>(y * 16.0f);
}

} // namespace Nightmourn
