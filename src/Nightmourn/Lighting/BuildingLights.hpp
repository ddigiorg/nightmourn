// =============================================================================
// Lighting/BuildingLights.hpp
//
// The lights buildings give off: every building whose config names a
// "light" (a torch) lights the ground around it, from its footprint's
// center. The game gathers them each frame next to its other lights:
//
//   lights.clear();
//   append_building_lights(world, surface, ctx.world_view, configs, lights);
//   light_drawer.draw(ctx, lights, seconds, day_night.light_strength());
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Lighting/LightSource.hpp"
#include <vector> // vector

namespace Nightmourn {

class Configs;
class Surface;

// Appends the light of every building on `surface` whose light can reach
// `area` (world tiles, e.g. DrawContext::world_view)
void append_building_lights(const Surface& surface, const Area2D& area, const Configs& configs,
                            std::vector<LightSource>& out);

} // namespace Nightmourn
