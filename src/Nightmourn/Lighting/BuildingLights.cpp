// =============================================================================
// Lighting/BuildingLights.cpp
// =============================================================================

#include "Nightmourn/Lighting/BuildingLights.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/Chunk/ChunkSpan.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/Surface/Surface.hpp"

namespace Nightmourn {

// How far past the area to look for lights shining into it, tiles
static constexpr float LIGHT_REACH = 12.0f;

void append_building_lights(const Surface& surface, const Area2D& area, const Configs& configs,
                            std::vector<LightSource>& out)
{
    const Area2D wide = Area2D(area.min - LIGHT_REACH, area.max + LIGHT_REACH);
    surface.each_chunk_in(ChunkSpan::span_from_box(wide), [&](const Chunk& chunk) {
        chunk.visit_entities([&](const Entity& entity) {
            if (!entity.is_alive() || !entity.is_type<Building>()) return true;
            const BuildingConfig& config = entity.as_type<Building>()->config();
            if (config.index >= configs.buildings.size()) return true;

            const std::string& name = configs.buildings[config.index].light;
            if (name.empty()) return true;
            const LightPrototype* light = configs.find_light(name);
            if (!light) return true;

            LightSource l = light->light;
            l.position = entity.position.to_vec2();
            l.seed     = entity.handle.index();
            out.push_back(l);
            return true;
        });
    });
}

} // namespace Nightmourn
