// =============================================================================
// Settlement/SettlementConfig.hpp
//
// What a settlement founded by a banner gets: a "settlement" config in data/
// (Data/Configs.hpp), named by the banner's building config. Distances are
// in tiles.
// =============================================================================

#pragma once
#include <cstdint>     // int32_t, uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct SettlementConfig {
    std::string_view name = {};

    // Buildings go within this many tiles of the banner's tile along each
    // axis: a (2 * build_radius + 1) square centered on the banner
    std::int32_t build_radius = 16;

    // Enemies never spawn within this many tiles outside the build area
    std::int32_t spawn_margin = 8;

    // A new banner can't go within this many tiles of another settlement's
    // build area (0: they may touch, but never overlap)
    std::int32_t min_gap = 4;

    // Slots in the settlement's inventory
    std::uint16_t inventory_slots = 40; // 10 x 4, as the player's
};

} // namespace Nightmourn
