// =============================================================================
// Resource/ResourceIDs.hpp
//
// How Nightmourn's resources are told apart, from coarse to fine:
//
//   type_id     EntityTypes::Resource  the C++ type (Resource)
//   subtype_id  ResourceSubtypes::Tree what it is: tree, stone mine, bush
//   variety     TreeVarieties::Pine    which one: oak or pine
//
// The subtype lives on Nezumi::Entity, so engine queries and events
// (OnEntityMined) carry it. The variety is the game's own (on Resource),
// mostly for looks.
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include "Nezumi/World/Entity/EntityID.hpp"
#include <cstdint> // uint8_t

namespace Nightmourn {

struct ResourceVarietyTag;
using ResourceVarietyID = Nezumi::ID<ResourceVarietyTag, std::uint8_t>;

namespace ResourceSubtypes {
    inline constexpr Nezumi::EntitySubtypeID Tree{ 0 };
    // Later: StoneMine, GoldMine, BerryBush, ...
}

// Varieties of ResourceSubtypes::Tree
namespace TreeVarieties {
    inline constexpr ResourceVarietyID Oak { 0 };
    inline constexpr ResourceVarietyID Pine{ 1 };

    inline constexpr std::uint8_t COUNT = 2;
}

} // namespace Nightmourn
