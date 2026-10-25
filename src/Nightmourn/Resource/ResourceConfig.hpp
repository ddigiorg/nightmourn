// =============================================================================
// Resource/ResourceConfig.hpp
//
// Shared, data-driven stats for one kind of resource (oak tree, pine tree,
// stone mine, ...). Resources keep a pointer to their config, so a config
// must outlive every resource built from it (static storage or a game-owned
// registry).
//
// Distances are in tiles, like CharacterConfig.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityID.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nightmourn/Resource/ResourceIDs.hpp"
#include "Nightmourn/Resource/ResourceKind.hpp"
#include <cstdint>     // uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct ResourceConfig {
    std::string_view name = {};

    // What it is (ResourceSubtypes) and which one (e.g. TreeVarieties)
    Nezumi::EntitySubtypeID subtype = ResourceSubtypes::Tree;
    ResourceVarietyID       variety = {};

    // What it holds, and how much when full (AoE2's trees hold 100 wood)
    ResourceKind kind       = ResourceKind::Wood;
    uint16_t     max_amount = 100;

    // Collision box, relative to the resource's position (its foot). A
    // tree's is its trunk, smaller than its sprite, so figures can pass
    // between two trees once collision response exists.
    Nezumi::Area2D collision_bounds = { -0.4f, -0.4f, 0.4f, 0.4f };
};

} // namespace Nightmourn
