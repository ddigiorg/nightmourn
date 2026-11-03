// =============================================================================
// Item/ItemID.hpp
//
// What an item is (wood, stone, a sword, ...): the key inventories and
// storage will count items by. Items are "item" configs in data/, and their
// ids are their place among them (Data/Configs.hpp), so two items can never
// share one. Item/Items.hpp names the base ones (Items::Wood, ...).
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
