// =============================================================================
// ResourceConfig.hpp
//
// Shared, data-driven stats for one kind of resource (an oak tree, a gold
// mine, ...). Resources keep a pointer to their config, so a config must
// outlive every resource built from it (keep them in static storage or a
// registry owned by the game).
//
// A resource is told apart from coarse to fine:
//
//   type_id     Resource::TYPE_ID   the C++ type
//   subtype_id  `subtype`           what it is: tree, gold, stone (game ids)
//   variety_id  `variety`           which one: oak, pine (game ids)
//
// Distances are in tiles.
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include <cstdint>     // uint8_t, uint16_t
#include <string_view> // string_view

namespace Nightmourn {

struct ResourceVarietyTag;

// Which one of a subtype (oak or pine tree), mostly for looks. 1 byte.
using ResourceVarietyID = Nezumi::ID<ResourceVarietyTag, std::uint8_t>;

struct ResourceConfig {
    std::string_view  name    = {};
    EntitySubtypeID   subtype = {}; // copied to Entity::subtype_id
    ResourceVarietyID variety = {}; // copied to Resource::variety_id

    // What mining yields, and how much a full resource holds
    ItemID   item       = {};
    uint16_t max_amount = 100;

    // Collision box, relative to the resource's position (its foot). Usually
    // smaller than its sprite (a tree's trunk), so figures can pass between
    // two of them once collision response exists.
    Area2D collision_bounds = { -0.4f, -0.4f, 0.4f, 0.4f };
};

} // namespace Nightmourn
