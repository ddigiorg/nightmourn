// =============================================================================
// Item/ItemID.hpp
//
// What an item is (wood, stone, a sword, ...): the key inventories and
// storage will count items by. The ids are assigned in one place,
// Item/Items.hpp, so two items can never share one:
//
//   namespace Items {
//       inline constexpr ItemID Wood{ 0 };
//       inline constexpr ItemID Gold{ 1 };
//   }
//
// A null ItemID means "no item" (e.g. a resource that yields nothing).
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct ItemTag;

using ItemID = Nezumi::ID<ItemTag, std::uint16_t>;

} // namespace Nightmourn
