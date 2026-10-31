// =============================================================================
// Character/CharacterConfig.hpp
//
// Shared, data-driven stats for one kind of character (the hero, a villager,
// ...). Characters keep a pointer to their config, so a config must outlive
// every character built from it (static storage or a game-owned registry).
//
// Distances are in tiles and speeds in tiles per second, like UnitConfig.
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Area2D.hpp"
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct CharacterConfig {
    std::string_view name = {};

    // Health
    uint16_t max_health = 100;

    // Movement
    float move_speed = 4.0f; // tiles per second

    // Mining: a swing every `mine_interval` ticks at most, taking
    // `mine_amount` from a resource whose collision box is within
    // `mine_reach` tiles of the character's feet. The hit lands at the start
    // of the swing for now (no wind-up until there are swing frames).
    uint16_t mine_interval = 30; // 0.5 s at 60 UPS
    uint16_t mine_amount   = 1;
    float    mine_reach    = 2.0f;

    // Collision box, relative to the character's position (its feet)
    Area2D collision_bounds = { -0.25f, -0.25f, 0.25f, 0.25f };

    // TODO: mana, attributes, level, etc. as the design settles
};

} // namespace Nightmourn
