// =============================================================================
// Settlement/RaidConfig.hpp
//
// Night raids on settlements: a "raid" config in data/ (Data/Configs.hpp).
// While it is dark enough, a RaidSpawner sends one `unit` every `interval`
// seconds at each settlement, up to `max_alive` at a time, from a random
// spot `min_distance` to `max_distance` tiles outside the settlement's
// spawn area. Raiders attack-move to the banner, fighting whatever enemy
// they meet on the way (walls included).
// =============================================================================

#pragma once
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct UnitConfig;

struct RaidConfig {
    std::string_view  name = {};
    const UnitConfig* unit = nullptr; // what spawns (Configs links it)

    float         interval     = 30.0f; // seconds between spawns, per settlement
    std::uint16_t max_alive    = 3;     // raiders alive at once, per settlement
    float         min_distance = 2.0f;  // tiles outside the spawn area
    float         max_distance = 8.0f;
    float         min_darkness = 0.5f;  // raids only while darkness is at least this (0 day .. 1 night)
};

} // namespace Nightmourn
