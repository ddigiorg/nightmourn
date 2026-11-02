// =============================================================================
// Lighting/LightDrawer.hpp
//
// Draws LightSources: each light as an engine PointLight into View::Lights,
// sized and placed in world tiles and projected like the ground (a 2:1
// ellipse in iso), and its glow, if any, as a small unlit PointLight into
// View::Emissive, upright above the light like a figure's head.
//
// Lights out of view are skipped, by their radius rather than a sprite box.
// Flicker is two octaves of value noise on intensity and a little on radius,
// seeded per light so torches never pulse together.
//
//   LightDrawer lights;
//   lights.draw(ctx, sources, engine.clock().get_real_time());
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Lighting/LightSource.hpp"
#include <span> // span

namespace Nightmourn {

class LightDrawer {
public:
    // `seconds`: any steadily increasing time, for flicker. `strength`
    // scales every light's pool (not its glow): by day the sun drowns a
    // torch out, so games pass DayNight::light_strength().
    void draw(DrawContext& ctx, std::span<const LightSource> lights, float seconds,
              float strength = 1.0f) const;
    void draw(DrawContext& ctx, const LightSource& light, float seconds,
              float strength = 1.0f) const;
};

} // namespace Nightmourn
