// =============================================================================
// Resource/TreeConfig.hpp
//
// Shared, data-driven stats for one kind of tree (oak, pine, ...). Trees keep
// a pointer to their config, so a config must outlive every tree built from
// it (static storage or a game-owned registry).
//
// Distances are in tiles, like CharacterConfig.
// =============================================================================

#pragma once
#include "Nezumi/World/Physics/Area2D.hpp"
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct TreeConfig {
    std::string_view name = {};

    // Wood a full tree holds (AoE2's trees hold 100)
    uint16_t max_wood = 100;

    // Collision box around the trunk, relative to the tree's position (its
    // foot). Smaller than the tile, so figures can squeeze between two trees
    // once collision response exists.
    Nezumi::Area2D collision_bounds = { -0.3f, -0.3f, 0.3f, 0.3f };
};

} // namespace Nightmourn
