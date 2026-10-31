// =============================================================================
// Resource/ResourceTypes.hpp
//
// Nightmourn's kinds of Resource, as the ids a Resource stores:
//
//   ResourceType  -> Entity::subtype_id    what it is: tree, stone, gold
//   TreeVariety   -> Resource::variety_id  which tree: oak, pine
//
// World does the rest (mining, OnEntityMined, the item it yields); this file
// only names the ids, and the ResourceConfigs say what each one is.
// =============================================================================

#pragma once
#include "Nightmourn/Resource/Resource.hpp"
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

constexpr EntitySubtypeID to_subtype(ResourceType type) {
    return EntitySubtypeID{ static_cast<std::uint16_t>(type) };
}

constexpr ResourceVarietyID to_variety(TreeVariety variety) {
    return ResourceVarietyID{ static_cast<std::uint8_t>(variety) };
}

constexpr bool is_tree(const Resource& resource) {
    return resource.subtype_id == to_subtype(ResourceType::Tree);
}

} // namespace Nightmourn
