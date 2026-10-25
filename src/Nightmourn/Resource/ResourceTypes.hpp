// =============================================================================
// Resource/ResourceTypes.hpp
//
// Nightmourn's kinds of Nezumi::Resource, as the ids the engine stores:
//
//   ResourceType  -> Entity::subtype_id    what it is: tree, stone, gold
//   TreeVariety   -> Resource::variety_id  which tree: oak, pine
//
// The engine does the rest (mining, OnEntityMined, the item it yields); the
// game only names the ids and writes the ResourceConfigs.
// =============================================================================

#pragma once
#include "Nezumi/World/Entities/Resource.hpp"
#include <cstdint> // uint8_t, uint16_t

namespace Nightmourn {

enum class ResourceType : std::uint16_t {
    Tree,
    Stone,
    Gold,
    BerryBush,
};

enum class TreeVariety : std::uint8_t {
    Oak,
    Pine,

    Count
};

constexpr Nezumi::EntitySubtypeID to_subtype(ResourceType type) {
    return Nezumi::EntitySubtypeID{ static_cast<std::uint16_t>(type) };
}

constexpr Nezumi::ResourceVarietyID to_variety(TreeVariety variety) {
    return Nezumi::ResourceVarietyID{ static_cast<std::uint8_t>(variety) };
}

constexpr bool is_tree(const Nezumi::Resource& resource) {
    return resource.subtype_id == to_subtype(ResourceType::Tree);
}

} // namespace Nightmourn
