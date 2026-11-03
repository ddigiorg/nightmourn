// =============================================================================
// Resource/ResourceTypes.hpp
//
// Nightmourn's kinds of Resource, as the ids a Resource stores:
//
//   ResourceType  -> Entity::subtype_id    what it is: tree, stone, gold
//
// Which tree (Resource::variety_id: oak, pine) is data: a tree config's
// place among the trees in data/ (Data/Configs.hpp). World does the rest
// (mining, OnEntityMined, the item it yields); this file only names the ids,
// and the ResourceConfigs say what each one is.
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

constexpr EntitySubtypeID to_subtype(ResourceType type) {
    return EntitySubtypeID{ static_cast<std::uint16_t>(type) };
}

constexpr bool is_tree(const Resource& resource) {
    return resource.subtype_id == to_subtype(ResourceType::Tree);
}

} // namespace Nightmourn
