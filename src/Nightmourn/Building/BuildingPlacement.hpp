// =============================================================================
// Building/BuildingPlacement.hpp
//
// Whether a building can go somewhere, and if not why: what
// World::check_placement and World::place_building answer. The placement
// preview colors itself by it and shows the reason.
// =============================================================================

#pragma once
#include <cstdint>     // uint8_t
#include <string_view> // string_view

namespace Nightmourn {

enum class PlacementResult : std::uint8_t {
    Ok,
    NoSettlement,     // only a banner can go up before the faction has a settlement
    HasSettlement,    // a banner, but the faction already has a settlement
    TooCloseToOther,  // a banner whose build area would come too close to another's
    OutsideBuildArea, // not wholly inside the faction's build area
    Occupied,         // another building stands on one of its tiles
    Blocked,          // something else in the way (a tree, a fallen settlement's walls)
};

constexpr bool is_ok(PlacementResult r) { return r == PlacementResult::Ok; }

// Short reason for the player, lowercase ("outside the build area")
constexpr std::string_view placement_message(PlacementResult r) {
    switch (r) {
        case PlacementResult::Ok:               return "ok";
        case PlacementResult::NoSettlement:     return "place a banner first";
        case PlacementResult::HasSettlement:    return "you already have a settlement";
        case PlacementResult::TooCloseToOther:  return "too close to another settlement";
        case PlacementResult::OutsideBuildArea: return "outside the build area";
        case PlacementResult::Occupied:         return "a building is in the way";
        case PlacementResult::Blocked:          return "something is in the way";
    }
    return "?";
}

} // namespace Nightmourn
