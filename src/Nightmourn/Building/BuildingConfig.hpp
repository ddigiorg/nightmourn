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
#include "Nightmourn/Crafting/Recipe.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <cstdint>     // uint8_t, uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct SettlementConfig;

// What a building does. Stored as Entity::subtype_id. How a building looks
// and lights up is data (its config), not its type.
enum class BuildingType : std::uint16_t {
    Banner,  // founds a settlement; its build area is centered on it
    Wall,    // joins up with the walls next to it
    House,   // (later) housing for settlers
    Crafter, // crafts: a workshop, later a furnace, a shrine (its "crafting")
    Torch,   // lights the ground around it (its config's "light")
    Barracks,// (later) trains units from the settlement's stock
    Gate,    // a wall its own faction walks through (Building::lets_through)
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

    // Blocks movement over its footprint (false: a torch figures walk past)
    bool solid = true;

    // Banners: the settlement it founds (its build area, inventory, ...)
    const SettlementConfig* settlement = nullptr;

    // Crafting ("crafting" in its config): the recipe categories it makes
    // (as a settlement machine, later), how fast (a time multiplier), and
    // whether standing in its settlement lets the player craft them too
    CraftCategoryMask crafting_categories = 0;
    float             crafting_speed      = 1.0f;
    bool              unlocks_for_player  = false;

    // The categories it lets the player craft while they're in its settlement
    CraftCategoryMask player_categories() const { return unlocks_for_player ? crafting_categories : 0; }
};

} // namespace Nightmourn
