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
#include "Nezumi/World/Physics/Area2D.hpp"
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct CharacterConfig {
    std::string_view name = {};

    // Health
    uint16_t max_health = 100;

    // Movement
    float move_speed = 4.0f; // tiles per second

    // Collision box, relative to the character's position (its feet)
    Nezumi::Area2D collision_bounds = { -0.25f, -0.25f, 0.25f, 0.25f };

    // TODO: mana, attributes, level, etc. as the design settles
};

} // namespace Nightmourn
