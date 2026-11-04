// =============================================================================
// Building/BuildingConfig.hpp
//
// Shared, data-driven stats for one kind of building (a banner, a wooden
// wall, a house, a workbench, ...): a "building" config in data/ (see
// Data/Configs.hpp). Buildings keep a pointer to their config, so a config
// must outlive every building made from it.
//
// A building is told apart from coarse to fine, like a Resource:
//
//   type_id     Building::TYPE_ID    the C++ type
//   subtype_id  `type` (BuildingType) what it does: banner, wall, house, crafter
//   config      `name`                which one: "wall", later "stone-wall"
//
// Footprints are whole tiles and snap to the tile grid. Distances in tiles.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <cstdint>     // uint8_t, uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct SettlementConfig;

// What a building does. Stored as Entity::subtype_id.
enum class BuildingType : std::uint16_t {
    Banner,  // founds a settlement; its build area is centered on it
    Wall,    // joins up with the walls next to it
    House,   // (later) housing for settlers
    Crafter, // (later) turns settlement items into other items each tick
};

constexpr EntitySubtypeID to_subtype(BuildingType type) {
    return EntitySubtypeID{ static_cast<std::uint16_t>(type) };
}

struct BuildingConfig {
    std::string_view name    = {};
    BuildingType     type    = BuildingType::House;
    std::uint16_t    index   = 0; // place among the building configs (load order)

    // Tiles it covers, x by y (each at least 1)
    TileCoord footprint = { 1, 1 };

    std::uint16_t max_health = 100;

    // The item placing it uses up, and deconstructing it gives back (none:
    // it can't be placed from an inventory)
    ItemID item = {};

    // Walls: joins up with the buildings next to it that also connect
    bool connects = false;

    // Banners: the settlement it founds (its build area, inventory, ...)
    const SettlementConfig* settlement = nullptr;
};

} // namespace Nightmourn
