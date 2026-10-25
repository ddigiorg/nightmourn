// =============================================================================
// Item/ItemID.hpp
//
// What an item is (wood, stone, a sword, ...): the key a future storage
// system counts items by. Resources name the item mining them yields (see
// Resource/ResourceKind.hpp).
//
// Only the raw materials exist so far. Ids are assigned here, in one place,
// so two items can never share one.
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct ItemTag;
using ItemID = Nezumi::ID<ItemTag, std::uint16_t>;

namespace Items {
    inline constexpr ItemID Wood { 0 };
    inline constexpr ItemID Stone{ 1 };
    inline constexpr ItemID Gold { 2 };
    inline constexpr ItemID Food { 3 };
}

} // namespace Nightmourn
